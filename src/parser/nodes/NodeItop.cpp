/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "../../include/parser/ast.hpp"
#include <vector>
#include <string>
#include "../../include/parser/nodes/NodeItop.hpp"
#include "../../include/parser/ast.hpp"

NodeItop::NodeItop(Node* value, Type* type, int loc) {
    this->value = value;
    this->type = type;
    this->loc = loc;
}

Type* NodeItop::getType() {return this->type->copy();}
Node* NodeItop::comptime() {return this;}
Node* NodeItop::copy() {return new NodeItop(this->value->copy(), this->type->copy(), this->loc);}

void NodeItop::check() {
    bool oldCheck = this->isChecked;
    this->isChecked = true;
    if(!oldCheck) this->value->check();
}

RaveValue NodeItop::generate() {
    RaveValue _int = this->value->generate();
    return {LLVMBuildIntToPtr(generator->builder, _int.value, generator->genType(this->type, this->loc), "itop"), this->type};
}