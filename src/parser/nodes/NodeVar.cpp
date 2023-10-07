/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "../../include/parser/nodes/NodeVar.hpp"
#include "../../include/parser/ast.hpp"
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

NodeVar::NodeVar(std::string name, Node* value, bool isExtern, bool isConst, bool isGlobal, std::vector<DeclarMod> mods, long loc, Type* type, bool isVolatile) {
    this->name = name;
    this->origName = name;
    this->value = value;
    this->isExtern = isExtern;
    this->isConst = isConst;
    this->isGlobal = isGlobal;
    this->mods = std::vector<DeclarMod>(mods);
    this->loc = loc;
    this->type = type;
    this->isVolatile = isVolatile;
}

NodeVar::NodeVar(std::string name, Node* value, bool isExtern, bool isConst, bool isGlobal, std::vector<DeclarMod> mods, long loc, Type* type, bool isVolatile, bool isChanged, bool noZeroInit) {
    this->name = name;
    this->origName = name;
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
    if(instanceof<TypeAlias>(this->type)) {
        AST::aliasTable[this->name] = this->value;
        return nullptr;
    }
    if(instanceof<TypeBuiltin>(this->type)) {
        /*NodeBuiltin nb = new NodeBuiltin(tb.name,tb.args.dup,loc,tb.block);
        nb.generate();
        this.t = nb.ty;*/
    }
    if(instanceof<TypeAuto>(this->type) && value == nullptr) {
        generator->error("using 'auto' without an explicit value is prohibited!",loc);
        return nullptr;
    }

    if(currScope == nullptr) {
        bool noMangling = false;
        std::string linkName = this->name;

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
        }
        if(!instanceof<TypeAuto>(this->type)) {
            if(instanceof<NodeInt>(this->value)) ((NodeInt*)this->value)->isVarVal = this->type;
            generator->globals[this->name] = LLVMAddGlobal(
                generator->lModule,
                generator->genType(this->type, loc),
                ((linkName == this->name && !noMangling) ? generator->mangle(name,false,false).c_str() : linkName.c_str())
            );
            if(this->value != nullptr) LLVMSetInitializer(generator->globals[this->name], this->value->generate());
        }
        else {
            LLVMValueRef val = nullptr;
            if(instanceof<NodeUnary>(this->value)) val = ((NodeUnary*)this->value)->generateConst();
            else val = this->value->generate();
            generator->globals[this->name] = LLVMAddGlobal(
                generator->lModule,
                LLVMTypeOf(val),
                ((linkName == this->name && !noMangling) ? generator->mangle(name,false,false).c_str() : linkName.c_str())
            );
            this->type = lTypeToType(LLVMTypeOf(val));
            LLVMSetInitializer(generator->globals[this->name],val);
            LLVMSetAlignment(generator->globals[this->name], generator->getAlignment(this->type));
            if(isVolatile) LLVMSetVolatile(generator->globals[name],true);
            return nullptr;
        }

        if(isExtern) LLVMSetLinkage(generator->globals[this->name], LLVMExternalLinkage);
        if(isVolatile) LLVMSetVolatile(generator->globals[this->name],true);

        LLVMSetAlignment(generator->globals[this->name], generator->getAlignment(this->type));
        if(value != nullptr && !isExtern) {
            LLVMValueRef val;
            if(instanceof<NodeUnary>(this->value)) val = ((NodeUnary*)this->value)->generateConst();
            else val = this->value->generate();
            LLVMSetInitializer(generator->globals[this->name], val);
        }
        else if(!isExtern) LLVMSetInitializer(generator->globals[this->name], LLVMConstNull(generator->genType(this->type,loc)));
        return nullptr;
    }
    else {
        for(int i=0; i<this->mods.size(); i++) {
            if(this->mods[i].name == "volatile") isVolatile = true;
            else if(this->mods[i].name == "nozeroinit") noZeroInit = true;
        }
        currScope->localVars[this->name] = this;

        if(instanceof<TypeAuto>(this->type)) {
            LLVMValueRef val = this->value->generate();
            currScope->localScope[this->name] = LLVMBuildAlloca(generator->builder,LLVMTypeOf(val),name.c_str());
            this->type = lTypeToType(LLVMTypeOf(val));
            LLVMSetAlignment(currScope->localScope[this->name], generator->getAlignment(this->type));
            LLVMBuildStore(generator->builder,val,currScope->localScope[this->name]);
            return currScope->localScope[this->name];
        }
        if(instanceof<NodeInt>(this->value)) ((NodeInt*)this->value)->isVarVal = this->type;

        LLVMTypeRef gT = generator->genType(this->type,this->loc);
        currScope->localScope[this->name] = LLVMBuildAlloca(generator->builder, gT, name.c_str());
        if(isVolatile) LLVMSetVolatile(currScope->localScope[this->name],true);
        LLVMSetAlignment(currScope->getWithoutLoad(this->name, this->loc), generator->getAlignment(this->type));

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