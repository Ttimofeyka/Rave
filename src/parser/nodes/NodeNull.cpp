/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "../../include/parser/nodes/NodeNull.hpp"
#include "../../include/utils.hpp"
#include "../../include/parser/ast.hpp"
#include <iostream>

NodeNull::NodeNull(Type* type, long loc) {this->type = type; this->loc = loc;}
Type* NodeNull::getType() {return (this->type == nullptr ? new TypePointer(new TypeVoid()) : this->type);}

LLVMValueRef NodeNull::generate() {
    if(this->type != nullptr) return LLVMConstNull(generator->genType(this->type, this->loc));
    // TODO: fix for lType
    return LLVMConstNull(LLVMPointerType(LLVMInt8TypeInContext(generator->context), 0));
}

void NodeNull::check() {this->isChecked = true;}
Node* NodeNull::comptime() {return this;}
Node* NodeNull::copy() {return new NodeNull(this->type, this->loc);}