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
        if (mods[i].name == "noCopy") { this->isNoCopy = true; break; }
    }

    if (value != nullptr && instanceof<TypeArray>(type) && ((TypeArray*)type)->count == nullptr) {
        ((TypeArray*)this->type)->count = new NodeInt(((NodeArray*)this->value)->values.size());
    }
}

NodeVar::~NodeVar() {
    if (value != nullptr) delete value;
    if (type != nullptr && !instanceof<TypeBasic>(type) && !instanceof<TypeVoid>(type)) delete type;
}

Type* NodeVar::getType() {return this->type->copy();}
Node* NodeVar::comptime() {return this;}

Node* NodeVar::copy() {
    return new NodeVar(
        this->name, 
        (this->value == nullptr) ? nullptr : this->value->copy(),
        this->isExtern, this->isConst, this->isGlobal, std::vector<DeclarMod>(this->mods), this->loc, this->type->copy(), this->isVolatile, this->isChanged, this->noZeroInit
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

RaveValue NodeVar::generate() {
    while (AST::aliasTypes.find(type->toString()) != AST::aliasTypes.end()) type = AST::aliasTypes[type->toString()];

    Types::replaceTemplates(&type);
    Types::replaceComptime(type);

    if (instanceof<TypeVoid>(type)) {
        // error: variable cannot be void
        generator->error("using \033[1mvoid\033[22m as a variable type is prohibited!", loc);
        return {};
    }

    if (instanceof<TypeAlias>(type)) {
        if (currScope != nullptr) currScope->aliasTable[name] = value;
        else AST::aliasTable[name] = value;

        return {};
    }

    if (instanceof<TypeAuto>(this->type) && value == nullptr) {
        generator->error("using \033[1mauto\033[22m without an explicit value is prohibited!", loc);
        return {};
    }

    if (type != nullptr) checkForTemplated(type);

    linkName = this->name;

    int alignment = -1;
 
    if (currScope == nullptr) {
        this->isUsed = true; // Maybe rework it?

        bool noMangling = false;

        for (size_t i=0; i<this->mods.size(); i++) {
            while (AST::aliasTable.find(this->mods[i].name) != AST::aliasTable.end()) {
                if (instanceof<NodeArray>(AST::aliasTable[this->mods[i].name])) {
                    mods[i].name = ((NodeString*)((NodeArray*)AST::aliasTable[this->mods[i].name])->values[0])->value;
                    mods[i].value = ((NodeArray*)AST::aliasTable[this->mods[i].name])->values[1];
                }
                else mods[i].name = ((NodeString*)AST::aliasTable[this->mods[i].name])->value;
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

        if (!instanceof<TypeAuto>(this->type)) {
            if (instanceof<NodeInt>(this->value)) ((NodeInt*)this->value)->isVarVal = this->type;
            linkName = ((linkName == this->name && !noMangling) ? generator->mangle(name, false, false) : linkName);

            LLVMTypeRef globalType = generator->genType(this->type, loc);

            if (generator->globals.find(this->name) != generator->globals.end() && LLVMGetLinkage(generator->globals[this->name].value) == LLVMExternalLinkage) {
                return {};
            }

            generator->globals[this->name] = {LLVMAddGlobal(
                generator->lModule,
                globalType,
                linkName.c_str()
            ), (instanceof<TypeConst>(type) ? new TypePointer(((TypeConst*)type)->instance) : new TypePointer(this->type))};

            if (!isExtern) {
                if (value != nullptr) LLVMSetInitializer(generator->globals[name].value, this->value->generate().value);
                else if (!noZeroInit) LLVMSetInitializer(generator->globals[name].value, LLVMConstNull(globalType));

                LLVMSetGlobalConstant(generator->globals[this->name].value, this->isConst);
            }
            else LLVMSetLinkage(generator->globals[this->name].value, LLVMExternalLinkage);
        }
        else {
            RaveValue val;
            if (instanceof<NodeUnary>(this->value)) val = ((NodeUnary*)this->value)->generateConst();
            else val = this->value->generate();

            linkName = ((linkName == this->name && !noMangling) ? generator->mangle(name,false,false) : linkName);
            generator->globals[this->name] = {LLVMAddGlobal(
                generator->lModule,
                LLVMTypeOf(val.value),
                linkName.c_str()
            ), new TypePointer(val.type)};

            this->type = value->getType();

            LLVMSetInitializer(generator->globals[this->name].value, val.value);

            if (alignment != -1) LLVMSetAlignment(generator->globals[this->name].value, alignment);
            else if (!instanceof<TypeVector>(this->type)) LLVMSetAlignment(generator->globals[this->name].value, generator->getAlignment(this->type));

            if (isVolatile) LLVMSetVolatile(generator->globals[name].value, true);

            return {};
        }

        if (this->isComdat) {
            LLVMComdatRef comdat = LLVMGetOrInsertComdat(generator->lModule, linkName.c_str());
            LLVMSetComdatSelectionKind(comdat, LLVMAnyComdatSelectionKind);
            LLVMSetComdat(generator->globals[this->name].value, comdat);
            LLVMSetLinkage(generator->globals[this->name].value, LLVMLinkOnceODRLinkage);
        }
        else if (isExtern) LLVMSetLinkage(generator->globals[this->name].value, LLVMExternalLinkage);

        if (isVolatile) LLVMSetVolatile(generator->globals[this->name].value, true);

        if (!isExtern) {
            RaveValue val = (value == nullptr) ? RaveValue{LLVMConstNull(generator->genType(this->type, this->loc)), this->type} : 
                            (instanceof<NodeUnary>(this->value) ? ((NodeUnary*)this->value)->generateConst() : this->value->generate());
            LLVMSetInitializer(generator->globals[this->name].value, val.value);
        }

        if (alignment != -1) LLVMSetAlignment(generator->globals[this->name].value, alignment);
        else if (!instanceof<TypeVector>(this->type)) LLVMSetAlignment(generator->globals[this->name].value, generator->getAlignment(this->type));

        return {};
    }
    else {
        currScope->localVars[this->name] = this;

        for (size_t i=0; i<this->mods.size(); i++) {
            if (mods[i].name == "volatile") isVolatile = true;
            else if (mods[i].name == "noOperators") isNoOperators = true;
            else if (mods[i].name == "nozeroinit") noZeroInit = true;
        }

        if (instanceof<TypeAuto>(this->type)) {
            RaveValue val;
            bool isStructConstructor = false;
            
            if (instanceof<NodeCall>(this->value) && instanceof<NodeIden>(((NodeCall*)this->value)->func)) {
                NodeIden* niden = (NodeIden*)((NodeCall*)this->value)->func;
                if (niden->name.find('<') != std::string::npos && AST::structTable.find(niden->name.substr(0, niden->name.find('<'))) != AST::structTable.end()) {
                    isStructConstructor = true;
                    if (AST::structTable.find(niden->name) != AST::structTable.end()) this->type = new TypeStruct(niden->name);
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
                        this->type = new TypeStruct(niden->name);
                    }
                }
            }
            
            if (!isStructConstructor) {
                val = this->value->generate();
                currScope->localScope[this->name] = LLVM::alloc(val.type, name.c_str());
                this->type = value->getType();

                if (isVolatile) LLVMSetVolatile(currScope->localScope[this->name].value, true);

                if (generator->settings.outDebugInfo) {
                    auto debugLoc = LLVMDIBuilderCreateDebugLocation(generator->context, loc, 0, AST::funcTable[currScope->funcName]->diFuncScope, nullptr);
                    auto varType = debugInfo->genType(type, loc);
                    LLVMMetadataRef emptyExpr = LLVMDIBuilderCreateExpression(debugInfo->diBuilder, nullptr, 0);

                    auto varInfo = LLVMDIBuilderCreateAutoVariable(
                        debugInfo->diBuilder, AST::funcTable[currScope->funcName]->diFuncScope, name.c_str(), name.length(),
                        debugInfo->diFile, loc, varType, true,
                        LLVMDIFlagZero, alignment > 0 ? alignment : 0
                    );

                    LLVMDIBuilderInsertDeclareAtEnd(
                        debugInfo->diBuilder, currScope->localScope[this->name].value,
                        varInfo, emptyExpr, debugLoc, generator->currBB
                    );
                }

                if (alignment != -1) LLVMSetAlignment(currScope->localScope[this->name].value, alignment);
                else if (!instanceof<TypeVector>(this->type)) LLVMSetAlignment(currScope->localScope[this->name].value, generator->getAlignment(this->type));

                LLVMBuildStore(generator->builder, val.value, currScope->localScope[this->name].value);
                return currScope->localScope[this->name];
            }
        }

        if (instanceof<NodeInt>(this->value) && !isFloatType(this->type)) ((NodeInt*)this->value)->isVarVal = this->type;

        LLVMTypeRef gT = generator->genType(this->type, this->loc);
        currScope->localScope[this->name] = LLVM::alloc(this->type, name.c_str());

        if (generator->settings.outDebugInfo) {
            auto debugLoc = LLVMDIBuilderCreateDebugLocation(generator->context, loc, 0, AST::funcTable[currScope->funcName]->diFuncScope, nullptr);
            auto varType = debugInfo->genType(type, loc);
            LLVMMetadataRef emptyExpr = LLVMDIBuilderCreateExpression(debugInfo->diBuilder, nullptr, 0);

            auto varInfo = LLVMDIBuilderCreateAutoVariable(
                debugInfo->diBuilder, AST::funcTable[currScope->funcName]->diFuncScope, name.c_str(), name.length(),
                debugInfo->diFile, loc, varType, true,
                LLVMDIFlagZero, alignment > 0 ? alignment : 0
            );

            LLVMDIBuilderInsertDeclareAtEnd(
                debugInfo->diBuilder, currScope->localScope[this->name].value,
                varInfo, emptyExpr, debugLoc, generator->currBB
            );
        }

        if (isVolatile) LLVMSetVolatile(currScope->localScope[this->name].value, true);
        if (alignment != -1) LLVMSetAlignment(currScope->localScope[this->name].value, alignment);
        else if (!instanceof<TypeVector>(this->type)) LLVMSetAlignment(currScope->localScope[this->name].value, generator->getAlignment(this->type));

        if (instanceof<TypeStruct>(type)) Predefines::handle(type, new NodeIden(name, loc), loc);

        if (this->value != nullptr) Binary::operation(TokType::Equ, new NodeIden(name, loc), value, loc);
        else if ((instanceof<TypeBasic>(type) || instanceof<TypePointer>(type)) && !noZeroInit) LLVMBuildStore(generator->builder, LLVMConstNull(gT), currScope->localScope[this->name].value);

        return currScope->localScope[this->name];
    }
    return {};
}
