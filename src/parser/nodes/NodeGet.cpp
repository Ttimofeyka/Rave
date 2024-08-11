/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "../../include/parser/nodes/NodeFunc.hpp"
#include "../../include/parser/nodes/NodeVar.hpp"
#include "../../include/parser/nodes/NodeGet.hpp"
#include "../../include/parser/nodes/NodeIden.hpp"
#include "../../include/parser/nodes/NodeIndex.hpp"
#include "../../include/parser/nodes/NodeCall.hpp"
#include "../../include/parser/nodes/NodeDone.hpp"
#include "../../include/parser/ast.hpp"
#include "../../include/llvm.hpp"
#include <vector>
#include <string>

NodeGet::NodeGet(Node* base, std::string field, bool isMustBePtr, long loc) {
    this->base = base;
    this->field = field;
    this->isMustBePtr = isMustBePtr;
    this->loc = loc;
}

Type* NodeGet::getType() {
    Type* baseType = this->base->getType();
    TypeStruct* ts = nullptr;

    if(instanceof<TypeStruct>(baseType)) ts = static_cast<TypeStruct*>(baseType);
    else if(instanceof<TypePointer>(baseType)) ts = static_cast<TypeStruct*>(static_cast<TypePointer*>(baseType)->instance);
    else return baseType;

    if(!ts) {
        generator->error("type '" + baseType->toString() + "' is not a structure!", loc);
        return nullptr;
    }

    // Replace ts if needed
    auto it = generator->toReplace.find(ts->name);
    while(it != generator->toReplace.end()) {
        ts = static_cast<TypeStruct*>(it->second);
        it = generator->toReplace.find(ts->name);
    }

    // Check method table
    auto methodIt = AST::methodTable.find({ts->name, this->field});
    if(methodIt != AST::methodTable.end()) return methodIt->second->getType();

    // Check struct numbers
    auto structIt = AST::structsNumbers.find({ts->name, this->field});
    if(structIt != AST::structsNumbers.end()) return structIt->second.var->getType();

    // Check for generic types
    for(const auto& x : generator->toReplace) {
        if(x.first.find('<') != std::string::npos) return x.second;
    }

    generator->error("structure '" + ts->name + "' does not contain element '" + field + "'!", loc);
    return nullptr;
}

LLVMValueRef NodeGet::checkStructure(LLVMValueRef ptr) {
    LLVMTypeRef type = LLVMTypeOf(ptr);
    
    if(!LLVMGetTypeKind(type) == LLVMPointerTypeKind) {
        LLVMValueRef temp = LLVM::alloc(type, "NodeGet_checkStructure");
        LLVMBuildStore(generator->builder, ptr, temp);
        return temp;
    }
    
    while(LLVMGetTypeKind(LLVMGetElementType(type)) == LLVMPointerTypeKind) {
        ptr = LLVM::load(ptr, "NodeGet_checkStructure_load");
        type = LLVMTypeOf(ptr);
    }
    
    return ptr;
}

LLVMValueRef NodeGet::checkIn(std::string structure) {
    auto structIt = AST::structTable.find(structure);
    if(structIt == AST::structTable.end()) {
        generator->error("structure '" + structure + "' does not exist!", loc);
        return nullptr;
    }

    const auto member = std::make_pair(structure, field);
    auto numberIt = AST::structsNumbers.find(member);
    
    if(numberIt == AST::structsNumbers.end()) {
        auto methodIt = AST::methodTable.find(member);
        if(methodIt != AST::methodTable.end()) return generator->functions[methodIt->second->name];
        generator->error("structure '" + structure + "' does not contain element '" + field + "'!", loc);
        return nullptr;
    }

    elementIsConst = instanceof<TypeConst>(numberIt->second.var->type);
    return nullptr;
}

LLVMValueRef NodeGet::generate() {
    if(instanceof<NodeIden>(this->base)) {
        LLVMValueRef ptr = checkStructure(currScope->getWithoutLoad(((NodeIden*)this->base)->name,loc));
        std::string structName = std::string(LLVMGetStructName(LLVMGetElementType(LLVMTypeOf(ptr))));
        LLVMValueRef f = this->checkIn(structName);
        if(f != nullptr) return f;
        if(this->isMustBePtr) return LLVM::structGep(
            ptr,
            AST::structsNumbers[std::pair<std::string, std::string>(structName, this->field)].number,
            "NodeGet_generate_Iden_ptr"
        );
        return LLVM::load(LLVM::structGep(
            ptr,
            AST::structsNumbers[std::pair<std::string, std::string>(structName, this->field)].number,
            "NodeGet_generate_Iden_preload"
        ), "NodeGet_generate_Iden_load");
    }

    if(instanceof<NodeIndex>(this->base) || instanceof<NodeGet>(this->base)) {
        if(instanceof<NodeIndex>(this->base)) ((NodeIndex*)this->base)->isMustBePtr = true;
        else ((NodeGet*)this->base)->isMustBePtr = true;

        LLVMValueRef ptr = checkStructure(this->base->generate());
        std::string structName = std::string(LLVMGetStructName(LLVMGetElementType(LLVMTypeOf(ptr))));
        LLVMValueRef f = checkIn(structName);

        if(f != nullptr) return f;
        if(this->isMustBePtr) return LLVM::structGep(
            ptr,
            AST::structsNumbers[std::pair<std::string, std::string>(structName, this->field)].number,
            "NodeGet_generate_Index_ptr"
        );
        return LLVM::load(LLVM::structGep(
            ptr,
            AST::structsNumbers[std::pair<std::string, std::string>(structName, this->field)].number,
            "NodeGet_generate_Index_preload"
        ), "NodeGet_generate_Index_load");
    }

    if(instanceof<NodeCall>(this->base)) {
        NodeCall* ncall = (NodeCall*)this->base;
        Type* ty = ncall->getType();
        std::string structName = "";
        if(ty != nullptr && instanceof<TypeStruct>(ty)) {
            structName = ((TypeStruct*)ty)->name;
            LLVMValueRef f = checkIn(structName);
            if(f != nullptr) return f;
        }
        LLVMValueRef ptr = checkStructure(ncall->generate());
        if(structName == "") structName = std::string(LLVMGetStructName(LLVMGetElementType(LLVMTypeOf(ptr))));

        if(this->isMustBePtr) return LLVM::structGep(
            ptr,
            AST::structsNumbers[std::pair<std::string, std::string>(structName, this->field)].number,
            "NodeGet_generate_Index_ptr"
        );
        return LLVM::load(LLVM::structGep(
            ptr,
            AST::structsNumbers[std::pair<std::string, std::string>(structName, this->field)].number,
            "NodeGet_generate_Index_preload"
        ), "NodeGet_generate_Index_load");
    }

    if(instanceof<NodeDone>(this->base)) {
        NodeDone* ndone = (NodeDone*)this->base;
        Type* ty = ndone->getType();
        std::string structName = "";
        if(ty != nullptr && instanceof<TypeStruct>(ty)) {
            structName = ((TypeStruct*)ty)->name;
            LLVMValueRef f = checkIn(structName);
            if(f != nullptr) return f;
        }
        LLVMValueRef ptr = checkStructure(ndone->generate());
        if(structName == "") structName = std::string(LLVMGetStructName(LLVMGetElementType(LLVMTypeOf(ptr))));

        if(this->isMustBePtr) return LLVM::structGep(
            ptr,
            AST::structsNumbers[std::pair<std::string, std::string>(structName, this->field)].number,
            "NodeGet_generate_Index_ptr"
        );
        return LLVM::load(LLVM::structGep(
            ptr,
            AST::structsNumbers[std::pair<std::string, std::string>(structName, this->field)].number,
            "NodeGet_generate_Index_preload"
        ), "NodeGet_generate_Index_load");
    }

    generator->error("assert into NodeGet (" + std::string(typeid(this->base[0]).name()) + ")", this->loc);
    return nullptr;
}

void NodeGet::check() {this->isChecked = true;}
Node* NodeGet::copy() {return new NodeGet(this->base->copy(), this->field, this->isMustBePtr, this->loc);}
Node* NodeGet::comptime() {return this;}