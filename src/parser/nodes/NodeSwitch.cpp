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

NodeSwitch::NodeSwitch(Node* expr, Node* _default, std::vector<std::pair<Node*, Node*>> statements, int loc) {
    this->expr = expr;
    this->statements = statements;
    this->loc = loc;
    this->_default = _default;
}

Type* NodeSwitch::getType() {return typeVoid;}

void NodeSwitch::check() {}

RaveValue NodeSwitch::generate() {
    if(statements.empty()) {
        generator->error("at least 1 case is required in switch!", loc);
        return {};
    }

    std::vector<NodeIf*> ifVector;
    ifVector.reserve(statements.size());

    for(const auto& statement : statements) {
        ifVector.push_back(new NodeIf(
            new NodeBinary(TokType::Equal, expr, statement.first, loc),
            statement.second, nullptr, loc, false
        ));
    }

    for(int i=0; i<ifVector.size() - 1; i++) ifVector[i]->_else = ifVector[i + 1];

    ifVector.back()->_else = _default;

    ifVector[0]->check();
    ifVector[0]->generate();

    return {};
}

Node* NodeSwitch::comptime() {return this;}

Node* NodeSwitch::copy() {
    return new NodeSwitch(
        this->expr->copy(), (this->_default == nullptr ? nullptr : this->_default->copy()),
        std::vector<std::pair<Node*, Node*>>(this->statements), this->loc
    );
}