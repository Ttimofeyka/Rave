/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "../../include/parser/nodes/NodeIden.hpp"
#include "../../include/parser/nodes/NodeFunc.hpp"
#include "../../include/parser/nodes/NodeVar.hpp"
#include "../../include/parser/nodes/NodeGet.hpp"
#include "../../include/utils.hpp"
#include "../../include/parser/ast.hpp"

NodeIden::NodeIden(std::string name, int loc) {
    this->name = name;
    this->loc = loc;
}

NodeIden::NodeIden(std::string name, int loc, bool isMustBePtr) {
    this->name = name;
    this->loc = loc;
    this->isMustBePtr = isMustBePtr;
}

Node* NodeIden::copy() { return new NodeIden(name, loc, isMustBePtr); }

void NodeIden::check() { isChecked = true; }

Type* getIdenType(std::string idenName, int loc) {
    if (AST::aliasTable.find(idenName) != AST::aliasTable.end()) return AST::aliasTable[idenName]->getType();
    if (AST::funcTable.find(idenName) != AST::funcTable.end()) return AST::funcTable[idenName]->getType();
    if (!currScope->has(idenName) && !currScope->hasAtThis(idenName)) {
        if (generator->toReplace.find(idenName) != generator->toReplace.end()) return getIdenType(generator->toReplace[idenName]->toString(), loc);

        generator->error("unknown identifier \033[1m" + idenName + "\033[22m!", loc);
        return nullptr;
    }

    return currScope->getVar(idenName, loc)->getType();
}

Type* NodeIden::getType() {
    return getIdenType(name, loc);
}

Node* NodeIden::comptime() {
    if (generator != nullptr && generator->toReplaceValues.find(name) != generator->toReplaceValues.end()) return generator->toReplaceValues[name]->comptime();

    if (AST::aliasTable.find(name) == AST::aliasTable.end()) {
        generator->error("unknown alias \033[1m" + name + "\033[22m!", loc);
        return nullptr;
    }

    return AST::aliasTable[name];
}

RaveValue NodeIden::generate() {
    if (AST::aliasTable.find(name) != AST::aliasTable.end()) return AST::aliasTable[name]->generate();

    if (generator->globals.find(name) != generator->globals.end()) {
        AST::varTable[name]->isUsed = true;

        if (isMustBePtr) return generator->globals[name];
        return LLVM::load(generator->globals[name], "NodeIden_load", loc);
    }

    if (currScope != nullptr) {
        if (currScope->has(name) || currScope->hasAtThis(name)) {
            if (currScope->localVars.find(name) != currScope->localVars.end()) currScope->localVars[name]->isUsed = true;

            if (isMustBePtr) return currScope->getWithoutLoad(name, loc);
            return currScope->get(name, loc);
        }
    }

    if (AST::funcTable.find(name) != AST::funcTable.end()) {
        if (generator->functions.find(name) == generator->functions.end()) AST::funcTable[name]->generate();

        generator->addAttr("noinline", LLVMAttributeFunctionIndex, generator->functions[name].value, loc);
        return generator->functions[name];
    }
    
    generator->error("unknown identifier \033[1m" + name + "\033[22m!", loc);
    return {};
}