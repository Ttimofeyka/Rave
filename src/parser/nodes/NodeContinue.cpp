/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "../../include/parser/nodes/NodeContinue.hpp"
#include "../../include/utils.hpp"
#include "../../include/parser/ast.hpp"
#include "../../include/parser/nodes/NodeRet.hpp"
#include <string>

NodeContinue::NodeContinue(int loc) {this->loc = loc;}
void NodeContinue::check() {this->isChecked = true;}
Type* NodeContinue::getType() {return typeVoid;}
Node* NodeContinue::comptime() {return this;}
Node* NodeContinue::copy() {return new NodeContinue(this->loc);}

int NodeContinue::getWhileLoop() {
    int i = generator->activeLoops.size() - 1;
    while((i >= 0) && generator->activeLoops[i].isIf) i -= 1;
    if(generator->activeLoops[i].isIf) i = -1;
    return i;
}

RaveValue NodeContinue::generate() {
    if(generator->activeLoops.empty()) generator->error("attempt to call 'continue' out of the loop!", this->loc);
    int id = this->getWhileLoop();
    if(id == -1) generator->error("attempt to call 'continue' out of the loop!", this->loc);

    LLVMBuildBr(generator->builder, generator->activeLoops[id].start);

    return {};
}