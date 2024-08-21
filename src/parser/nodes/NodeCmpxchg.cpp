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

Node* NodeCmpxchg::copy() {return new NodeCmpxchg(this->ptr->copy(), this->value1->copy(), this->value2->copy(), this->loc);}
Type* NodeCmpxchg::getType() {return new TypeVoid();}
Type* NodeCmpxchg::getLType() {return new TypeVoid();}
void NodeCmpxchg::check() {this->isChecked = true;}
Node* NodeCmpxchg::comptime() {return nullptr;}

LLVMValueRef NodeCmpxchg::generate() {
    return LLVMBuildAtomicCmpXchg(generator->builder, this->ptr->generate(), this->value1->generate(), this->value2->generate(),
    LLVMAtomicOrderingSequentiallyConsistent, LLVMAtomicOrderingSequentiallyConsistent, false);
}