/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "../../include/parser/ast.hpp"
#include <vector>
#include <string>
#include "../../include/parser/nodes/NodePtoi.hpp"
#include "../../include/parser/ast.hpp"

NodePtoi::NodePtoi(Node* value, int loc) {this->value = value; this->loc = loc;}

Type* NodePtoi::getType() {return basicTypes[pointerSize == 64 ? BasicType::Long : pointerSize == 32 ? BasicType::Int : BasicType::Short];}
Node* NodePtoi::comptime() {return this;}
Node* NodePtoi::copy() {return new NodePtoi(this->value->copy(), this->loc);}

void NodePtoi::check() {
    bool oldCheck = this->isChecked;
    this->isChecked = true;
    if(!oldCheck) this->value->check();
}

RaveValue NodePtoi::generate() {
    LLVMTypeRef pointerType = nullptr;

    if(pointerSize == 64) pointerType = LLVMInt64TypeInContext(generator->context);
    else if(pointerSize == 32) pointerType = LLVMInt32TypeInContext(generator->context);
    else pointerType == LLVMInt16TypeInContext(generator->context);

    return {LLVMBuildPtrToInt(generator->builder, this->value->generate().value, pointerType, "ptoi"), basicTypes[pointerSize == 64 ? BasicType::Long : pointerSize == 32 ? BasicType::Int : BasicType::Short]};
}

NodePtoi::~NodePtoi() {
    if(this->value != nullptr) delete this->value;
}