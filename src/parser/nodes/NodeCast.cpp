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
#include "../../include/parser/nodes/NodeStruct.hpp"
#include "../../include/parser/nodes/NodeBuiltin.hpp"
#include "../../include/llvm.hpp"
#include <iostream>

NodeCast::NodeCast(Type* type, Node* value, int loc) {
    this->type = type;
    this->value = value;
    this->loc = loc;
}

void NodeCast::check() {
    if(isChecked) return;
    isChecked = true;

    value->check();
}

NodeCast::~NodeCast() {
    if(this->value != nullptr) delete this->value;
    if(this->type != nullptr && !instanceof<TypeBasic>(this->type) && !instanceof<TypeVoid>(this->type)) delete this->type;
}

Type* NodeCast::getType() {return this->type->copy();}
Node* NodeCast::copy() {return new NodeCast(this->type->copy(), this->value->copy(), this->loc);}
Node* NodeCast::comptime() {return nullptr;}

RaveValue NodeCast::generate() {
    RaveValue result = {nullptr, nullptr};

    checkForTemplated(this->type);

    if(instanceof<TypeVoid>(this->type)) generator->error("cannot cast to \033[1mvoid\033[22m!", loc);

    if(instanceof<TypeBasic>(this->type)) {
        TypeBasic* tbasic = (TypeBasic*)this->type;

        if(tbasic->type == BasicType::Bool && tbasic->toString() != "bool") return (new NodeCast(new TypeStruct(tbasic->toString()), this->value, this->loc))->generate();

        result = this->value->generate();

        if(instanceof<TypeBasic>(result.type)) {
            TypeBasic* tbasic2 = (TypeBasic*)result.type;

            if(tbasic->isFloat()) {
                if(tbasic2->isFloat()) return {LLVMBuildFPCast(generator->builder, result.value, generator->genType(this->type, this->loc), "NodeCast_ftof"), this->type};
                return {LLVMBuildSIToFP(generator->builder, result.value, generator->genType(this->type, this->loc), "NodeCast_itof"), this->type};
            }

            if(tbasic2->isFloat()) return {LLVMBuildFPToSI(generator->builder, result.value, generator->genType(this->type, this->loc), "NodeCast_ftoi"), this->type};
            return {LLVMBuildIntCast2(generator->builder, result.value, generator->genType(type, loc), !tbasic->isUnsigned(), "NodeCast_itoi"), this->type};
        }

        if(instanceof<TypePointer>(result.type)) {
            if(!tbasic->isFloat()) return {LLVMBuildPtrToInt(generator->builder, result.value, generator->genType(tbasic, this->loc), "NodeCast_ptoi"), this->type};
            return {
                LLVMBuildSIToFP(
                    generator->builder, LLVMBuildPtrToInt(generator->builder, result.value, LLVMInt64TypeInContext(generator->context), "NodeCast_temp"),
                    generator->genType(this->type, loc), "NodeCast_ptof"
                ), this->type
            };
        }

        if(instanceof<TypeStruct>(result.type)) generator->error("casting a structure to the basic type is prohibited!", loc);

        if(tbasic->isFloat()) return {LLVMBuildFPCast(generator->builder, result.value, generator->genType(this->type, this->loc), "NodeCast_ftof"), this->type};
        return {LLVMBuildFPToSI(generator->builder, result.value, generator->genType(this->type, this->loc), "NodeCast_ftoi"), this->type};
    }

    result = this->value->generate();

    if(instanceof<TypePointer>(this->type)) {
        TypePointer* tpointer = (TypePointer*)this->type;

        if(instanceof<TypeBasic>(result.type)) return {LLVMBuildIntToPtr(generator->builder, result.value, generator->genType(tpointer, this->loc), "NodeCast_itop"), this->type};

        if(instanceof<TypeStruct>(result.type)) {
            if(instanceof<NodeIden>(this->value)) {((NodeIden*)this->value)->isMustBePtr = true; result = this->value->generate();}
            else if(instanceof<NodeGet>(this->value)) {((NodeGet*)this->value)->isMustBePtr = true; result = this->value->generate();}
            else if(instanceof<NodeIndex>(this->value)) {((NodeIndex*)this->value)->isMustBePtr = true; result = this->value->generate();}
        }

        if(instanceof<TypeArray>(result.type)) generator->error("cannot cast an array to a pointer!", this->loc);

        return {LLVMBuildPointerCast(generator->builder, result.value, generator->genType(this->type, this->loc), "NodeCast_ptop"), this->type};
    }

    if(instanceof<TypeFunc>(this->type)) {
        TypeFunc* tfunc = (TypeFunc*)this->type;
        if(instanceof<TypeBasic>(result.type)) return {LLVMBuildIntToPtr(generator->builder, result.value, generator->genType(tfunc, this->loc), "NodeCast_itofn"), this->type};
        return {LLVMBuildPointerCast(generator->builder, result.value, generator->genType(tfunc, this->loc), "NodeCast_ptop"), this->type};
    }

    if(instanceof<TypeStruct>(this->type)) {
        TypeStruct* tstruct = (TypeStruct*)this->type;
        if(generator->toReplace.find(tstruct->name) != generator->toReplace.end()) return (new NodeCast(generator->toReplace[tstruct->name], this->value, this->loc))->generate();
        RaveValue ptrResult = LLVM::alloc(result.type, "NodeCast_ptrResult");
        LLVMBuildStore(generator->builder, result.value, ptrResult.value);
        return LLVM::load({
            LLVMBuildBitCast(generator->builder, ptrResult.value, LLVMPointerType(generator->genType(tstruct, this->loc), 0), "NodeCast_tempFn"),
            new TypePointer(tstruct)
        }, "NodeCast_fnload", loc);
    }

    if(instanceof<TypeBuiltin>(this->type)) {
        TypeBuiltin* tbuiltin = (TypeBuiltin*)this->type;
        NodeBuiltin* nb = new NodeBuiltin(tbuiltin->name, tbuiltin->args, this->loc, tbuiltin->block);
        nb->generate();
        this->type = nb->type;
        return this->generate();
    }

    generator->error("NodeCast assert!", this->loc);
    return {};
}