/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "../../include/parser/nodes/NodeSizeof.hpp"
#include "../../include/parser/nodes/NodeInt.hpp"
#include "../../include/parser/nodes/NodeString.hpp"
#include "../../include/parser/nodes/NodeType.hpp"

NodeSizeof::NodeSizeof(Node* value, long loc) {
    this->value = value;
    this->loc = loc;
}

Type* NodeSizeof::getType() {return new TypeBasic(BasicType::Int);}
Node* NodeSizeof::comptime() {return this;}
Node* NodeSizeof::copy() {return new NodeSizeof(this->value->copy(), this->loc);}
void NodeSizeof::check() {this->isChecked = true;}

LLVMValueRef NodeSizeof::generate() {
    if(instanceof<NodeType>(this->value)) return LLVMBuildIntCast(generator->builder, LLVMSizeOf(generator->genType(((NodeType*)this->value)->type,loc)), LLVMInt32TypeInContext(generator->context), "icast");
    return LLVMSizeOf(LLVMTypeOf(this->value->generate()));
}