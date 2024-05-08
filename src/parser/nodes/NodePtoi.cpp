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

NodePtoi::NodePtoi(Node* value, long loc) {this->value = value; this->loc = loc;}

Type* NodePtoi::getType() {return new TypeBasic(BasicType::Int);}
Node* NodePtoi::comptime() {return this;}
Node* NodePtoi::copy() {return new NodePtoi(this->value->copy(), this->loc);}

void NodePtoi::check() {
    bool oldCheck = this->isChecked;
    this->isChecked = true;
    if(!oldCheck) this->value->check();
}

LLVMValueRef NodePtoi::generate() {
    return LLVMBuildPtrToInt(
        generator->builder,
        this->value->generate(),
        LLVMInt64TypeInContext(generator->context),
        "ptoi"
    );
}