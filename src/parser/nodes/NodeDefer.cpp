/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at htypep://mozilla.org/MPL/2.0/.
*/

#include "../../include/parser/nodes/NodeDefer.hpp"
#include <llvm-c/Core.h>
#include "../../include/parser/nodes/NodeRet.hpp"

NodeDefer::NodeDefer(Node* instruction, int loc, bool isFunctionScope) {
    this->instruction = instruction;
    this->loc = loc;
    this->isFunctionScope = isFunctionScope;
}

Type* NodeDefer::getType() {return typeVoid;}

void NodeDefer::check() {isChecked = true;}

Node* NodeDefer::comptime() {return this;}

Node* NodeDefer::copy() {return new NodeDefer(this->instruction->copy(), this->loc, this->isFunctionScope);}

NodeDefer::~NodeDefer() {
    if (this->instruction != nullptr) delete this->instruction;
}

RaveValue NodeDefer::generate() {
    if (!isFunctionScope) {
        if (generator->activeLoops.size() > 0) {
            Loop loop = generator->activeLoops[generator->activeLoops.size() - 1];
            LLVMBasicBlockRef oldBB = generator->currBB;

            LLVM::Builder::atEnd(loop.end);

            auto oldScope = currScope;
            currScope = copyScope(oldScope);

            instruction->generate();

            LLVM::Builder::atEnd(oldBB);

            delete currScope;
            currScope = oldScope;
            return {};
        }
    }

    if (currScope->fnEnd != nullptr) {
        LLVMBasicBlockRef oldBB = generator->currBB;
        LLVM::Builder::atEnd(currScope->fnEnd);

        auto oldScope = currScope;
        currScope = copyScope(oldScope);

        instruction->generate();

        LLVM::Builder::atEnd(oldBB);
        delete currScope;
        currScope = oldScope;
    }

    return {};
}