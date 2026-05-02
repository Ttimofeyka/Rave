/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at htypep://mozilla.org/MPL/2.0/.
*/

#include "../../include/parser/nodes/NodeDefer.hpp"
#include <llvm-c/Core.h>
#include "../../include/parser/nodes/NodeRet.hpp"

#include <iostream>

void Defer::emit(Loop& loop) {
    for (auto it = loop.defers.rbegin(); it != loop.defers.rend(); ++it) {
        (*it)->generate();
    }
    loop.defers.clear();
}

void Defer::emit(Scope* scope) {
    for (auto it = scope->defers.rbegin(); it != scope->defers.rend(); ++it) {
        (*it)->generate();
    }
    scope->defers.clear();
}

void Defer::emitAll() {
    // Emit block-scope defers from innermost to outermost.
    // Use copies because the normal execution path may also need to emit them.
    std::vector<Node*> copies;
    for (int i = generator->activeLoops.size() - 1; i >= 0; --i) {
        auto& loop = generator->activeLoops[i];
        for (auto dit = loop.defers.rbegin(); dit != loop.defers.rend(); ++dit) {
            Node* copy = (*dit)->copy();
            copies.push_back(copy);
            copy->generate();
        }
    }
    for (Node* n : copies) delete n;
    // Function-scope defers are NOT emitted here; they run at fnEnd
}

void Defer::make(Node* value, bool isFunctionScope) {
    if (!isFunctionScope && !generator->activeLoops.empty()) {
        auto& loop = generator->activeLoops[generator->activeLoops.size() - 1];
        if (loop.isIf) {
            // For if/else blocks, collect defer for emission at block end
            loop.defers.push_back(value);
            return;
        }
        // For loops (while/for), emit immediately to end block (existing behavior)
        LLVMBasicBlockRef targetBlock = loop.end;
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
    else if (currScope && currScope->fnEnd) {
        // Function-scope defer: collect for emission at function exit
        currScope->defers.push_back(value);
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
