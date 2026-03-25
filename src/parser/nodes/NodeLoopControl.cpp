/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "../../include/parser/nodes/NodeLoopControl.hpp"
#include "../../include/parser/ast.hpp"
#include "../../include/parser/nodes/NodeRet.hpp"
#include "../../include/debug.hpp"
#include <string>

NodeLoopControl::NodeLoopControl(LoopControlKind kind, int loc) : kind(kind), loc(loc) {}

void NodeLoopControl::check() {
    isChecked = true;
}

Type* NodeLoopControl::getType() {
    return typeVoid;
}

Node* NodeLoopControl::comptime() {
    return this;
}

Node* NodeLoopControl::copy() {
    return new NodeLoopControl(kind, loc);
}

int NodeLoopControl::getWhileLoop() {
    if (generator->activeLoops.empty()) return -1;

    int i = generator->activeLoops.size() - 1;

    // Find the enclosing loop (skip past if statements)
    while ((i >= 0) && generator->activeLoops[i].isIf) i -= 1;
    if (i >= 0 && generator->activeLoops[i].isIf) i = -1;

    return i;
}

RaveValue NodeLoopControl::generate() {
    int id = getWhileLoop();

    const char* keyword = (kind == LoopControlKind::Break) ? "break" : "continue";

    DEBUG_LOG_LOC(Debug::Category::CodeGen, std::string("LoopControl: ") + keyword, loc);

    if (id == -1) {
        generator->error(
            std::string("attempt to call \033[1m") + keyword + "\033[22m out of the loop!",
            this->loc
        );
    }

    if (kind == LoopControlKind::Break) {
        generator->activeLoops[generator->activeLoops.size() - 1].hasEnd = true;
        LLVMBuildBr(generator->builder, generator->activeLoops[id].end);
    } else {
        // Continue: branch to loop start
        LLVMBuildBr(generator->builder, generator->activeLoops[id].start);
        generator->activeLoops[id].hasEnd = true;
    }

    return {};
}