/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "../../include/parser/nodes/NodeIden.hpp"
#include "../../include/parser/ast.hpp"
#include "../../include/parser/nodes/NodeFunc.hpp"
#include "../../include/parser/nodes/NodeVar.hpp"
#include "../../include/parser/nodes/NodeGet.hpp"

NodeIden::NodeIden(std::string name, int loc, bool isMustBePtr) 
    : name(name), loc(loc), isMustBePtr(isMustBePtr) {}

NodeIden::NodeIden(std::string name, int loc) : name(name), loc(loc), isMustBePtr(false) {}

Node* NodeIden::copy() { return new NodeIden(name, loc, isMustBePtr); }

void NodeIden::check() { isChecked = true; }

Type* getIdenType(std::string idenName, int loc) {
    if (auto it = AST::aliasTable.find(idenName); it != AST::aliasTable.end()) return it->second->getType();
    if (auto it = AST::funcTable.find(idenName); it != AST::funcTable.end()) return it->second->getType();

    if (!currScope->has(idenName) && !currScope->hasAtThis(idenName)) {
        if (auto it = generator->toReplace.find(idenName); it != generator->toReplace.end()) 
            return getIdenType(it->second->toString(), loc);

        generator->error("unknown identifier \033[1m" + idenName + "\033[22m!", loc);
        return nullptr;
    }

    return currScope->getVar(idenName, loc)->getType();
}

Type* NodeIden::getType() { return getIdenType(name, loc); }

Node* NodeIden::comptime() {
    if (auto it = generator->toReplaceValues.find(name); it != generator->toReplaceValues.end()) 
        return it->second->comptime();

    auto it = AST::aliasTable.find(name);
    if (it == AST::aliasTable.end()) {
        generator->error("unknown alias \033[1m" + name + "\033[22m!", loc);
        return nullptr;
    }

    return it->second;
}

RaveValue NodeIden::generate() {
    if (auto it = AST::aliasTable.find(name); it != AST::aliasTable.end()) return it->second->generate();

    if (auto it = generator->globals.find(name); it != generator->globals.end()) {
        AST::varTable[name]->isUsed = true;
        return isMustBePtr ? it->second : LLVM::load(it->second, "NodeIden_load", loc);
    }

    if (currScope && (currScope->has(name) || currScope->hasAtThis(name))) {
        if (auto it = currScope->localVars.find(name); it != currScope->localVars.end()) 
            it->second->isUsed = true;
        return isMustBePtr ? currScope->getWithoutLoad(name, loc) : currScope->get(name, loc);
    }

    if (auto it = AST::funcTable.find(name); it != AST::funcTable.end()) {
        if (generator->functions.find(name) == generator->functions.end()) it->second->generate();
        generator->addAttr("noinline", LLVMAttributeFunctionIndex, generator->functions[name].value, loc);
        return generator->functions[name];
    }

    generator->error("unknown identifier \033[1m" + name + "\033[22m!", loc);
    return {};
}
