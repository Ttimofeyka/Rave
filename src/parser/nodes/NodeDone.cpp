/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "../../include/parser/nodes/NodeDone.hpp"

NodeDone::NodeDone(LLVMValueRef value) {this->value = value;}
void NodeDone::check() {this->isChecked = true;}
LLVMValueRef NodeDone::generate() {return this->value;}
Type* NodeDone::getType() {return lTypeToType(LLVMTypeOf(this->value));}
Node* NodeDone::comptime() {return nullptr;}
Node* NodeDone::copy() {return new NodeDone(this->value);}