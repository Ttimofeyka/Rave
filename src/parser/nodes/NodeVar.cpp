/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "../../include/parser/nodes/NodeVar.hpp"
#include "../../include/parser/ast.hpp"
#include <llvm-c/Comdat.h>
#include "../../include/parser/nodes/NodeArray.hpp"
#include "../../include/parser/nodes/NodeString.hpp"
#include "../../include/parser/nodes/NodeInt.hpp"
#include "../../include/parser/nodes/NodeFloat.hpp"
#include "../../include/parser/nodes/NodeUnary.hpp"
#include "../../include/parser/nodes/NodeBinary.hpp"
#include "../../include/parser/nodes/NodeStruct.hpp"
#include "../../include/parser/nodes/NodeIden.hpp"
#include "../../include/parser/nodes/NodeFunc.hpp"
#include "../../include/parser/nodes/NodeCall.hpp"
#include "../../include/parser/nodes/NodeGet.hpp"
#include "../../include/parser/nodes/NodeNull.hpp"
#include "../../include/lexer/lexer.hpp"
#include "../../include/parser/parser.hpp"
#include "../../include/llvm.hpp"

#if LLVM_VERSION > 18
#define LLVMDIBuilderInsertDeclareAtEnd LLVMDIBuilderInsertDeclareRecordAtEnd
#endif

NodeVar::NodeVar(std::string name, Node* value, bool isExtern, bool isConst, bool isGlobal, std::vector<DeclarMod> mods, int loc, Type* type, bool isVolatile, bool isChanged, bool noZeroInit) {
    this->name = name;
    this->origName = name;
    this->linkName = this->name;
    this->value = value;
    this->isExtern = isExtern;
    this->isConst = isConst;
    this->isGlobal = isGlobal;
    this->mods = std::vector<DeclarMod>(mods);
    this->loc = loc;
    this->type = type;
    this->isVolatile = isVolatile;
    this->isChanged = isChanged;
    this->noZeroInit = noZeroInit;
    this->isNoCopy = false;

    for (size_t i=0; i<mods.size(); i++) {
        if (mods[i].name == "noCopy") { isNoCopy = true; break; }
    }

    if (value != nullptr && instanceof<TypeArray>(type) && ((TypeArray*)type)->count == nullptr) {
        ((TypeArray*)this->type)->count = new NodeInt(((NodeArray*)this->value)->values.size());
    }
}

NodeVar::~NodeVar() {
    if (value != nullptr) delete value;
    if (type != nullptr && !instanceof<TypeBasic>(type) && !instanceof<TypeVoid>(type)) delete type;
}

Type* NodeVar::getType() { return this->type->copy(); }

Node* NodeVar::comptime() { return this; }

Node* NodeVar::copy() {
    return new NodeVar(
        name, (value == nullptr) ? nullptr : value->copy(), isExtern, isConst, isGlobal,
        std::vector<DeclarMod>(mods), loc, type->copy(), isVolatile, isChanged, noZeroInit
    );
}

void NodeVar::check() {
    if (isChecked) return;
    isChecked = true;

    if (namespacesNames.size() > 0) name = namespacesToString(namespacesNames, name);
    if (isGlobal) AST::varTable[this->name] = this;

    Types::replaceTemplates(&type);

    if (instanceof<TypeBasic>(type) && !AST::aliasTypes.empty()) {
        Type* _type = type->check(nullptr);
        if (_type != nullptr) type = _type;
    }
}

void Predefines::handle(Type* type, Node* node, int loc) {
    if (AST::structTable[((TypeStruct*)type)->name]->predefines.size() > 0) {
        NodeGet* getter = new NodeGet(node, "", true, loc);

        for (auto& predefine : AST::structTable[((TypeStruct*)type)->name]->predefines) {
            getter->field = predefine.first;

            if (predefine.second.isStruct) Predefines::handle(getter->getType(), getter, loc);
            else if (predefine.second.value != nullptr) Binary::operation(TokType::Equ, getter, predefine.second.value, loc);
        }
    }
}

void NodeVar::prepareType() {
    while (AST::aliasTypes.find(type->toString()) != AST::aliasTypes.end()) 
        type = AST::aliasTypes[type->toString()];

    Types::replaceTemplates(&type);
    Types::replaceComptime(type);

    if (instanceof<TypeVoid>(type)) generator->error("using \033[1mvoid\033[22m as a variable type is prohibited!", loc);
}

void NodeVar::processGlobalModifiers(int& alignment, bool& noMangling) {
    for (size_t i=0; i<mods.size(); i++) {
        while (AST::aliasTable.find(mods[i].name) != AST::aliasTable.end()) {
            if (instanceof<NodeArray>(AST::aliasTable[mods[i].name])) {
                mods[i].name = ((NodeString*)((NodeArray*)AST::aliasTable[mods[i].name])->values[0])->value;
                mods[i].value = ((NodeArray*)AST::aliasTable[mods[i].name])->values[1];
            }
            else mods[i].name = ((NodeString*)AST::aliasTable[mods[i].name])->value;
        }
        if (mods[i].name == "C") noMangling = true;
        else if (mods[i].name == "volatile") isVolatile = true;
        else if (mods[i].name == "linkname") {
            Node* newLinkName = mods[i].value->comptime();
            if (!instanceof<NodeString>(newLinkName)) generator->error("value type of \033[1mlinkname\033[22m must be a string!", loc);
            linkName = ((NodeString*)newLinkName)->value;
        }
        else if (mods[i].name == "noOperators") isNoOperators = true;
        else if (mods[i].name == "align") {
            Node* newAlignment = mods[i].value->comptime();
            if (!instanceof<NodeInt>(newAlignment)) generator->error("value type of \033[1malign\033[22m must be an integer!", loc);
            alignment = ((NodeInt*)(mods[i].value))->value.to_int();
        }
        else if (mods[i].name == "nozeroinit") noZeroInit = true;
    }
}

void NodeVar::processLocalModifiers() {
    for (size_t i=0; i<mods.size(); i++) {
        if (mods[i].name == "volatile") isVolatile = true;
        else if (mods[i].name == "noOperators") isNoOperators = true;
        else if (mods[i].name == "nozeroinit") noZeroInit = true;
    }
}

void NodeVar::applyAlignment(LLVMValueRef llvmValue, int alignment) {
    if (alignment != -1) LLVMSetAlignment(llvmValue, alignment);
    else if (!instanceof<TypeVector>(type)) LLVMSetAlignment(llvmValue, generator->getAlignment(type));
}

void NodeVar::generateDebugInfo(LLVMValueRef llvmValue) {
    if (!generator->settings.outDebugInfo) return;

    auto debugLoc = LLVMDIBuilderCreateDebugLocation(generator->context, loc, 0, AST::funcTable[currScope->funcName]->diFuncScope, nullptr);
    auto varType = debugInfo->genType(type, loc);
    LLVMMetadataRef emptyExpr = LLVMDIBuilderCreateExpression(debugInfo->diBuilder, nullptr, 0);

    auto varInfo = LLVMDIBuilderCreateAutoVariable(
        debugInfo->diBuilder, AST::funcTable[currScope->funcName]->diFuncScope, name.c_str(), name.length(),
        debugInfo->diFile, loc, varType, true,
        LLVMDIFlagZero, 0
    );

    LLVMDIBuilderInsertDeclareAtEnd(
        debugInfo->diBuilder, llvmValue,
        varInfo, emptyExpr, debugLoc, generator->currBB
    );
}

void NodeVar::createLLVMGlobal(int alignment, bool noMangling) {
    LLVMTypeRef globalType = generator->genType(type, loc);

    if (generator->globals.find(name) != generator->globals.end() && 
        LLVMGetLinkage(generator->globals[name].value) == LLVMExternalLinkage) {
        return;
    }

    linkName = ((linkName == name && !noMangling) ? generator->mangle(name, false, false) : linkName);

    generator->globals[name] = { LLVMAddGlobal(
        generator->lModule,
        globalType,
        linkName.c_str()
    ), (instanceof<TypeConst>(type) ? new TypePointer(((TypeConst*)type)->instance) : new TypePointer(type)) };
}

void NodeVar::handleGlobalInitialization(int alignment) {
    if (!isExtern) {
        RaveValue val = (value == nullptr) ? 
            RaveValue{LLVMConstNull(generator->genType(type, loc)), type} : 
            (instanceof<NodeUnary>(value) ? ((NodeUnary*)value)->generateConst() : value->generate());
        
        LLVMSetInitializer(generator->globals[name].value, val.value);
    }

    if (isComdat) {
        LLVMComdatRef comdat = LLVMGetOrInsertComdat(generator->lModule, linkName.c_str());
        LLVMSetComdatSelectionKind(comdat, LLVMAnyComdatSelectionKind);
        LLVMSetComdat(generator->globals[name].value, comdat);
        LLVMSetLinkage(generator->globals[name].value, LLVMLinkOnceODRLinkage);
    }
    else if (isExtern) LLVMSetLinkage(generator->globals[name].value, LLVMExternalLinkage);

    if (isVolatile) LLVMSetVolatile(generator->globals[name].value, true);
    applyAlignment(generator->globals[name].value, alignment);
}

RaveValue NodeVar::generateAutoTypeGlobal() {
    RaveValue value = instanceof<NodeUnary>(this->value) ? ((NodeUnary*)this->value)->generateConst() : this->value->generate();

    linkName = generator->mangle(name, false, false);
    generator->globals[name] = {LLVMAddGlobal(
        generator->lModule,
        LLVMTypeOf(value.value),
        linkName.c_str()
    ), new TypePointer(value.type)};

    type = this->value->getType();
    LLVMSetInitializer(generator->globals[this->name].value, value.value);

    return {};
}

RaveValue NodeVar::generateAutoTypeLocal() {
    RaveValue val;
    bool isStructConstructor = false;

    if (instanceof<NodeCall>(value) && instanceof<NodeIden>(((NodeCall*)value)->func)) {
        NodeIden* niden = (NodeIden*)((NodeCall*)value)->func;
        if (niden->name.find('<') != std::string::npos && AST::structTable.find(niden->name.substr(0, niden->name.find('<'))) != AST::structTable.end()) {
            isStructConstructor = true;

            if (AST::structTable.find(niden->name) != AST::structTable.end()) type = new TypeStruct(niden->name);
            else {
                std::string sTypes = niden->name.substr(niden->name.find('<')+  1, niden->name.find('>'));
                sTypes.pop_back();

                Lexer lexer = Lexer(sTypes, 0);
                Parser parser = Parser(lexer.tokens, "(BUILTIN)");
                std::vector<Type*> types;

                while (true) {
                    if (parser.peek()->type == TokType::Comma) parser.next();
                    else if (parser.peek()->type == TokType::Eof) break;
                    types.push_back(parser.parseType());
                }

                AST::structTable[niden->name.substr(0, niden->name.find('<'))]->genWithTemplate("<" + sTypes + ">", types);
                type = new TypeStruct(niden->name);
            }
        }
    }
    
    if (!isStructConstructor) {
        val = value->generate();
        currScope->localScope[name] = LLVM::alloc(val.type, name.c_str());
        type = value->getType();

        if (isVolatile) LLVMSetVolatile(currScope->localScope[name].value, true);

        generateDebugInfo(currScope->localScope[name].value);
        applyAlignment(currScope->localScope[name].value, -1);

        LLVMBuildStore(generator->builder, val.value, currScope->localScope[name].value);
        return currScope->localScope[name];
    }

    return {};
}

RaveValue NodeVar::generateGlobalVariable() {
    prepareType();

    if (instanceof<TypeAlias>(type)) { AST::aliasTable[name] = value; return {}; }

    if (instanceof<TypeAuto>(type) && !value) {
        generator->error("using \033[1mauto\033[22m without an explicit value is prohibited!", loc);
        return {};
    }

    if (type) checkForTemplated(type);

    isUsed = true;
    bool noMangling = false;
    int alignment = -1;
    linkName = name;

    processGlobalModifiers(alignment, noMangling);

    if (instanceof<TypeAuto>(type)) return generateAutoTypeGlobal();

    if (instanceof<NodeInt>(value)) ((NodeInt*)value)->isVarVal = type;

    createLLVMGlobal(alignment, noMangling);

    if (!isExtern) {
        if (value) LLVMSetInitializer(generator->globals[name].value, value->generate().value);
        else if (!noZeroInit) LLVMSetInitializer(generator->globals[name].value, LLVMConstNull(generator->genType(type, loc)));

        LLVMSetGlobalConstant(generator->globals[name].value, isConst);
    }
    else LLVMSetLinkage(generator->globals[name].value, LLVMExternalLinkage);

    handleGlobalInitialization(alignment);
    return {};
}

RaveValue NodeVar::generateLocalVariable() {
    currScope->localVars[name] = this;
    processLocalModifiers();

    prepareType();

    if (instanceof<TypeAlias>(type)) { currScope->aliasTable[name] = value; return {}; }

    if (instanceof<TypeAuto>(type)) return generateAutoTypeLocal();

    if (instanceof<NodeInt>(value) && !isFloatType(type)) ((NodeInt*)value)->isVarVal = type;

    LLVMTypeRef gT = generator->genType(type, loc);
    currScope->localScope[name] = LLVM::alloc(type, name.c_str());

    generateDebugInfo(currScope->localScope[name].value);

    if (isVolatile) LLVMSetVolatile(currScope->localScope[name].value, true);
    applyAlignment(currScope->localScope[name].value, -1);

    if (instanceof<TypeStruct>(type)) Predefines::handle(type, new NodeIden(name, loc), loc);

    if (value) Binary::operation(TokType::Equ, new NodeIden(name, loc), value, loc);
    else if ((instanceof<TypeBasic>(type) || instanceof<TypePointer>(type)) && !noZeroInit) 
        LLVMBuildStore(generator->builder, LLVMConstNull(gT), currScope->localScope[name].value);

    return currScope->localScope[name];
}

RaveValue NodeVar::generate() {
    if (!currScope) return generateGlobalVariable();
    else return generateLocalVariable();
}
