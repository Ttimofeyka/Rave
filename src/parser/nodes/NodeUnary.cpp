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

RaveValue Unary::negative(Node* base) {
    if(instanceof<NodeInt>(base)) {
        NodeInt* newValue = (new NodeInt(-((NodeInt*)base)->value));
        newValue->isMustBeChar = ((NodeInt*)base)->isMustBeChar;
        newValue->isMustBeShort = ((NodeInt*)base)->isMustBeShort;
        newValue->isMustBeLong = ((NodeInt*)base)->isMustBeLong;
        return newValue->generate();
    }
    else if(instanceof<NodeFloat>(base)) {
        NodeFloat* old = (NodeFloat*)base;
        NodeFloat* newValue = (NodeFloat*)old->copy();
        newValue->value = (newValue->value[0] == '-' ? "" : "-") + newValue->value;
        return newValue->generate();
    }

    RaveValue bs = base->generate();

    if(instanceof<TypeBasic>(bs.type) && !((TypeBasic*)bs.type)->isFloat())
        return {LLVMBuildNeg(generator->builder, bs.value, "Unary_negative"), bs.type};

    return {LLVMBuildFNeg(generator->builder, bs.value, "Unary_negative"), bs.type};
}

RaveValue Unary::make(int loc, char type, Node* base) {
    if(type == TokType::Minus) return Unary::negative(base);

    if(type == TokType::GetPtr) {
        RaveValue value;

        if(instanceof<NodeGet>(base)) {
            ((NodeGet*)base)->isMustBePtr = true;
            value = base->generate();
        }
        else if(instanceof<NodeCall>(base)) {
            NodeCall* call = (NodeCall*)base;
            RaveValue gcall = call->generate();
            auto temp = LLVM::alloc(gcall.type, "NodeUnary_temp");
            LLVMBuildStore(generator->builder, gcall.value, temp.value);
            value = temp;
        }
        else if(instanceof<NodeBinary>(base)) value = generator->byIndex(base->generate(), {LLVM::makeInt(64, 0, false)});
        else if(instanceof<NodeIden>(base)) {
            NodeIden* id = ((NodeIden*)base);

            if(currScope == nullptr) {
                id->isMustBePtr = true;
                return id->generate();
            }
            else {
                // If we are in the local scope, make this variable marked as used
                if(currScope->localVars.find(id->name) != currScope->localVars.end()) currScope->localVars[id->name]->isUsed = true;

                // If it is an array, generate pointer to the first element
                if(instanceof<TypeArray>(currScope->getVar(id->name, loc)->type)) value = LLVM::gep(currScope->getWithoutLoad(id->name),
                    std::vector<LLVMValueRef>({LLVMConstInt(LLVMInt32Type(), 0, false), LLVMConstInt(LLVMInt32Type(), 0, false)}).data(),
                    2, "NodeUnary_gep");
                else value = currScope->getWithoutLoad(id->name);
            }
        }
        else if(instanceof<NodeIndex>(base)) {
            ((NodeIndex*)base)->isMustBePtr = true;
            value = base->generate();
        }
        else if(instanceof<NodeArray>(base)) {
            value = base->generate();
            LLVM::makeAsPointer(value);
        }

        return value;
    }

    if(type == TokType::Ne) {
        if(instanceof<NodeIden>(base)) ((NodeIden*)base)->isMustBePtr = false;
        else if(instanceof<NodeIndex>(base)) ((NodeIndex*)base)->isMustBePtr = false;
        else if(instanceof<NodeGet>(base)) ((NodeGet*)base)->isMustBePtr = false;
    
        RaveValue toRet = base->generate();

        if(instanceof<TypePointer>(toRet.type)) toRet = LLVM::load(toRet, "NodeUnary_Ne_load", loc);
        if(instanceof<TypeStruct>(toRet.type)) generator->error("cannot use the struct as a boolean type for '!' operator!", loc);
    
        return {LLVMBuildNot(generator->builder, toRet.value, "NodeUnary_not"), toRet.type};
    }

    if(type == TokType::Multiply) return LLVM::load(base->generate(), "NodeUnary_multiply_load", loc); // NOTE: Deprecated

    if(type == TokType::Destructor) {
        if(instanceof<NodeIden>(base)) ((NodeIden*)base)->isMustBePtr = true;
        else if(instanceof<NodeIndex>(base)) ((NodeIndex*)base)->isMustBePtr = true;
        else if(instanceof<NodeGet>(base)) ((NodeGet*)base)->isMustBePtr = true;

        RaveValue value = base->generate();

        if(!instanceof<TypePointer>(value.type) && !instanceof<TypeStruct>(value.type)) generator->error("the attempt to call the destructor without structure!", loc);
        
        if(!instanceof<TypePointer>(value.type)) {
            if(!instanceof<TypeStruct>(value.type)) generator->error("the attempt to call the destructor is not in the structure!", loc);

            LLVM::makeAsPointer(value);
        }
        else if(instanceof<TypePointer>(value.type->getElType()->getElType())) value = LLVM::load(value, "NodeCall_destructor_load", loc);

        if(!instanceof<TypeStruct>(value.type->getElType())) generator->error("the attempt to call the destructor is not in the structure!", loc);

        std::string structure = ((TypeStruct*)value.type->getElType())->toString();
        if(AST::structTable[structure]->destructor == nullptr) return {nullptr, nullptr};

        if(instanceof<NodeIden>(base) && currScope->has(((NodeIden*)base)->name))
            currScope->getVar(((NodeIden*)base)->name, loc)->isAllocated = false;

        return Call::make(loc, new NodeIden(AST::structTable[structure]->destructor->name, loc), {base});
    }

    generator->error("NodeUnary undefined operator!", loc);
    return {};
}

NodeUnary::NodeUnary(int loc, char type, Node* base) {
    this->loc = loc;
    this->type = type;
    this->base = base;
}

NodeUnary::~NodeUnary() {
    if(this->base != nullptr) delete this->base;
}

Type* NodeUnary::getType() {
    switch(this->type) {
        case TokType::GetPtr:
            if(instanceof<TypeArray>(this->base->getType())) return new TypePointer(this->base->getType()->getElType());
            return new TypePointer(this->base->getType());
        case TokType::Minus: case TokType::Ne: return this->base->getType();
        case TokType::Destructor: return typeVoid;
        case TokType::Multiply:
            Type* ty = this->base->getType();

            if(instanceof<TypePointer>(ty) || instanceof<TypeArray>(ty)) return ty->getElType();
            return nullptr;
    }

    return nullptr;
}
void NodeUnary::check() {
    bool oldCheck = this->isChecked;
    this->isChecked = true;

    if(!oldCheck) this->base->check();
}

RaveValue NodeUnary::generateConst() {
    if(this->type == TokType::Minus) return Unary::negative(base);

    return {};
}

RaveValue NodeUnary::generate() {
    return Unary::make(loc, type, base);
}

Node* NodeUnary::comptime() {
    Node* comptimed = this->base->comptime();

    switch(this->type) {
        case TokType::Minus:
            if(instanceof<NodeInt>(comptimed)) return new NodeInt(-((NodeInt*)comptimed)->value);
            else if(instanceof<NodeFloat>(comptimed)) {
                std::string value = ((NodeFloat*)comptimed)->value;
                if(value[0] == '-') return new NodeFloat(value.substr(1));
                return new NodeFloat("-" + value);
            }

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
