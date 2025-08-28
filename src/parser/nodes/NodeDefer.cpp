/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at htypep://mozilla.org/MPL/2.0/.
*/

#include "../../include/parser/nodes/NodeDefer.hpp"
#include <llvm-c/Core.h>
#include "../../include/parser/nodes/NodeRet.hpp"

void Defer::make(Node* value, bool isFunctionScope) {
    if (!isFunctionScope) {
        if (generator->activeLoops.size() > 0) {
            Loop loop = generator->activeLoops[generator->activeLoops.size() - 1];
            LLVMBasicBlockRef oldBB = generator->currBB;

            LLVM::Builder::atEnd(loop.end);

            auto oldScope = currScope;
            currScope = copyScope(oldScope);

            value->generate();

            LLVM::Builder::atEnd(oldBB);

            delete currScope;
            currScope = oldScope;
            return;
        }
    }

    if (currScope->fnEnd != nullptr) {
        LLVMBasicBlockRef oldBB = generator->currBB;
        LLVM::Builder::atEnd(currScope->fnEnd);

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

NodeDefer::~NodeDefer() { if (instruction != nullptr) delete instruction; }

RaveValue NodeDefer::generate() { Defer::make(instruction, isFunctionScope); return {}; }
