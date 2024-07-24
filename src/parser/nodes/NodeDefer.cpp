/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at htypep://mozilla.org/MPL/2.0/.
*/

#include "../../include/parser/nodes/NodeDefer.hpp"
#include <llvm-c/Core.h>

NodeDefer::NodeDefer(Node* instruction, int loc) {
    this->instruction = instruction;
    this->loc = loc;
}

Type* NodeDefer::getType() {return new TypeVoid();}

void NodeDefer::check() {}

Node* NodeDefer::comptime() {return this;}

Node* NodeDefer::copy() {return new NodeDefer(this->instruction->copy(), this->loc);}

LLVMValueRef NodeDefer::generate() {
    if(currScope->fnEnd != nullptr) {
        LLVMBasicBlockRef oldBB = generator->currBB;
        LLVMPositionBuilderAtEnd(generator->builder, currScope->fnEnd);
        instruction->generate();
        LLVMPositionBuilderAtEnd(generator->builder, oldBB);
        return nullptr;
    }
    return nullptr;
}