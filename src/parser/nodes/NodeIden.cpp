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

Node* NodeIden::copy() {return new NodeIden(this->name, this->loc, this->isMustBePtr);}
void NodeIden::check() {this->isChecked = true;}

Type* NodeIden::getType() {
    if(AST::aliasTable.find(this->name) != AST::aliasTable.end()) return AST::aliasTable[this->name]->getType();
    if(AST::funcTable.find(this->name) != AST::funcTable.end()) return AST::funcTable[this->name]->getType();
    if(!currScope->has(this->name) && !currScope->hasAtThis(this->name)) {
        if(generator->toReplace.find(this->name) != generator->toReplace.end()) {
            NodeIden* newNode = new NodeIden(generator->toReplace[this->name]->toString(), loc);
            newNode->isMustBePtr = this->isMustBePtr;
            return newNode->getType();
        }
        generator->error("unknown identifier '" + this->name + "'!", this->loc);
        return nullptr;
    }
    return currScope->getVar(this->name, this->loc)->getType();
}

Node* NodeIden::comptime() {
    if(AST::aliasTable.find(this->name) == AST::aliasTable.end()) {
        generator->error("unknown alias '" + name + "'!",loc);
        return nullptr;
    }
    return AST::aliasTable[this->name];
}

RaveValue NodeIden::generate() {
    if(AST::aliasTable.find(this->name) != AST::aliasTable.end()) return AST::aliasTable[this->name]->generate();

    if(generator->functions.find(this->name) != generator->functions.end()) {
        generator->addAttr("noinline", LLVMAttributeFunctionIndex, generator->functions[this->name].value, loc);
        return generator->functions[this->name];
    }

    if(generator->globals.find(name) != generator->globals.end()) {
        if(isMustBePtr) return generator->globals[name];
        return LLVM::load(generator->globals[name], "NodeIden_load", loc);
    }

    if(currScope != nullptr) {
        if(currScope->has(this->name) || currScope->hasAtThis(this->name)) {
            if(isMustBePtr) return currScope->getWithoutLoad(this->name, loc);
            return currScope->get(this->name, loc);
        }
    }
    
    generator->error("unknown identifier '" + this->name + "'!", loc);
    return {};
}