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

NodeUnary::NodeUnary(long loc, char type, Node* base) {
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

LLVMValueRef NodeUnary::generatePtr() {
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

LLVMValueRef NodeUnary::generateConst() {
    if(this->type == TokType::Minus) {
        LLVMValueRef bs = this->base->generate();
        #if LLVM_VERSION_MAJOR >= 15
            return (LLVMGetTypeKind(LLVMTypeOf(bs)) == LLVMIntegerTypeKind) ? LLVMBuildNeg(generator->builder, bs, "") : LLVMBuildFNeg(generator->builder, bs, "");
        #else
            return (LLVMGetTypeKind(LLVMTypeOf(bs)) == LLVMIntegerTypeKind) ? LLVMConstNeg(bs) : LLVMConstFNeg(bs);
        #endif
    }
    return nullptr;
}

LLVMValueRef NodeUnary::generate() {
    if(this->type == TokType::Minus) {
        LLVMValueRef bs = this->base->generate();
        return (LLVMGetTypeKind(LLVMTypeOf(bs)) == LLVMIntegerTypeKind) ? LLVMBuildNeg(generator->builder, bs, "NodeUnary_neg") : LLVMBuildFNeg(generator->builder, bs, "NodeUnary_fneg");
    }
    if(this->type == TokType::GetPtr) {
        LLVMValueRef val;
        if(instanceof<NodeGet>(this->base)) {
            ((NodeGet*)this->base)->isMustBePtr = true;
            val = this->base->generate();
        }
        else if(instanceof<NodeCall>(this->base)) {
            NodeCall* call = (NodeCall*)this->base;
            auto gcall = call->generate();
            auto temp = LLVMBuildAlloca(generator->builder, LLVMTypeOf(gcall), "NodeUnary_temp");
            LLVMBuildStore(generator->builder, gcall, temp);
            val = temp;
        }
        else if(instanceof<NodeBinary>(this->base)) val = generator->byIndex(this->base->generate(), std::vector<LLVMValueRef>({LLVMConstInt(LLVMInt32TypeInContext(generator->context), 0, false)}));
        else if(instanceof<NodeIden>(this->base)) {
            NodeIden* id = ((NodeIden*)this->base);
            if(currScope == nullptr) {
                id->isMustBePtr = true;
                return id->generate();
            }
            else if(instanceof<TypeArray>(currScope->getVar(id->name, this->loc)->type)) {
                val = LLVM::inboundsGep(currScope->getWithoutLoad(id->name),
                    std::vector<LLVMValueRef>({LLVMConstInt(LLVMInt32Type(), 0, false), LLVMConstInt(LLVMInt32Type(), 0, false)}).data(),
                2, "NodeUnary_ingep");
            }
            else val = currScope->getWithoutLoad(id->name);
        }
        else if(instanceof<NodeIndex>(this->base)) {
            ((NodeIndex*)this->base)->isMustBePtr = true;
            val = this->base->generate();
        }
        else if(instanceof<NodeArray>(this->base)) {
            val = this->base->generate();
            LLVMValueRef temp = LLVMBuildAlloca(generator->builder, LLVMTypeOf(val), "NodeUnary_NA_temp");
            LLVMBuildStore(generator->builder, val, temp);
            val = temp;
        }
        if(LLVMGetTypeKind(LLVMTypeOf(val)) == LLVMPointerTypeKind && LLVMGetTypeKind(LLVMGetElementType(LLVMTypeOf(val))) == LLVMArrayTypeKind) {
            return LLVMBuildPointerCast(
                generator->builder,
                val,
                LLVMPointerType(LLVMGetElementType(LLVMGetElementType(LLVMTypeOf(val))),0),
                "NodeUnary_bitcast"
            );
        }
        return val;
    }
    if(this->type == TokType::Ne) return LLVMBuildNot(generator->builder, this->base->generate(), "NodeUnary_not");
    if(this->type == TokType::Multiply) {
        LLVMValueRef _base = this->base->generate();
        return LLVM::load(_base, "NodeUnary_multiply_load");
    }
    if(this->type == TokType::Destructor) {
        LLVMValueRef val2 = this->generatePtr();
        if(LLVMGetTypeKind(LLVMTypeOf(val2)) != LLVMPointerTypeKind
        && LLVMGetTypeKind(LLVMTypeOf(val2)) != LLVMStructTypeKind) generator->error("the attempt to call the destructor is not in the structure!", this->loc); 
        if(LLVMGetTypeKind(LLVMTypeOf(val2)) == LLVMPointerTypeKind) {
            if(LLVMGetTypeKind(LLVMGetElementType(LLVMTypeOf(val2))) == LLVMPointerTypeKind) {
                if(LLVMGetTypeKind(LLVMGetElementType(LLVMGetElementType(LLVMTypeOf(val2)))) == LLVMStructTypeKind) val2 = LLVM::load(val2, "NodeCall_destructor_load");
            }
        }
        if(LLVMGetTypeKind(LLVMTypeOf(val2)) != LLVMPointerTypeKind) {
            LLVMValueRef temp = LLVMBuildAlloca(generator->builder, LLVMTypeOf(val2), "NodeUnary_temp");
            LLVMBuildStore(generator->builder, val2, temp);
            val2 = temp;
        }
        std::string struc = std::string(LLVMGetStructName(LLVMGetElementType(LLVMTypeOf(val2))));
        if(AST::structTable[struc]->destructor == nullptr) {
            if(instanceof<NodeIden>(this->base)) {
                NodeIden* id = (NodeIden*)this->base;
                if(currScope->localVars.find(id->name) == currScope->localVars.end() && !instanceof<TypeStruct>(currScope->localVars[id->name]->type)) {
                    return (new NodeCall(this->loc, new NodeIden("std::free", this->loc), {this->base}))->generate();
                }
                return nullptr;
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
    std::cout << "NodeUnary undefined operator. Operator " << (int)this->type << std::endl;
    return nullptr;
}

Node* NodeUnary::comptime() {
    switch(this->type) {
        case TokType::Minus:
            Node* comptimed = this->base->comptime();
            if(instanceof<NodeInt>(comptimed)) return new NodeInt(-((NodeInt*)comptimed)->value);
            return new NodeFloat(-((NodeFloat*)comptimed)->value);
        case TokType::Ne: return new NodeBool(!(((NodeBool*)this->base->comptime()))->value);
        default: break;
    }
    return this;
}

Node* NodeUnary::copy() {return new NodeUnary(this->loc, this->type, this->base->copy());}