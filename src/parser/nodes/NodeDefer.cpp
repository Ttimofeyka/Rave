/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at htypep://mozilla.org/MPL/2.0/.
*/

#include "../../include/parser/nodes/NodeDefer.hpp"
#include <llvm-c/Core.h>
#include "../../include/parser/nodes/NodeRet.hpp"

void Defer::make(Node* value, bool isFunctionScope) {
    LLVMBasicBlockRef targetBlock = nullptr;

    // Determine target block: loop end for non-function scope, or function end
    if (!isFunctionScope && !generator->activeLoops.empty()) targetBlock = generator->activeLoops.rbegin()->second.end;
    else if (currScope->fnEnd) targetBlock = currScope->fnEnd;

    if (targetBlock) {
        LLVMBasicBlockRef oldBB = generator->currBB;
        LLVM::Builder::atEnd(targetBlock);

        auto oldScope = currScope;
        currScope = copyScope(oldScope);

        value->generate();

        LLVM::Builder::atEnd(oldBB);
        delete currScope;
        currScope = oldScope;
    }
}

NodeDefer::NodeDefer(int loc, Node* instruction, bool isFunctionScope) {
    this->instruction = instruction;
    this->loc = loc;
    this->isFunctionScope = isFunctionScope;
}

Type* NodeDefer::getType() { return typeVoid; }

void NodeDefer::check() { isChecked = true; }

Node* NodeDefer::comptime() { return this; }

Node* NodeDefer::copy() { return new NodeDefer(loc, instruction->copy(), isFunctionScope); }

NodeDefer::~NodeDefer() { if (instruction) delete instruction; }

RaveValue NodeDefer::generate() { Defer::make(instruction, isFunctionScope); return {}; }
