/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "../../include/parser/nodes/NodeSwitch.hpp"
#include "../../include/parser/nodes/NodeRet.hpp"
#include "../../include/parser/nodes/NodeBool.hpp"
#include "../../include/parser/nodes/NodeBinary.hpp"
#include "../../include/parser/nodes/NodeFunc.hpp"
#include "../../include/parser/nodes/NodeIf.hpp"
#include "../../include/utils.hpp"

NodeSwitch::NodeSwitch(Node* expr, Node* _default, std::vector<std::pair<Node*, Node*>> statements, long loc, std::string func) {
    this->expr = expr;
    this->statements = statements;
    this->loc = loc;
    this->funcName = func;
    this->_default = _default;
}

Type* NodeSwitch::getType() {return new TypeVoid();}

void NodeSwitch::check() {}

LLVMValueRef NodeSwitch::generate() {
    std::vector<NodeIf*> ifVector;

    for(int i=0; i<this->statements.size(); i++) {
        ifVector.push_back(new NodeIf(new NodeBinary(
            TokType::Equal, this->expr, this->statements[i].first, this->loc),
            this->statements[i].second, nullptr, this->loc, this->funcName, false
        ));
    }

    if(ifVector.size() == 0) {
        generator->error("at least 1 case is required in switch!", this->loc);
        return nullptr;
    }

    NodeIf* currIf = ifVector[0];
    NodeIf* origIf = currIf;

    for(int i=1; i<ifVector.size(); i++) {
        currIf->_else = ifVector[i];
        currIf = (NodeIf*)currIf->_else;
    }

    currIf->_else = this->_default;

    origIf->check();
    origIf->generate();

    return nullptr;
}

Node* NodeSwitch::comptime() {return this;}
Node* NodeSwitch::copy() {
    return new NodeSwitch(
        this->expr->copy(), (this->_default == nullptr ? nullptr : this->_default->copy()),
        std::vector<std::pair<Node*, Node*>>(this->statements), this->loc, this->funcName
    );
}