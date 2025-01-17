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
#include "../../include/lexer/lexer.hpp"
#include "../../include/parser/parser.hpp"
#include "../../include/llvm.hpp"

NodeVar::NodeVar(std::string name, Node* value, bool isExtern, bool isConst, bool isGlobal, std::vector<DeclarMod> mods, int loc, Type* type, bool isVolatile) {
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
    this->isChanged = false;

    if(value != nullptr && instanceof<TypeArray>(type) && ((TypeArray*)type)->count == 0) {
        // Set the count of array elements to the count of values
        ((TypeArray*)this->type)->count = new NodeInt(((NodeArray*)this->value)->values.size());
    }
}

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

    for(int i=0; i<this->mods.size(); i++) {
        if(this->mods[i].name == "noCopy") this->isNoCopy = true;
    }

    if(value != nullptr && instanceof<TypeArray>(type) && ((TypeArray*)type)->count == nullptr) {
        // Set the count of array elements to the count of values
        ((TypeArray*)this->type)->count = new NodeInt(((NodeArray*)this->value)->values.size());
    }
}

NodeVar::~NodeVar() {
    if(value != nullptr) delete value;
    if(type != nullptr && !instanceof<TypeBasic>(type) && !instanceof<TypeVoid>(type)) delete type;
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
    bool oldCheck = this->isChecked;
    this->isChecked = true;
    if(!oldCheck) {
        if(this->namespacesNames.size() > 0) this->name = namespacesToString(this->namespacesNames, this->name);
        if(this->isGlobal) AST::varTable[this->name] = this;
        if(instanceof<TypeBasic>(this->type) && !AST::aliasTypes.empty()) {
            Type* _type = this->type->check(nullptr);
            if(_type != nullptr) this->type = _type;
        }
        if(instanceof<TypeStruct>(this->type)) {
            TypeStruct* ts = (TypeStruct*)this->type;
            if(ts->types.size() > 0 && generator != nullptr && generator->toReplace.size() > 0) {
                for(int i=0; i<ts->types.size(); i++) {
                    Type* _type = ts->types[i];
                    while(generator->toReplace.find(_type->toString()) != generator->toReplace.end()) _type = generator->toReplace[_type->toString()];
                    _type = _type->copy();
                    ts->types[i] = _type;
                }
            }
        }
    }
}

RaveValue NodeVar::generate() {
    while(AST::aliasTypes.find(this->type->toString()) != AST::aliasTypes.end()) this->type = AST::aliasTypes[this->type->toString()];

    if(instanceof<TypeVoid>(this->type)) {
        // error: variable cannot be void
        generator->error("using 'void' in variable '" + this->name + "' is prohibited!", this->loc);
        return {};
    }

    if(instanceof<TypeAlias>(this->type)) {
        if(currScope != nullptr) currScope->aliasTable[this->name] = this->value;
        else AST::aliasTable[this->name] = this->value;
        return {};
    }

    if(instanceof<TypeAuto>(this->type) && value == nullptr) {
        generator->error("using 'auto' without an explicit value is prohibited!", this->loc);
        return {};
    }

    if(instanceof<TypeStruct>(this->type)) {
        while(instanceof<TypeStruct>(this->type) &&
              generator->toReplace.find(((TypeStruct*)this->type)->name) != generator->toReplace.end()
        ) this->type = generator->toReplace[((TypeStruct*)this->type)->name];
    }

    linkName = this->name;

    int alignment = -1;
 
    if(currScope == nullptr) {
        this->isUsed = true; // Maybe rework it?

        bool noMangling = false;

        for(int i=0; i<this->mods.size(); i++) {
            while(AST::aliasTable.find(this->mods[i].name) != AST::aliasTable.end()) {
                if(instanceof<NodeArray>(AST::aliasTable[this->mods[i].name])) {
                    mods[i].name = ((NodeString*)((NodeArray*)AST::aliasTable[this->mods[i].name])->values[0])->value;
                    mods[i].value = ((NodeArray*)AST::aliasTable[this->mods[i].name])->values[1];
                }
                else mods[i].name = ((NodeString*)AST::aliasTable[this->mods[i].name])->value;
            }
            if(mods[i].name == "C") noMangling = true;
            else if(mods[i].name == "volatile") isVolatile = true;
            else if(mods[i].name == "linkname") {
                Node* newLinkName = mods[i].value->comptime();
                if(!instanceof<NodeString>(newLinkName)) generator->error("value type of 'linkname' must be a string!", loc);
                linkName = ((NodeString*)newLinkName)->value;
            }
            else if(mods[i].name == "noOperators") isNoOperators = true;
            else if(mods[i].name == "align") {
                Node* newAlignment = mods[i].value->comptime();
                if(!instanceof<NodeInt>(newAlignment)) generator->error("value type of 'align' must be an integer!", loc);
                alignment = ((NodeInt*)(mods[i].value))->value.to_int();
            }
        }

        if(!instanceof<TypeAuto>(this->type)) {
            if(instanceof<NodeInt>(this->value)) ((NodeInt*)this->value)->isVarVal = this->type;
            linkName = ((linkName == this->name && !noMangling) ? generator->mangle(name, false, false) : linkName);

            generator->globals[this->name] = {LLVMAddGlobal(
                generator->lModule,
                generator->genType(this->type, loc),
                linkName.c_str()
            ), new TypePointer(this->type)};

            if(!this->isExtern && this->value != nullptr) {
                LLVMSetInitializer(generator->globals[this->name].value, this->value->generate().value);
                LLVMSetGlobalConstant(generator->globals[this->name].value, this->isConst);
                if(alignment != -1) LLVMSetAlignment(generator->globals[this->name].value, alignment);
            }
        }
        else {
            RaveValue val;
            if(instanceof<NodeUnary>(this->value)) val = ((NodeUnary*)this->value)->generateConst();
            else val = this->value->generate();

            linkName = ((linkName == this->name && !noMangling) ? generator->mangle(name,false,false) : linkName);
            generator->globals[this->name] = {LLVMAddGlobal(
                generator->lModule,
                LLVMTypeOf(val.value),
                linkName.c_str()
            ), new TypePointer(val.type)};

            this->type = value->getType();
            LLVMSetInitializer(generator->globals[this->name].value, val.value);
            LLVMSetGlobalConstant(generator->globals[this->name].value, this->isConst);
            if(alignment != -1) LLVMSetAlignment(generator->globals[this->name].value, alignment);
            else if(!instanceof<TypeVector>(this->type)) LLVMSetAlignment(generator->globals[this->name].value, generator->getAlignment(this->type));
            if(isVolatile) LLVMSetVolatile(generator->globals[name].value, true);
            return {};
        }

        if(this->isComdat) {
            LLVMComdatRef comdat = LLVMGetOrInsertComdat(generator->lModule, linkName.c_str());
            LLVMSetComdatSelectionKind(comdat, LLVMAnyComdatSelectionKind);
            LLVMSetComdat(generator->globals[this->name].value, comdat);
            LLVMSetLinkage(generator->globals[this->name].value, LLVMLinkOnceODRLinkage);
        }
        else if(isExtern) LLVMSetLinkage(generator->globals[this->name].value, LLVMExternalLinkage);
        if(isVolatile) LLVMSetVolatile(generator->globals[this->name].value, true);

        if(alignment != -1) LLVMSetAlignment(generator->globals[this->name].value, alignment);
        else if(!instanceof<TypeVector>(this->type)) LLVMSetAlignment(generator->globals[this->name].value, generator->getAlignment(this->type));

        if(value != nullptr && !isExtern) {
            RaveValue val;
            if(instanceof<NodeUnary>(this->value)) val = ((NodeUnary*)this->value)->generateConst();
            else val = this->value->generate();
            LLVMSetInitializer(generator->globals[this->name].value, val.value);
        }
        else if(!isExtern) LLVMSetInitializer(generator->globals[this->name].value, LLVMConstNull(generator->genType(this->type, this->loc)));
        return {};
    }
    else {
        for(int i=0; i<this->mods.size(); i++) {
            if(mods[i].name == "volatile") isVolatile = true;
            else if(mods[i].name == "nozeroinit") noZeroInit = true;
            else if(mods[i].name == "noOperators") isNoOperators = true;
        }

        currScope->localVars[this->name] = this;

        if(instanceof<TypeAuto>(this->type)) {
            if(instanceof<NodeCall>(this->value)) {
                // Constructor?
                NodeCall* ncall = (NodeCall*)this->value;
                if(instanceof<NodeIden>(ncall->func)) {
                    NodeIden* niden = (NodeIden*)ncall->func;
                    if(niden->name.find('<') != std::string::npos && AST::structTable.find(niden->name.substr(0, niden->name.find('<'))) != AST::structTable.end()) {
                        // Excellent!
                        if(AST::structTable.find(niden->name) != AST::structTable.end()) this->type = new TypeStruct(niden->name);
                        else {
                            // Generate structure with template
                            std::string sTypes = niden->name.substr(niden->name.find('<')+1, niden->name.find('>'));
                            sTypes.pop_back();

                            Lexer* lexer = new Lexer(sTypes, 0);
                            Parser* parser = new Parser(lexer->tokens, "(BUILTIN)");
                            std::vector<Type*> types;

                            while(true) {
                                if(parser->peek()->type == TokType::Comma) parser->next();
                                else if(parser->peek()->type == TokType::Eof) break;
                                types.push_back(parser->parseType());
                            }
                            AST::structTable[niden->name.substr(0, niden->name.find('<'))]->genWithTemplate("<"+sTypes+">", types);
                            this->type = new TypeStruct(niden->name);
                        }
                    }
                    else {
                        RaveValue val = this->value->generate();
                        currScope->localScope[this->name] = LLVM::alloc(val.type, name.c_str());
                        this->type = value->getType();
                        if(isVolatile) LLVMSetVolatile(currScope->localScope[this->name].value, true);
                        if(alignment != -1) LLVMSetAlignment(generator->globals[this->name].value, alignment);
                        else if(!instanceof<TypeVector>(this->type)) LLVMSetAlignment(currScope->localScope[this->name].value, generator->getAlignment(this->type));
                        LLVMBuildStore(generator->builder, val.value, currScope->localScope[this->name].value);
                        return currScope->localScope[this->name];
                    }
                }
                else {
                    RaveValue val = this->value->generate();
                    currScope->localScope[this->name] = LLVM::alloc(val.type, name.c_str());
                    this->type = value->getType();
                    if(isVolatile) LLVMSetVolatile(currScope->localScope[this->name].value, true);
                    if(alignment != -1) LLVMSetAlignment(generator->globals[this->name].value, alignment);
                    else if(!instanceof<TypeVector>(this->type)) LLVMSetAlignment(currScope->localScope[this->name].value, generator->getAlignment(this->type));
                    LLVMBuildStore(generator->builder, val.value, currScope->localScope[this->name].value);
                    return currScope->localScope[this->name];
                }
            }
            else {
                RaveValue val = this->value->generate();
                currScope->localScope[this->name] = LLVM::alloc(val.type, name.c_str());
                this->type = value->getType();
                if(isVolatile) LLVMSetVolatile(currScope->localScope[this->name].value, true);
                if(alignment != -1) LLVMSetAlignment(generator->globals[this->name].value, alignment);
                else if(!instanceof<TypeVector>(this->type)) LLVMSetAlignment(currScope->localScope[this->name].value, generator->getAlignment(this->type));
                LLVMBuildStore(generator->builder, val.value, currScope->localScope[this->name].value);
                return currScope->localScope[this->name];
            }
        }
        if(instanceof<NodeInt>(this->value)) ((NodeInt*)this->value)->isVarVal = this->type;

        LLVMTypeRef gT = generator->genType(this->type, this->loc);
        currScope->localScope[this->name] = LLVM::alloc(this->type, name.c_str());

        if(isVolatile) LLVMSetVolatile(currScope->localScope[this->name].value, true);
        if(alignment != -1) LLVMSetAlignment(generator->globals[this->name].value, alignment);
        else if(!instanceof<TypeVector>(this->type)) LLVMSetAlignment(currScope->getWithoutLoad(this->name, this->loc).value, generator->getAlignment(this->type));

        if((this->value == nullptr || instanceof<NodeCall>(this->value)) && instanceof<TypeStruct>(this->type)) {
            if(AST::structTable[((TypeStruct*)this->type)->name]->predefines.size() > 0) {
                (new NodeCall(this->loc, new NodeIden(((TypeStruct*)this->type)->name, this->loc), std::vector<Node*>({new NodeIden(this->name, this->loc)})))->generate();
            }
        }
        if(this->value != nullptr) (new NodeBinary(TokType::Equ, new NodeIden(this->name, this->loc), this->value, this->loc))->generate();
        else if(!noZeroInit) {
            if(LLVMGetTypeKind(gT) == LLVMArrayTypeKind) {
                std::vector<LLVMValueRef> values;
                for(int i=0; i<LLVMGetArrayLength(gT); i++) values.push_back(LLVMConstNull(LLVMGetElementType(gT)));
                LLVMBuildStore(generator->builder, LLVMConstArray(LLVMGetElementType(gT), values.data(), values.size()), currScope->localScope[this->name].value);
            }
            else if(LLVMGetTypeKind(gT) != LLVMStructTypeKind) LLVMBuildStore(generator->builder, LLVMConstNull(gT), currScope->localScope[this->name].value);
        }
        return currScope->localScope[this->name];
    }
    return {};
}