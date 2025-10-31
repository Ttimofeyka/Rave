/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "../../include/parser/nodes/NodeCmpxchg.hpp"
#include "../../include/utils.hpp"
#include "../../include/parser/ast.hpp"

NodeCmpxchg::NodeCmpxchg(Node* ptr, Node* value1, Node* value2, int loc) : ptr(ptr), value1(value1), value2(value2), loc(loc) {}

NodeCmpxchg::~NodeCmpxchg() {
    if (ptr) delete ptr;
    if (value1) delete value1;
    if (value2) delete value2;
}

Node* NodeCmpxchg::copy() { return new NodeCmpxchg(ptr->copy(), value1->copy(), value2->copy(), loc); }

Type* NodeCmpxchg::getType() { return typeVoid; }

void NodeCmpxchg::check() { isChecked = true; }

Node* NodeCmpxchg::comptime() { return this; }

RaveValue NodeCmpxchg::generate() {
    return {LLVMBuildAtomicCmpXchg(
        generator->builder, ptr->generate().value, value1->generate().value,
        value2->generate().value, LLVMAtomicOrderingSequentiallyConsistent, LLVMAtomicOrderingSequentiallyConsistent, false
    ), ptr->getType()};
}
