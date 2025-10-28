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

NodeItop::NodeItop(Node* value, Type* type, int loc) : value(value), type(type), loc(loc) {}

Type* NodeItop::getType() { return type->copy(); }

Node* NodeItop::comptime() { return this; }

Node* NodeItop::copy() { return new NodeItop(value->copy(), type->copy(), loc); }

void NodeItop::check() {
    if (isChecked) return;
    isChecked = true;

    value->check();
}

RaveValue NodeItop::generate() {
    return { LLVMBuildIntToPtr(generator->builder, value->generate().value, generator->genType(type, loc), "itop"), type };
}

NodeItop::~NodeItop() {
    if (type && !instanceof<TypeBasic>(type) && !instanceof<TypeVoid>(type)) delete type;
    if (value) delete value;
}
