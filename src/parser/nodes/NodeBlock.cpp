/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "../../include/parser/nodes/NodeBlock.hpp"
#include "../../include/parser/nodes/NodeWhile.hpp"
#include "../../include/parser/nodes/NodeFor.hpp"
#include "../../include/parser/nodes/NodeForeach.hpp"
#include "../../include/parser/nodes/NodeIf.hpp"
#include "../../include/parser/nodes/NodeVar.hpp"
#include "../../include/parser/Types.hpp"
#include "../../include/utils.hpp"

NodeBlock::NodeBlock(std::vector<Node*> nodes) {
    this->nodes = std::vector<Node*>(nodes);
}

Node* NodeBlock::copy() {
    std::vector<Node*> newNodes;
    for(int i=0; i<this->nodes.size(); i++) {
        if(this->nodes[i] != nullptr) newNodes.push_back(this->nodes[i]->copy());
    }
    return new NodeBlock(newNodes);
}

void NodeBlock::check() {
    bool oldCheck = this->isChecked;
    this->isChecked = true;
    if(!oldCheck) for(int i=0; i<this->nodes.size(); i++) {
        if(this->nodes[i] != nullptr) this->nodes[i]->check();
    }
}

void NodeBlock::optimize() {
    for(Node* nd: this->nodes) {
        NodeVar *ndvar = dynamic_cast<NodeVar*>(nd);
        if(ndvar == nullptr) nd->optimize();
        else if(!ndvar->isGlobal && !ndvar->isUsed) generator->warning("unused variable \033[1m" + ndvar->name + "\033[22m!", ndvar->loc);
    }
}

Node* NodeBlock::comptime() {return this;}
Type* NodeBlock::getType() {return typeVoid;}

RaveValue NodeBlock::generate() {
    for(Node* nd: this->nodes) if(nd != nullptr) nd->generate();
    return {};
}

NodeBlock::~NodeBlock() {
    for(Node* nd: this->nodes) if(nd != nullptr) delete nd;
}