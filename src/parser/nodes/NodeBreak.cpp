/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "../../include/parser/nodes/NodeBreak.hpp"
#include "../../include/utils.hpp"
#include "../../include/parser/ast.hpp"
#include "../../include/parser/nodes/NodeRet.hpp"
#include <string>

NodeBreak::NodeBreak(int loc) {this->loc = loc;}
void NodeBreak::check() {this->isChecked = true;}
Type* NodeBreak::getType() {return typeVoid;}
Node* NodeBreak::comptime() {return this;}
Node* NodeBreak::copy() {return new NodeBreak(this->loc);}

int NodeBreak::getWhileLoop() {
    int i = generator->activeLoops.size() - 1;
    while((i >= 0) && generator->activeLoops[i].isIf) i -= 1;
    if(generator->activeLoops[i].isIf) i = -1;
    return i;
}

RaveValue NodeBreak::generate() {
    if(generator->activeLoops.empty()) generator->error("attempt to call \033[1mbreak\033[22m out of the loop!", this->loc);

    int id = this->getWhileLoop();
    if(id == -1) generator->error("attempt to call \033[1mbreak\033[22m out of the loop!", this->loc);

    generator->activeLoops[generator->activeLoops.size() - 1].hasEnd = true;
    LLVMBuildBr(generator->builder, generator->activeLoops[id].end);

    return {};
}