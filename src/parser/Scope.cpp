/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

// Scope class implementation - Variable and scope management
// Split from ast.cpp for better maintainability

#include "../include/parser/ast.hpp"
#include "../include/parser/nodes/NodeVar.hpp"
#include "../include/parser/nodes/NodeGet.hpp"
#include "../include/parser/nodes/NodeIden.hpp"
#include "../include/parser/nodes/NodeFunc.hpp"
#include "../include/debug.hpp"

Scope::Scope(std::string funcName, std::unordered_map<std::string, int> args, std::unordered_map<std::string, NodeVar*> argVars) {
    DEBUG_LOG(Debug::Category::Scope, "Scope: Creating scope for function " + funcName);

    this->funcName = funcName;
    this->args = std::unordered_map<std::string, int>(args);
    this->argVars = std::unordered_map<std::string, NodeVar*>(argVars);
    this->aliasTable = std::unordered_map<std::string, Node*>();
    this->localScope = std::unordered_map<std::string, RaveValue>();
    this->localVars = std::unordered_map<std::string, NodeVar*>();
}

void Scope::remove(std::string name) {
    DEBUG_LOG(Debug::Category::Scope, "Scope: Removing variable " + name);

    if (this->localScope.find(name) != this->localScope.end()) {
        this->localScope.erase(name);
        this->localVars.erase(name);
    }
    else if (this->aliasTable.find(name) != this->aliasTable.end())
        this->aliasTable.erase(name);
}

TypeStruct* Scope::getThisStructType(int loc) {
    if (!this->has("this")) return nullptr;
    if (AST::funcTable.find(this->funcName) == AST::funcTable.end()) return nullptr;
    if (!AST::funcTable[this->funcName]->isMethod) return nullptr;

    NodeVar* nv = this->getVar("this", loc);
    if (nv == nullptr) return nullptr;

    Type* t = nv->type;
    if (instanceof<TypeStruct>(t)) return (TypeStruct*)t;
    if (instanceof<TypePointer>(t) && instanceof<TypeStruct>(t->getElType())) {
        return (TypeStruct*)(t->getElType());
    }
    return nullptr;
}

RaveValue Scope::get(std::string name, int loc) {
    DEBUG_LOG(Debug::Category::Scope, "Scope::get: Looking up " + name + " in function " + funcName);

    RaveValue value = {nullptr, nullptr};

    // Check various scopes in order of priority
    auto itNode = AST::aliasTable.find(name);
    if (itNode != AST::aliasTable.end())
        value = itNode->second->generate();
    else if ((itNode = generator->toReplaceValues.find(name)) != generator->toReplaceValues.end())
        value = itNode->second->generate();
    else if ((itNode = this->aliasTable.find(name)) != this->aliasTable.end())
        value = itNode->second->generate();
    else {
        auto itVal = localScope.find(name);
        if (itVal != localScope.end())
            value = itVal->second;
        else if ((itVal = generator->globals.find(name)) != generator->globals.end())
            value = itVal->second;
        else if (generator->functions.find(this->funcName) != generator->functions.end()) {
            if (this->args.find(name) == this->args.end()) {
                if (generator->functions.find(name) != generator->functions.end())
                    generator->error(name, loc);
            }
            else return {LLVMGetParam(generator->functions[this->funcName].value, this->args[name]),
                AST::funcTable[this->funcName]->getArgType(name)};
        }
    }

    // Check if it's a member of "this" struct
    if (value.value == nullptr && hasAtThis(name)) {
        TypeStruct* ts = getThisStructType(loc);
        if (ts != nullptr) {
            NodeGet* nget = new NodeGet(new NodeIden("this", loc), name, true, loc);
            value = nget->generate();
        }
    }

    if (value.value == nullptr)
        generator->error("undefined identifier \033[1m" + name + "\033[22m at function \033[1m" + this->funcName + "\033[22m!", loc);

    // Strip const and load pointer types
    value.type = Types::stripConst(value.type);
    if (instanceof<TypePointer>(value.type))
        value = LLVM::load(value, "scopeGetLoad", loc);
    value.type = Types::stripConst(value.type);

    return value;
}

RaveValue Scope::getWithoutLoad(std::string name, int loc) {
    DEBUG_LOG(Debug::Category::Scope, "Scope::getWithoutLoad: Looking up " + name);

    auto itNode = generator->toReplaceValues.find(name);
    if (itNode != generator->toReplaceValues.end())
        return itNode->second->generate();
    if ((itNode = AST::aliasTable.find(name)) != AST::aliasTable.end())
        return itNode->second->generate();
    if ((itNode = this->aliasTable.find(name)) != this->aliasTable.end())
        return itNode->second->generate();

    auto itVal = this->localScope.find(name);
    if (itVal != this->localScope.end())
        return itVal->second;
    if ((itVal = generator->globals.find(name)) != generator->globals.end())
        return itVal->second;
    if (hasAtThis(name)) {
        TypeStruct* ts = getThisStructType(loc);
        if (ts != nullptr) {
            NodeGet* nget = new NodeGet(new NodeIden("this", loc), name, true, loc);
            return nget->generate();
        }
    }
    if (this->args.find(name) == this->args.end())
        generator->error("undefined identifier \033[1m" + name + "\033[22m at function \033[1m" + this->funcName + "\033[22m!", loc);
    return {LLVMGetParam(generator->functions[this->funcName].value, this->args[name]),
        AST::funcTable[this->funcName]->getArgType(name)};
}

bool Scope::has(std::string name) {
    return AST::aliasTable.find(name) != AST::aliasTable.end() ||
        this->aliasTable.find(name) != this->aliasTable.end() ||
        this->localScope.find(name) != this->localScope.end() ||
        generator->globals.find(name) != generator->globals.end() ||
        this->args.find(name) != this->args.end();
}

bool Scope::hasAtThis(std::string name) {
    TypeStruct* ts = getThisStructType(-1);
    if (ts == nullptr) return false;
    return AST::structTable.find(ts->toString()) != AST::structTable.end() &&
           AST::structMembersTable.find({ts->toString(), name}) != AST::structMembersTable.end();
}

bool Scope::locatedAtThis(std::string name) {
    if (AST::aliasTable.find(name) != AST::aliasTable.end()) return false;
    if (this->aliasTable.find(name) != this->aliasTable.end()) return false;
    if (this->localScope.find(name) != this->localScope.end()) return false;
    if (generator->globals.find(name) != generator->globals.end()) return false;
    return this->hasAtThis(name);
}

NodeVar* Scope::getVar(std::string name, int loc) {
    DEBUG_LOG(Debug::Category::Scope, "Scope::getVar: Looking up variable " + name);

    auto it = this->localVars.find(name);
    if (it != this->localVars.end())
        return it->second;
    auto it2 = AST::varTable.find(name);
    if (it2 != AST::varTable.end())
        return it2->second;
    auto it3 = this->argVars.find(name);
    if (it3 != this->argVars.end()) {
        it3->second->isUsed = true;
        return it3->second;
    }
    auto it4 = this->aliasTable.find(name);
    if (it4 != this->aliasTable.end())
        return (new NodeVar(name, it4->second->copy(), false, false, false, {}, loc,
            it4->second->getType(), false, false, false));

    // Check if it's a member of "this" struct
    TypeStruct* ts = getThisStructType(loc);
    if (ts != nullptr && AST::structTable.find(ts->toString()) != AST::structTable.end() &&
        AST::structMembersTable.find({ts->toString(), name}) != AST::structMembersTable.end()) {
        return AST::structMembersTable[{ts->toString(), name}].var;
    }

    generator->error("undefined variable \033[1m" + name + "\033[22m!", loc);
    return nullptr;
}

void Scope::hasChanged(std::string name) {
    auto it = this->localVars.find(name);
    if (it != this->localVars.end())
        ((NodeVar*)it->second)->isChanged = true;
    auto it2 = AST::varTable.find(name);
    if (it2 != AST::varTable.end())
        it2->second->isChanged = true;
    auto it3 = this->argVars.find(name);
    if (it3 != this->argVars.end())
        ((NodeVar*)it3->second)->isChanged = true;
}

// Helper function to copy scope
Scope* copyScope(Scope* original) {
    Scope* newScope = new Scope(original->funcName, original->args, original->argVars);
    newScope->fnEnd = original->fnEnd;
    newScope->funcHasRet = original->funcHasRet;
    newScope->localVars = original->localVars;
    newScope->localScope = original->localScope;
    newScope->aliasTable = original->aliasTable;
    newScope->defers = original->defers;
    return newScope;
}

std::string typeToString(LLVMTypeRef type) {
    return std::string(LLVMPrintTypeToString(type));
}