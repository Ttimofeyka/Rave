/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "../../include/parser/nodes/Node.hpp"
#include <iostream>

class Type;

RaveValue Node::generate() {
    return {};
}

void Node::check() {isChecked = true;}
void Node::optimize() {}
Node* Node::comptime() {return this;}
Type* Node::getType() {return nullptr;}
Node* Node::copy() {return nullptr;}
Node::~Node() {}