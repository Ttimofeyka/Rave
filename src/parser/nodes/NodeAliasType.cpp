/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "../../include/parser/nodes/NodeAliasType.hpp"
#include "../../include/parser/nodes/NodeIden.hpp"
#include "../../include/parser/nodes/NodeType.hpp"
#include "../../include/utils.hpp"
#include "../../include/parser/ast.hpp"

NodeAliasType::NodeAliasType(std::string name, Type* value, long loc) {
    this->name = name;
    this->origName = name;
    this->value = value;
    this->loc = loc;
}

Type* NodeAliasType::getType() {return this->value;}
LLVMValueRef NodeAliasType::generate() {return nullptr;}
Node* NodeAliasType::comptime() {return new NodeType(this->value, this->loc);}
Node* NodeAliasType::copy() {return new NodeAliasType(this->name, this->value->copy(), this->loc);}

void NodeAliasType::check() {
    bool oldCheck = this->isChecked;
    this->isChecked = true;

    if(!oldCheck) {
        while(AST::aliasTable.find(this->name) != AST::aliasTable.end()) this->name = ((NodeIden*)AST::aliasTable[this->name])->name;
        if(this->namespacesNames.size() > 0) name = namespacesToString(this->namespacesNames, this->name);
        AST::aliasTypes[this->name] = this->value;
    }
}