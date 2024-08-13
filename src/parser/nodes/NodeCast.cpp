/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "../../include/parser/ast.hpp"
#include <vector>
#include <string>
#include "../../include/parser/nodes/NodeCast.hpp"
#include "../../include/parser/nodes/NodeIden.hpp"
#include "../../include/parser/nodes/NodeGet.hpp"
#include "../../include/parser/nodes/NodeIndex.hpp"
#include "../../include/parser/nodes/NodeBuiltin.hpp"
#include "../../include/llvm.hpp"

NodeCast::NodeCast(Type* type, Node* value, int loc) {
    this->type = type;
    this->value = value;
    this->loc = loc;
}

void NodeCast::check() {
    bool oldCheck = isChecked;
    this->isChecked = true;
    if(!oldCheck) this->value->check();
}

Type* NodeCast::getType() {return this->type->copy();}
Node* NodeCast::copy() {return new NodeCast(this->type->copy(), this->value->copy(), this->loc);}
Node* NodeCast::comptime() {return nullptr;}

LLVMValueRef NodeCast::generate() {
    LLVMValueRef result = nullptr;

    if(instanceof<TypeBasic>(this->type)) {
        TypeBasic* tbasic = (TypeBasic*)this->type;
        if(tbasic->type == BasicType::Bool && tbasic->toString() != "bool") {
            return (new NodeCast(new TypeStruct(tbasic->toString()), this->value, this->loc))->generate();
        }
        result = this->value->generate();
        if(LLVMGetTypeKind(LLVMTypeOf(result)) == LLVMIntegerTypeKind) {
            if(tbasic->isFloat()) return LLVMBuildSIToFP(generator->builder, result, generator->genType(this->type, this->loc), "NodeCast_itof");
            return LLVMBuildIntCast(generator->builder, result, generator->genType(type, loc), "NodeCast_itoi");
        }

        if(LLVMGetTypeKind(LLVMTypeOf(result)) == LLVMPointerTypeKind) {
            if(!tbasic->isFloat()) return LLVMBuildPtrToInt(generator->builder, result, generator->genType(tbasic, this->loc), "NodeCast_ptoi");
            return LLVM::load(LLVMBuildPointerCast(generator->builder, result, LLVMPointerType(generator->genType(this->type, this->loc), 0), "NodeCast_ptofLoad"), "NodeCast_ptofLoadC");
        }

        if(LLVMGetTypeKind(LLVMTypeOf(result)) == LLVMStructTypeKind) {
            LLVMValueRef _temp = LLVM::alloc(LLVMTypeOf(result), "NodeCast_stoiTemp");
            LLVMBuildStore(generator->builder, result, _temp);
            if(tbasic->isFloat()) return LLVMBuildSIToFP(generator->builder, LLVMBuildPtrToInt(generator->builder, _temp, LLVMInt32TypeInContext(generator->context), "NodeCast_ptoi_stop"), generator->genType(type,loc), "NodeCast_ptoi_stopC");
            return LLVMBuildPtrToInt(generator->builder, _temp, LLVMInt32TypeInContext(generator->context), "NodeCast_stoi");
        }

        if(tbasic->isFloat()) return LLVMBuildFPCast(generator->builder, result, generator->genType(this->type, this->loc), "NodeCast_ftof");
        return LLVMBuildFPToSI(generator->builder, result, generator->genType(this->type, this->loc), "NodeCast_ftoi");
    }
    result = this->value->generate();
    if(instanceof<TypePointer>(this->type)) {
        TypePointer* tpointer = (TypePointer*)this->type;
        if(LLVMGetTypeKind(LLVMTypeOf(result)) == LLVMIntegerTypeKind) return LLVMBuildIntToPtr(generator->builder, result, generator->genType(tpointer, this->loc), "NodeCast_itop");
        if(LLVMGetTypeKind(LLVMTypeOf(result)) == LLVMStructTypeKind) {
            if(instanceof<NodeIden>(this->value)) {((NodeIden*)this->value)->isMustBePtr = true; result = this->value->generate();}
            else if(instanceof<NodeGet>(this->value)) {((NodeGet*)this->value)->isMustBePtr = true; result = this->value->generate();}
            else if(instanceof<NodeIndex>(this->value)) {((NodeIndex*)this->value)->isMustBePtr = true; result = this->value->generate();}
        }
        return LLVMBuildPointerCast(generator->builder, result, generator->genType(this->type, this->loc), "NodeCast_ptop");
    }
    if(instanceof<TypeFunc>(this->type)) {
        TypeFunc* tfunc = (TypeFunc*)this->type;
        if(LLVMGetTypeKind(LLVMTypeOf(result)) == LLVMIntegerTypeKind) return LLVMBuildIntToPtr(generator->builder, result, generator->genType(tfunc, this->loc), "NodeCast_itofn");
        return LLVMBuildPointerCast(generator->builder, result, generator->genType(tfunc, this->loc), "NodeCast_ptop");
    }
    if(instanceof<TypeStruct>(this->type)) {
        TypeStruct* tstruct = (TypeStruct*)this->type;
        if(generator->toReplace.find(tstruct->name) != generator->toReplace.end()) return (new NodeCast(generator->toReplace[tstruct->name], this->value, this->loc))->generate();
        LLVMValueRef ptrResult = LLVM::alloc(LLVMTypeOf(result), "NodeCast_ptrResult");
        LLVMBuildStore(generator->builder, result, ptrResult);
        return LLVM::load(LLVMBuildBitCast(generator->builder, ptrResult, LLVMPointerType(generator->genType(tstruct, this->loc), 0), "NodeCast_tempFn"), "NodeCast_fnload");
    }
    if(instanceof<TypeBuiltin>(this->type)) {
        TypeBuiltin* tbuiltin = (TypeBuiltin*)this->type;
        NodeBuiltin* nb = new NodeBuiltin(tbuiltin->name, tbuiltin->args, this->loc, tbuiltin->block);
        nb->generate();
        this->type = nb->type;
        return this->generate();
    }
    generator->error("NodeCast assert!", this->loc);
    return nullptr;
}