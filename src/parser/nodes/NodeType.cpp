/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "../../include/parser/nodes/NodeType.hpp"

NodeType::NodeType(Type* type, long loc) {
    this->type = type;
    this->loc = loc;
}

Type* NodeType::getType() {return this->type;}
void NodeType::check() {this->isChecked = true;}
LLVMValueRef NodeType::generate() {return nullptr;}
Node* NodeType::comptime() {return this;}
Node* NodeType::copy() {return new NodeType(this->type->copy(), this->loc);}