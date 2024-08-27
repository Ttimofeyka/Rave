/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "../../include/parser/nodes/NodeRet.hpp"
#include "../../include/utils.hpp"
#include "../../include/parser/ast.hpp"
#include "../../include/parser/nodes/NodeFunc.hpp"
#include "../../include/parser/nodes/NodeNull.hpp"
#include "../../include/parser/nodes/NodeLambda.hpp"
#include "../../include/parser/nodes/NodeIden.hpp"
#include "../../include/parser/nodes/NodeGet.hpp"
#include "../../include/parser/nodes/NodeIndex.hpp"
#include "../../include/parser/nodes/NodeVar.hpp"

namespace AST {
    extern std::map<std::string, NodeFunc*> funcTable;
}

NodeRet::NodeRet(Node* value, std::string parent, int loc) {
    this->value = (value == nullptr ? nullptr : value->copy());
    this->parent = parent;
    this->loc = loc;
}

Node* NodeRet::copy() {return new NodeRet(this->value->copy(), this->parent, this->loc);}
Type* NodeRet::getType() {return this->value->getType();}
Node* NodeRet::comptime() {return this;}

void NodeRet::check() {
    bool oldCheck = this->isChecked;
    this->isChecked = true;
    if(!oldCheck && AST::funcTable.find(this->parent) != AST::funcTable.end()) AST::funcTable[this->parent]->rets.push_back(this);
}

Loop NodeRet::getParentBlock(int n) {
    if(generator->activeLoops.size() == 0) return Loop{.isActive = false, .start = nullptr, .end = nullptr, .hasEnd = false, .isIf = false, .loopRets = std::vector<LoopReturn>()};
    return generator->activeLoops[generator->activeLoops.size()-1];
}

void NodeRet::setParentBlock(Loop value, int n) {
    if(generator->activeLoops.size() > 0) {
        if(n == -1) generator->activeLoops[generator->activeLoops.size()-1] = value;
        else generator->activeLoops[n] = value;
    }
}

RaveValue NodeRet::generate() {
    if(currScope == nullptr || !currScope->has("return")) return {};

    if(this->value == nullptr) this->value = new NodeNull(currScope->getVar("return", loc)->getType(), this->loc);
    
    RaveValue generated = value->generate();    
    RaveValue ptr = currScope->getWithoutLoad("return", loc);

    if(generated.type->toString() == ptr.type->toString()) generated = LLVM::load(generated, "NodeRet_load", loc);

    LLVMBuildStore(generator->builder, generated.value, ptr.value);

    currScope->funcHasRet = true;
    return {};
}