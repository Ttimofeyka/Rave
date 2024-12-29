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

NodeGet::~NodeGet() {
    if(base != nullptr) delete base;
}

Type* NodeGet::getType() {
    Type* baseType = this->base->getType();
    TypeStruct* ts = nullptr;

    if(instanceof<TypeStruct>(baseType)) ts = static_cast<TypeStruct*>(baseType);
    else if(instanceof<TypePointer>(baseType) && instanceof<TypeStruct>(baseType->getElType())) ts = static_cast<TypeStruct*>(static_cast<TypePointer*>(baseType)->instance);
    else return baseType;

    if(!ts) {
        generator->error("type '" + baseType->toString() + "' is not a structure!", loc);
        return nullptr;
    }

    TypeStruct* old = ts;

    // Replace ts if needed
    auto it = generator->toReplace.find(ts->name);
    while(it != generator->toReplace.end()) {
        ts = static_cast<TypeStruct*>(it->second);
        it = generator->toReplace.find(ts->name);
    }

    // Check method table
    auto methodIt = AST::methodTable.find({ts->name, this->field});
    if(methodIt != AST::methodTable.end()) {
        return methodIt->second->getType();
    }

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
    RaveValue ptr;
    std::string structName;
    Type* ty = nullptr;

    if(instanceof<NodeIden>(this->base)) {
        NodeIden* niden = static_cast<NodeIden*>(this->base);
        if(currScope->localVars.find(niden->name) != currScope->localVars.end()) currScope->localVars[niden->name]->isUsed = true;
        ptr = checkStructure(currScope->getWithoutLoad(niden->name, loc));
        ty = ptr.type;

        Type* t = ty;

        while(instanceof<TypePointer>(t)) t = t->getElType();

        if(!instanceof<TypeStruct>(t)) generator->error("type '" + ty->toString() + "' is not a structure!", loc);
    }
    else if(instanceof<NodeIndex>(this->base) || instanceof<NodeGet>(this->base)) {
        if(instanceof<NodeIndex>(this->base)) static_cast<NodeIndex*>(this->base)->isMustBePtr = true;
        else static_cast<NodeGet*>(this->base)->isMustBePtr = true;
        ptr = checkStructure(this->base->generate());
    }
    else if(instanceof<NodeCall>(this->base) || instanceof<NodeDone>(this->base)) {
        Node* node = this->base;
        ty = node->getType();
        if(ty != nullptr && instanceof<TypeStruct>(ty)) {
            structName = static_cast<TypeStruct*>(ty)->name;
            RaveValue f = checkIn(structName);
            if(f.value != nullptr) return f;
        }
        ptr = checkStructure(node->generate());
    }
    else {
        generator->error("assert into NodeGet (" + std::string(typeid(this->base[0]).name()) + ")", this->loc);
        return {};
    }

    TypeStruct* tstruct = static_cast<TypeStruct*>(ty ? ty->getElType() : ptr.type->getElType());

    for(auto& type : tstruct->types) {
        auto it = generator->toReplace.find(type->toString());
        if(it != generator->toReplace.end()) type = it->second;
    }

    if(!tstruct->types.empty()) tstruct->updateByTypes();

    if(structName.empty()) structName = tstruct->toString();

    RaveValue f = checkIn(structName);
    if(f.value != nullptr) return f;

    auto structPair = std::make_pair(structName, this->field);
    int fieldNumber = AST::structsNumbers[structPair].number;

    if(this->isMustBePtr) return LLVM::structGep(ptr, fieldNumber, "NodeGet_generate_ptr");

    return LLVM::load(LLVM::structGep(ptr, fieldNumber, "NodeGet_generate_preload"), "NodeGet_generate_load", loc);
}

void NodeGet::check() {this->isChecked = true;}

Node* NodeGet::copy() {
    NodeGet* nget = new NodeGet(this->base->copy(), this->field, this->isMustBePtr, this->loc);
    nget->isPtrForIndex = this->isPtrForIndex;
    return nget;
}

Node* NodeGet::comptime() {return this;}