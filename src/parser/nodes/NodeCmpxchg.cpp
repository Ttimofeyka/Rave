/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "../../include/parser/nodes/NodeCmpxchg.hpp"
#include "../../include/utils.hpp"
#include "../../include/parser/ast.hpp"

NodeCmpxchg::NodeCmpxchg(Node* ptr, Node* value1, Node* value2, int loc) {
    this->ptr = ptr;
    this->value1 = value1;
    this->value2 = value2;
    this->loc = loc;
}

NodeCmpxchg::~NodeCmpxchg() {
    if (ptr != nullptr) delete ptr;
    if (value1 != nullptr) delete value1;
    if (value2 != nullptr) delete value2;
}

Node* NodeCmpxchg::copy() {return new NodeCmpxchg(this->ptr->copy(), this->value1->copy(), this->value2->copy(), this->loc);}
Type* NodeCmpxchg::getType() {return typeVoid;}
void NodeCmpxchg::check() {isChecked = true;}
Node* NodeCmpxchg::comptime() {return this;}

RaveValue NodeCmpxchg::generate() {
    return {LLVMBuildAtomicCmpXchg(
        generator->builder, this->ptr->generate().value, this->value1->generate().value,
        this->value2->generate().value, LLVMAtomicOrderingSequentiallyConsistent, LLVMAtomicOrderingSequentiallyConsistent, false
    ), this->ptr->getType()};
}