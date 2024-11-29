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

NodeGet::NodeGet(Node* base, std::string field, bool isMustBePtr, int loc) {
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

RaveValue NodeGet::checkStructure(RaveValue ptr) {
    if(!instanceof<TypePointer>(ptr.type)) {
        if(LLVMIsAArgument(ptr.value) && LLVMGetTypeKind(LLVMTypeOf(ptr.value)) == LLVMPointerTypeKind) ptr.type = new TypePointer(ptr.type);
        else {
            RaveValue temp = LLVM::alloc(ptr.type, "NodeGet_checkStructure");
            LLVMBuildStore(generator->builder, ptr.value, temp.value);
            return temp;
        }
    }
    
    while(instanceof<TypePointer>(ptr.type->getElType())) ptr = LLVM::load(ptr, "NodeGet_checkStructure_load", loc);

    while(generator->toReplace.find(ptr.type->getElType()->toString()) != generator->toReplace.end()) {
        ((TypePointer*)ptr.type)->instance = generator->toReplace[ptr.type->getElType()->toString()];
    }
    
    return ptr;
}

RaveValue NodeGet::checkIn(std::string structure) {
    auto structIt = AST::structTable.find(structure);
    if(structIt == AST::structTable.end()) {
        generator->error("structure '" + structure + "' does not exist!", loc);
        return {};
    }

    const auto member = std::make_pair(structure, field);
    auto numberIt = AST::structsNumbers.find(member);
    
    if(numberIt == AST::structsNumbers.end()) {
        auto methodIt = AST::methodTable.find(member);
        if(methodIt != AST::methodTable.end()) return generator->functions[methodIt->second->name];
        generator->error("structure '" + structure + "' does not contain element '" + field + "'!", loc);
        return {};
    }

    elementIsConst = instanceof<TypeConst>(numberIt->second.var->type);
    return {nullptr, nullptr};
}

RaveValue NodeGet::generate() {
    if(instanceof<NodeIden>(this->base)) {
        NodeIden* niden = ((NodeIden*)this->base);

        if(currScope->localVars.find(niden->name) != currScope->localVars.end()) currScope->localVars[niden->name]->isUsed = true;
        RaveValue ptr = checkStructure(currScope->getWithoutLoad(niden->name, loc));
        Type* ty = ptr.type;

        TypeStruct* tstruct = (TypeStruct*)ty->getElType();
        for(int i=0; i<tstruct->types.size(); i++) {
            if(generator->toReplace.find(tstruct->types[i]->toString()) != generator->toReplace.end()) tstruct->types[i] = generator->toReplace[tstruct->types[i]->toString()];
        }
        if(tstruct->types.size() > 0) tstruct->updateByTypes();

        std::string structName = tstruct->toString();
        RaveValue f = this->checkIn(structName);
        if(f.value != nullptr) return f;

        if(this->isMustBePtr) return LLVM::structGep(
            ptr,
            AST::structsNumbers[std::pair<std::string, std::string>(structName, this->field)].number,
            "NodeGet_generate_Iden_ptr"
        );

        return LLVM::load(LLVM::structGep(
            ptr,
            AST::structsNumbers[std::pair<std::string, std::string>(structName, this->field)].number,
            "NodeGet_generate_Iden_preload"
        ), "NodeGet_generate_Iden_load", loc);
    }

    if(instanceof<NodeIndex>(this->base) || instanceof<NodeGet>(this->base)) {
        if(instanceof<NodeIndex>(this->base)) ((NodeIndex*)this->base)->isMustBePtr = true;
        else ((NodeGet*)this->base)->isMustBePtr = true;

        RaveValue ptr = checkStructure(this->base->generate());

        TypeStruct* tstruct = (TypeStruct*)ptr.type->getElType();
        for(int i=0; i<tstruct->types.size(); i++) {
            if(generator->toReplace.find(tstruct->types[i]->toString()) != generator->toReplace.end()) tstruct->types[i] = generator->toReplace[tstruct->types[i]->toString()];
        }
        if(tstruct->types.size() > 0) tstruct->updateByTypes();

        std::string structName = tstruct->toString();
        RaveValue f = checkIn(structName);

        if(f.value != nullptr) return f;

        if(this->isMustBePtr) return LLVM::structGep(
            ptr,
            AST::structsNumbers[std::pair<std::string, std::string>(structName, this->field)].number,
            "NodeGet_generate_Index_ptr"
        );

        return LLVM::load(LLVM::structGep(
            ptr,
            AST::structsNumbers[std::pair<std::string, std::string>(structName, this->field)].number,
            "NodeGet_generate_Index_preload"
        ), "NodeGet_generate_Index_load", loc);
    }

    if(instanceof<NodeCall>(this->base)) {
        NodeCall* ncall = (NodeCall*)this->base;
        Type* ty = ncall->getType();
        std::string structName = "";

        if(ty != nullptr && instanceof<TypeStruct>(ty)) {
            structName = ((TypeStruct*)ty)->name;
            RaveValue f = checkIn(structName);
            if(f.value != nullptr) return f;
        }

        RaveValue ptr = checkStructure(ncall->generate());
        if(structName == "") structName = ptr.type->getElType()->toString();

        if(this->isMustBePtr) return LLVM::structGep(
            ptr,
            AST::structsNumbers[std::pair<std::string, std::string>(structName, this->field)].number,
            "NodeGet_generate_Index_ptr"
        );

        return LLVM::load(LLVM::structGep(
            ptr,
            AST::structsNumbers[std::pair<std::string, std::string>(structName, this->field)].number,
            "NodeGet_generate_Index_preload"
        ), "NodeGet_generate_Index_load", loc);
    }

    if(instanceof<NodeDone>(this->base)) {
        NodeDone* ndone = (NodeDone*)this->base;
        Type* ty = ndone->getType();
        std::string structName = "";

        if(ty != nullptr && instanceof<TypeStruct>(ty)) {
            structName = ((TypeStruct*)ty)->name;
            RaveValue f = checkIn(structName);
            if(f.value != nullptr) return f;
        }

        RaveValue ptr = checkStructure(ndone->generate());
        if(structName == "") structName = ptr.type->getElType()->toString();

        if(this->isMustBePtr) return LLVM::structGep(
            ptr,
            AST::structsNumbers[std::pair<std::string, std::string>(structName, this->field)].number,
            "NodeGet_generate_Index_ptr"
        );
        return LLVM::load(LLVM::structGep(
            ptr,
            AST::structsNumbers[std::pair<std::string, std::string>(structName, this->field)].number,
            "NodeGet_generate_Index_preload"
        ), "NodeGet_generate_Index_load", loc);
    }

    generator->error("assert into NodeGet (" + std::string(typeid(this->base[0]).name()) + ")", this->loc);
    return {};
}

void NodeGet::check() {this->isChecked = true;}

Node* NodeGet::copy() {
    NodeGet* nget = new NodeGet(this->base->copy(), this->field, this->isMustBePtr, this->loc);
    nget->isPtrForIndex = this->isPtrForIndex;
    return nget;
}

Node* NodeGet::comptime() {return this;}