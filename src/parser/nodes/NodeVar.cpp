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

NodeVar::NodeVar(std::string name, Node* value, bool isExtern, bool isConst, bool isGlobal, std::vector<DeclarMod> mods, long loc, Type* type, bool isVolatile) {
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
}

NodeVar::NodeVar(std::string name, Node* value, bool isExtern, bool isConst, bool isGlobal, std::vector<DeclarMod> mods, long loc, Type* type, bool isVolatile, bool isChanged, bool noZeroInit) {
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

LLVMValueRef NodeVar::generate() {
    if(instanceof<TypeVoid>(this->type)) {
        // error: variable cannot be void
        generator->error("using 'void' for variables is prohibited!", this->loc);
        return nullptr;
    }

    if(instanceof<TypeAlias>(this->type)) {
        if(currScope != nullptr) currScope->aliasTable[this->name] = this->value;
        else AST::aliasTable[this->name] = this->value;
        return nullptr;
    }

    if(instanceof<TypeAuto>(this->type) && value == nullptr) {
        generator->error("using 'auto' without an explicit value is prohibited!", this->loc);
        return nullptr;
    }

    if(instanceof<TypeStruct>(this->type)) {
        while(instanceof<TypeStruct>(this->type) &&
              generator->toReplace.find(((TypeStruct*)this->type)->name) != generator->toReplace.end()
        ) this->type = generator->toReplace[((TypeStruct*)this->type)->name];
    }

    linkName = this->name;
 
    if(currScope == nullptr) {
        bool noMangling = false;

        for(int i=0; i<this->mods.size(); i++) {
            while(AST::aliasTable.find(this->mods[i].name) != AST::aliasTable.end()) {
                if(instanceof<NodeArray>(AST::aliasTable[this->mods[i].name])) {
                    mods[i].name = ((NodeString*)((NodeArray*)AST::aliasTable[this->mods[i].name])->values[0])->value;
                    mods[i].value = ((NodeString*)((NodeArray*)AST::aliasTable[this->mods[i].name])->values[1])->value;
                }
                else mods[i].name = ((NodeString*)AST::aliasTable[this->mods[i].name])->value;
            }
            if(mods[i].name == "C") noMangling = true;
            else if(mods[i].name == "volatile") isVolatile = true;
            else if(mods[i].name == "linkname") linkName = mods[i].value;
            else if(mods[i].name == "noOperators") isNoOperators = true;
        }
        if(!instanceof<TypeAuto>(this->type)) {
            if(instanceof<NodeInt>(this->value)) ((NodeInt*)this->value)->isVarVal = this->type;
            linkName = ((linkName == this->name && !noMangling) ? generator->mangle(name, false, false) : linkName);
            generator->globals[this->name] = LLVMAddGlobal(
                generator->lModule,
                generator->genType(this->type, loc),
                linkName.c_str()
            );
            if(!this->isExtern && this->value != nullptr) {
                LLVMSetInitializer(generator->globals[this->name], this->value->generate());
                LLVMSetGlobalConstant(generator->globals[this->name], this->isConst);
            }
        }
        else {
            LLVMValueRef val = nullptr;
            if(instanceof<NodeUnary>(this->value)) val = ((NodeUnary*)this->value)->generateConst();
            else val = this->value->generate();
            linkName = ((linkName == this->name && !noMangling) ? generator->mangle(name,false,false) : linkName);
            generator->globals[this->name] = LLVMAddGlobal(
                generator->lModule,
                LLVMTypeOf(val),
                linkName.c_str()
            );
            this->type = lTypeToType(LLVMTypeOf(val));
            LLVMSetInitializer(generator->globals[this->name], val);
            LLVMSetGlobalConstant(generator->globals[this->name], this->isConst);
            if(!instanceof<TypeVector>(this->type)) LLVMSetAlignment(generator->globals[this->name], generator->getAlignment(this->type));
            if(isVolatile) LLVMSetVolatile(generator->globals[name], true);
            return nullptr;
        }

        if(this->isComdat) {
            LLVMComdatRef comdat = LLVMGetOrInsertComdat(generator->lModule, linkName.c_str());
            LLVMSetComdatSelectionKind(comdat, LLVMAnyComdatSelectionKind);
            LLVMSetComdat(generator->globals[this->name], comdat);
            LLVMSetLinkage(generator->globals[this->name], LLVMLinkOnceODRLinkage);
        }
        else if(isExtern) LLVMSetLinkage(generator->globals[this->name], LLVMExternalLinkage);
        if(isVolatile) LLVMSetVolatile(generator->globals[this->name], true);

        if(!instanceof<TypeVector>(this->type)) LLVMSetAlignment(generator->globals[this->name], generator->getAlignment(this->type));

        if(value != nullptr && !isExtern) {
            LLVMValueRef val;
            if(instanceof<NodeUnary>(this->value)) val = ((NodeUnary*)this->value)->generateConst();
            else val = this->value->generate();
            LLVMSetInitializer(generator->globals[this->name], val);
        }
        else if(!isExtern) LLVMSetInitializer(generator->globals[this->name], LLVMConstNull(generator->genType(this->type, this->loc)));
        return nullptr;
    }
    else {
        for(int i=0; i<this->mods.size(); i++) {
            if(this->mods[i].name == "volatile") isVolatile = true;
            else if(this->mods[i].name == "nozeroinit") noZeroInit = true;
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
                        LLVMValueRef val = this->value->generate();
                        currScope->localScope[this->name] = LLVMBuildAlloca(generator->builder, LLVMTypeOf(val), name.c_str());
                        this->type = lTypeToType(LLVMTypeOf(val));
                        if(!instanceof<TypeVector>(this->type)) LLVMSetAlignment(currScope->localScope[this->name], generator->getAlignment(this->type));
                        LLVMBuildStore(generator->builder,val,currScope->localScope[this->name]);
                        return currScope->localScope[this->name];
                    }
                }
                else {
                    LLVMValueRef val = this->value->generate();
                    currScope->localScope[this->name] = LLVMBuildAlloca(generator->builder, LLVMTypeOf(val), name.c_str());
                    this->type = lTypeToType(LLVMTypeOf(val));
                    if(!instanceof<TypeVector>(this->type)) LLVMSetAlignment(currScope->localScope[this->name], generator->getAlignment(this->type));
                    LLVMBuildStore(generator->builder,val,currScope->localScope[this->name]);
                    return currScope->localScope[this->name];
                }
            }
            else {
                LLVMValueRef val = this->value->generate();
                currScope->localScope[this->name] = LLVMBuildAlloca(generator->builder, LLVMTypeOf(val), name.c_str());
                this->type = lTypeToType(LLVMTypeOf(val));
                if(!instanceof<TypeVector>(this->type)) LLVMSetAlignment(currScope->localScope[this->name], generator->getAlignment(this->type));
                LLVMBuildStore(generator->builder,val,currScope->localScope[this->name]);
                return currScope->localScope[this->name];
            }
        }
        if(instanceof<NodeInt>(this->value)) ((NodeInt*)this->value)->isVarVal = this->type;

        LLVMTypeRef gT = generator->genType(this->type, this->loc);
        currScope->localScope[this->name] = LLVMBuildAlloca(generator->builder, gT, name.c_str());
        if(isVolatile) LLVMSetVolatile(currScope->localScope[this->name],true);
        if(!instanceof<TypeVector>(this->type)) LLVMSetAlignment(currScope->getWithoutLoad(this->name, this->loc), generator->getAlignment(this->type));

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
                LLVMBuildStore(generator->builder,LLVMConstArray(LLVMGetElementType(gT),values.data(),values.size()),currScope->localScope[this->name]);
            }
            else if(LLVMGetTypeKind(gT) != LLVMStructTypeKind) LLVMBuildStore(generator->builder,LLVMConstNull(gT), currScope->localScope[this->name]);
        }
        return currScope->localScope[this->name];
    }
    return nullptr;
}