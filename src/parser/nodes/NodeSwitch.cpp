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

NodeSwitch::NodeSwitch(Node* expr, Node* _default, std::vector<std::pair<std::vector<Node*>, Node*>> statements, int loc) {
    this->expr = expr;
    this->statements = statements;
    this->loc = loc;
    this->_default = _default;
}

NodeSwitch::~NodeSwitch() {
    if(expr != nullptr) delete expr;
    if(_default != nullptr) delete _default;
    
    for(size_t i=0; i<statements.size(); i++) {
        for(int j=0; j<statements[i].first.size(); j++) {
            if(statements[i].first[j] != nullptr) delete statements[i].first[j];
        }

        statements[i].first.clear();

        if(statements[i].second != nullptr) delete statements[i].second;
    }
}

Type* NodeSwitch::getType() {return typeVoid;}

void NodeSwitch::check() {isChecked = true;}

RaveValue NodeSwitch::generate() {
    if(statements.empty()) {
        generator->error("at least 1 case is required in switch!", loc);
        return {};
    }

    std::vector<NodeIf*> ifVector;
    ifVector.reserve(statements.size());

    for(const auto& statement : statements) {
        NodeBinary* _equal = new NodeBinary(TokType::Equal, expr, statement.first[0], loc);

        for(int i=1; i<statement.first.size(); i++) _equal = new NodeBinary(TokType::Or, _equal, new NodeBinary(TokType::Equal, expr, statement.first[i], loc), loc);

        ifVector.push_back(new NodeIf(_equal, statement.second, nullptr, loc, false));
    }

    for(size_t i=0; i<ifVector.size() - 1; i++) ifVector[i]->_else = ifVector[i + 1];

    ifVector.back()->_else = _default;

    ifVector[0]->check();
    ifVector[0]->generate();

    return {};
}

Node* NodeSwitch::comptime() {return this;}

Node* NodeSwitch::copy() {
    return new NodeSwitch(
        this->expr->copy(), (this->_default == nullptr ? nullptr : this->_default->copy()),
        std::vector<std::pair<std::vector<Node*>, Node*>>(this->statements), this->loc
    );
}