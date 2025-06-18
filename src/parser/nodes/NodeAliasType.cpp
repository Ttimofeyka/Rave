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

NodeAliasType::NodeAliasType(std::string name, Type* value, int loc) {
    this->name = name;
    this->origName = name;
    this->value = value;
    this->loc = loc;
}

Type* NodeAliasType::getType() {return this->value;}
RaveValue NodeAliasType::generate() {return {};}
Node* NodeAliasType::comptime() {return new NodeType(this->value, this->loc);}
Node* NodeAliasType::copy() {return new NodeAliasType(this->name, this->value->copy(), this->loc);}

void NodeAliasType::check() {
    if(isChecked) return;
    isChecked = true;

    while(AST::aliasTable.find(name) != AST::aliasTable.end()) name = ((NodeIden*)AST::aliasTable[name])->name;

    if(namespacesNames.size() > 0) name = namespacesToString(namespacesNames, name);
    AST::aliasTypes[name] = value;
}

NodeAliasType::~NodeAliasType() {
    if(this->value != nullptr && !instanceof<TypeBasic>(this->value) && !instanceof<TypeVoid>(this->value)) delete this->value;
}