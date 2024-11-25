/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "../../include/utils.hpp"
#include "../../include/parser/nodes/NodeUnary.hpp"
#include "../../include/parser/nodes/NodeBool.hpp"
#include "../../include/parser/nodes/NodeArray.hpp"
#include "../../include/parser/nodes/NodeGet.hpp"
#include "../../include/parser/nodes/NodeIndex.hpp"
#include "../../include/parser/nodes/NodeIden.hpp"
#include "../../include/parser/nodes/NodeCall.hpp"
#include "../../include/parser/nodes/NodeBinary.hpp"
#include "../../include/parser/nodes/NodeStruct.hpp"
#include "../../include/parser/nodes/NodeFunc.hpp"
#include "../../include/parser/nodes/NodeVar.hpp"
#include "../../include/parser/nodes/NodeInt.hpp"
#include "../../include/parser/nodes/NodeFloat.hpp"
#include "../../include/parser/nodes/NodeBuiltin.hpp"
#include "../../include/parser/ast.hpp"
#include "../../include/llvm.hpp"

NodeUnary::NodeUnary(int loc, char type, Node* base) {
    this->loc = loc;
    this->type = type;
    this->base = base;
}

Type* NodeUnary::getType() {
    switch(this->type) {
        case TokType::GetPtr:
            if(instanceof<TypeArray>(this->base->getType())) return new TypePointer(((TypeArray*)this->base->getType())->element);
            return new TypePointer(this->base->getType());
        case TokType::Minus: case TokType::Ne: return this->base->getType();
        case TokType::Destructor: return new TypeVoid();
        case TokType::Multiply:
            Type* ty = this->base->getType();
            if(instanceof<TypePointer>(ty)) return ((TypePointer*)ty)->instance;
            if(instanceof<TypeArray>(ty)) return ((TypeArray*)ty)->element;
            return nullptr;
    }
    return nullptr;
}

RaveValue NodeUnary::generatePtr() {
    if(instanceof<NodeIden>(this->base)) ((NodeIden*)this->base)->isMustBePtr = true;
    if(instanceof<NodeIndex>(this->base)) ((NodeIndex*)this->base)->isMustBePtr = true;
    if(instanceof<NodeGet>(this->base)) ((NodeGet*)this->base)->isMustBePtr = true;
    return this->base->generate();
}

void NodeUnary::check() {
    bool oldCheck = this->isChecked;
    this->isChecked = true;
    if(!oldCheck) this->base->check();
}

RaveValue NodeUnary::generateConst() {
    if(this->type == TokType::Minus) {
        RaveValue bs = this->base->generate();
        if(instanceof<TypeBasic>(bs.type) && !((TypeBasic*)bs.type)->isFloat()) return {LLVMBuildNeg(generator->builder, bs.value, ""), bs.type};
        return {LLVMBuildFNeg(generator->builder, bs.value, ""), bs.type};
    }
    return {};
}

RaveValue NodeUnary::generate() {
    if(this->type == TokType::Minus) {
        RaveValue bs = this->base->generate();
        if(instanceof<TypeBasic>(bs.type) && !((TypeBasic*)bs.type)->isFloat()) return {LLVMBuildNeg(generator->builder, bs.value, "NodeUnary_neg"), bs.type};
        return {LLVMBuildFNeg(generator->builder, bs.value, "NodeUnary_fneg"), bs.type};
    }
    if(this->type == TokType::GetPtr) {
        RaveValue val;

        if(instanceof<NodeGet>(this->base)) {
            ((NodeGet*)this->base)->isMustBePtr = true;
            val = this->base->generate();
        }
        else if(instanceof<NodeCall>(this->base)) {
            NodeCall* call = (NodeCall*)this->base;
            RaveValue gcall = call->generate();
            auto temp = LLVM::alloc(gcall.type, "NodeUnary_temp");
            LLVMBuildStore(generator->builder, gcall.value, temp.value);
            val = temp;
        }
        else if(instanceof<NodeBinary>(this->base)) val = generator->byIndex(this->base->generate(), std::vector<LLVMValueRef>({LLVMConstInt(LLVMInt32TypeInContext(generator->context), 0, false)}));
        else if(instanceof<NodeIden>(this->base)) {
            NodeIden* id = ((NodeIden*)this->base);
            if(currScope == nullptr) {
                id->isMustBePtr = true;
                return id->generate();
            }
            else {
                if(currScope->localVars.find(id->name) != currScope->localVars.end()) currScope->localVars[id->name]->isUsed = true;

                if(instanceof<TypeArray>(currScope->getVar(id->name, this->loc)->type)) {
                    val = LLVM::gep(currScope->getWithoutLoad(id->name),
                        std::vector<LLVMValueRef>({LLVMConstInt(LLVMInt32Type(), 0, false), LLVMConstInt(LLVMInt32Type(), 0, false)}).data(),
                    2, "NodeUnary_gep");
                }
                else val = currScope->getWithoutLoad(id->name);
            }
        }
        else if(instanceof<NodeIndex>(this->base)) {
            ((NodeIndex*)this->base)->isMustBePtr = true;
            val = this->base->generate();
        }
        else if(instanceof<NodeArray>(this->base)) {
            val = this->base->generate();
            RaveValue temp = LLVM::alloc(val.type, "NodeUnary_NA_temp");
            LLVMBuildStore(generator->builder, val.value, temp.value);
            val = temp;
        }
        return val;
    }
    if(this->type == TokType::Ne) {
        if(instanceof<NodeIden>(base)) ((NodeIden*)base)->isMustBePtr = false;
        else if(instanceof<NodeIndex>(base)) ((NodeIndex*)base)->isMustBePtr = false;
        else if(instanceof<NodeGet>(base)) ((NodeGet*)base)->isMustBePtr = false;
    
        RaveValue toRet = base->generate();
        if(instanceof<TypePointer>(toRet.type)) toRet = LLVM::load(toRet, "NodeUnary_Ne_load", loc);
    
        return {LLVMBuildNot(generator->builder, toRet.value, "NodeUnary_not"), toRet.type};
    }
    if(this->type == TokType::Multiply) {
        RaveValue _base = this->base->generate();
        return LLVM::load(_base, "NodeUnary_multiply_load", loc);
    }
    if(this->type == TokType::Destructor) {
        RaveValue val2 = this->generatePtr();

        if(!instanceof<TypePointer>(val2.type)
        && !instanceof<TypeStruct>(val2.type)) generator->error("the attempt to call the destructor without structure!", this->loc);

        if(instanceof<TypePointer>(val2.type)) {
            if(instanceof<TypePointer>(val2.type->getElType()) && instanceof<TypePointer>(val2.type->getElType()->getElType())) {
                val2 = LLVM::load(val2, "NodeCall_destructor_load", loc);
            }
        }
        
        if(!instanceof<TypePointer>(val2.type)) {
            RaveValue temp = LLVM::alloc(val2.type, "NodeUnary_temp");
            LLVMBuildStore(generator->builder, val2.value, temp.value);
            val2 = temp;
        }

        if(!instanceof<TypeStruct>(val2.type->getElType())) generator->error("the attempt to call the destructor is not in the structure!", this->loc);

        std::string struc = ((TypeStruct*)val2.type->getElType())->toString();
        if(AST::structTable[struc]->destructor == nullptr) {
            if(instanceof<NodeIden>(this->base)) {
                NodeIden* id = (NodeIden*)this->base;
                if(currScope->localVars.find(id->name) == currScope->localVars.end() && !instanceof<TypeStruct>(currScope->localVars[id->name]->type)) {
                    return (new NodeCall(this->loc, new NodeIden("std::free", this->loc), {this->base}))->generate();
                }
                return {nullptr, nullptr};
            }
            return (new NodeCall(this->loc, new NodeIden("std::free", this->loc), {base}))->generate();
        }
        if(instanceof<NodeIden>(this->base)) {
            if(currScope->has(((NodeIden*)this->base)->name)) {
                currScope->getVar(((NodeIden*)this->base)->name, this->loc)->isAllocated = false;
            }
        }
        return (new NodeCall(this->loc, new NodeIden(AST::structTable[struc]->destructor->name, this->loc), {this->base}))->generate();
    }
    generator->error("NodeUnary undefined operator!", loc);
    return {};
}

Node* NodeUnary::comptime() {
    Node* comptimed = this->base->comptime();
    switch(this->type) {
        case TokType::Minus:
            if(instanceof<NodeInt>(comptimed)) return new NodeInt(-((NodeInt*)comptimed)->value);
            else if(instanceof<NodeFloat>(comptimed)) return new NodeFloat(-((NodeFloat*)comptimed)->value);
            generator->error("NodeUnary::comptime() cannot work with this value!", this->loc);
            return nullptr;
        case TokType::Ne:
            if(instanceof<NodeBool>(comptimed)) return new NodeBool(!(((NodeBool*)comptimed))->value);
            generator->error("NodeUnary::comptime() cannot work with this value!", this->loc);
            return nullptr;
        default: break;
    }
    return this;
}

Node* NodeUnary::copy() {return new NodeUnary(this->loc, this->type, this->base->copy());}