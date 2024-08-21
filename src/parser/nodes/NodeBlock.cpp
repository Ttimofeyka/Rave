/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "../../include/parser/nodes/NodeBlock.hpp"
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

Node* NodeBlock::comptime() {return this;}
Type* NodeBlock::getType() {return new TypeVoid();}
Type* NodeBlock::getLType() {return new TypeVoid();}

LLVMValueRef NodeBlock::generate() {
    for(int i=0; i<this->nodes.size(); i++) if(this->nodes[i] != nullptr) this->nodes[i]->generate();
    return nullptr;
}