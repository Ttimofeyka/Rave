/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "../../include/parser/nodes/Node.hpp"
#include <iostream>

class Type;

LLVMValueRef Node::generate() {
    return nullptr;
}

void Node::check() {this->isChecked = true;}
Node* Node::comptime() {return this;}
Type* Node::getType() {return nullptr;}
Node* Node::copy() {return nullptr;}