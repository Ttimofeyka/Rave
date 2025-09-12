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
#include "../../include/parser/nodes/NodeBinary.hpp"
#include "../../include/parser/ast.hpp"
#include "../../include/llvm.hpp"
#include <vector>
#include <string>

/**
 * Constructs a NodeGet for member access operations
 * @param base: Node representing the base object
 * @param field: Name of the member being accessed
 * @param isMustBePtr: Flag indicating if result must be a pointer
 * @param loc: Source line number
 */
NodeGet::NodeGet(Node* base, std::string field, bool isMustBePtr, int loc) {
    this->base = base;
    this->field = field;
    this->isMustBePtr = isMustBePtr;
    this->loc = loc;
}

NodeGet::~NodeGet() {
    if (base != nullptr) delete base;
}

Type* NodeGet::getType() {
    Type* baseType = this->base->getType();
    TypeStruct* ts = nullptr;

    // Resolve base to TypeStruct (either directly or via pointer)
    if (instanceof<TypeStruct>(baseType)) ts = static_cast<TypeStruct*>(baseType);
    else if (instanceof<TypePointer>(baseType)) {
        Type* elType = baseType->getElType();

        if (instanceof<TypeStruct>(elType)) ts = static_cast<TypeStruct*>(elType);
    }
    
    if (!ts) {
        generator->error("type \033[1m" + baseType->toString() + "\033[22m is not a structure!", loc);
        return nullptr;
    }

    // Apply template substitutions if needed
    Types::replaceTemplates((Type**)&ts);

    const std::string& structName = ts->name;
    auto memberKey = std::make_pair(structName, field);

    if (auto methodIt = AST::methodTable.find(memberKey); methodIt != AST::methodTable.end()) return methodIt->second->getType();

    if (auto structIt = AST::structMembersTable.find(memberKey); structIt != AST::structMembersTable.end()) return structIt->second.var->getType();

    if (field.find('<') != std::string::npos) {
        memberKey = std::make_pair(structName, field.substr(0, field.find('<')));
        if (auto methodIt = AST::methodTable.find(memberKey); methodIt != AST::methodTable.end()) return methodIt->second->getType();
    }

    generator->error("structure \033[1m" + structName + "\033[22m does not contain element \033[1m" + field + "\033[22m!", loc);
    return nullptr;
}

RaveValue NodeGet::checkStructure(RaveValue ptr) {
    // Handle non-pointer types
    if (!instanceof<TypePointer>(ptr.type)) {
        if (LLVMIsAArgument(ptr.value) && LLVMGetTypeKind(LLVMTypeOf(ptr.value)) == LLVMPointerTypeKind) ptr.type = new TypePointer(ptr.type);
        else {
            RaveValue temp = LLVM::alloc(ptr.type, "NodeGet_checkStructure");
            LLVMBuildStore(generator->builder, ptr.value, temp.value);
            return temp;
        }
    }

    // Dereference nested pointers
    while (instanceof<TypePointer>(ptr.type->getElType())) ptr = LLVM::load(ptr, "NodeGet_checkStructure_load", loc);

    // Apply type replacements if needed
    const std::string typeStr = ptr.type->getElType()->toString();
    if (auto it = generator->toReplace.find(typeStr); it != generator->toReplace.end())
        static_cast<TypePointer*>(ptr.type)->instance = it->second;
    
    return ptr;
}

/**
 * Checks member existence in struct and returns its value
 * Optimized with combined lookup and const detection
 */
RaveValue NodeGet::checkIn(std::string structure) {
    if (auto structIt = AST::structTable.find(structure); structIt == AST::structTable.end()) {
        generator->error("structure \033[1m" + structure + "\033[22m does not exist!", loc);
        return {};
    }

    const auto memberKey = std::make_pair(structure, field);

    if (auto numberIt = AST::structMembersTable.find(memberKey); numberIt != AST::structMembersTable.end()) {
        elementIsConst = instanceof<TypeConst>(numberIt->second.var->type);
        return {nullptr, nullptr};
    } 
    
    if (auto methodIt = AST::methodTable.find(memberKey); methodIt != AST::methodTable.end()) return generator->functions[methodIt->second->name];

    generator->error("structure \033[1m" + structure + "\033[22m does not contain element \033[1m" + field + "\033[22m!", loc);
    return {};
}

RaveValue NodeGet::generate() {
    RaveValue ptr;
    std::string structName;
    Type* ty = nullptr;

    if (instanceof<NodeIden>(base)) {
        NodeIden* niden = static_cast<NodeIden*>(base);
        if (auto varIt = currScope->localVars.find(niden->name); varIt != currScope->localVars.end()) varIt->second->isUsed = true;

        ptr = checkStructure(currScope->getWithoutLoad(niden->name, loc));
        ty = ptr.type;

        // Resolve pointer chains
        Type* t = ty;
        while (instanceof<TypePointer>(t)) t = t->getElType();
        if (!instanceof<TypeStruct>(t)) generator->error("type \033[1m" + ty->toString() + "\033[22m is not a structure!", loc);
    }
    else if (instanceof<NodeIndex>(base) || instanceof<NodeGet>(base)) {
        if (instanceof<NodeIndex>(base)) static_cast<NodeIndex*>(base)->isMustBePtr = true;
        else static_cast<NodeGet*>(base)->isMustBePtr = true;
        ptr = checkStructure(base->generate());
    }
    else {
        ty = base->getType();

        // Early return for method calls
        if (ty && instanceof<TypeStruct>(ty)) {
            structName = static_cast<TypeStruct*>(ty)->name;
            if (RaveValue f = checkIn(structName); f.value) return f;
        }

        ptr = checkStructure(base->generate());
    }

    // Resolve struct type with template handling
    TypeStruct* tstruct = static_cast<TypeStruct*>(ty ? ty->getElType() : ptr.type->getElType());
    Types::replaceTemplates(reinterpret_cast<Type**>(&tstruct));
    if (structName.empty()) structName = tstruct->toString();

    // Check for method access
    if (RaveValue f = checkIn(structName); f.value) return f;

    // Generate struct member access
    const int fieldNumber = AST::structMembersTable[std::make_pair(structName, field)].number;
    RaveValue memberPtr = LLVM::structGep(ptr, fieldNumber, "NodeGet_generate_ptr");

    return isMustBePtr ? memberPtr : LLVM::load(memberPtr, "NodeGet_generate_load", loc);
}

void NodeGet::check() { isChecked = true; }

Node* NodeGet::copy() {
    NodeGet* nget = new NodeGet(this->base->copy(), this->field, this->isMustBePtr, this->loc);
    nget->isPtrForIndex = this->isPtrForIndex;
    return nget;
}

Node* NodeGet::comptime() { return this; }
