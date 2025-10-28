/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "../../include/parser/nodes/NodeNull.hpp"
#include "../../include/utils.hpp"
#include "../../include/parser/ast.hpp"
#include "../../include/parser/nodes/NodeCast.hpp"
#include <iostream>

NodeNull::NodeNull(Type* type, int loc) : type(type), loc(loc) {}

Type* NodeNull::getType() { return (!type ? new TypePointer(typeVoid) : type); }

RaveValue NodeNull::generate() {
    if (type) return { LLVMConstNull(generator->genType(type, loc)), type };
    return { LLVMConstNull(LLVMPointerType(LLVMInt8TypeInContext(generator->context), 0)), new TypePointer(typeVoid) };
}

void NodeNull::check() { isChecked = true; }

Node* NodeNull::comptime() { return this; }

Node* NodeNull::copy() { return new NodeNull(type, loc); }

NodeNull::~NodeNull() {
    if (type && !instanceof<TypeBasic>(type) && !instanceof<TypeVoid>(type)) delete type;
}
