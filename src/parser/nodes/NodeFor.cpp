/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "../../include/parser/nodes/NodeFor.hpp"
#include "../../include/parser/nodes/NodeWhile.hpp"
#include "../../include/parser/nodes/NodeIden.hpp"
#include "../../include/parser/nodes/NodeUnary.hpp"
#include "../../include/parser/nodes/NodeBinary.hpp"
#include "../../include/parser/nodes/NodeVar.hpp"
#include "../../include/utils.hpp"
#include "../../include/parser/nodes/NodeBlock.hpp"
#include "../../include/parser/nodes/NodeForeach.hpp"
#include "../../include/parser/nodes/NodeIf.hpp"
#include "../../include/parser/ast.hpp"
#include <iostream>

NodeFor::NodeFor(std::vector<Node*> presets, Node* cond, std::vector<Node*> afters, NodeBlock* block, int loc) {
    this->presets = std::vector<Node*>(presets);
    this->cond = cond;
    this->afters = std::vector<Node*>(afters);
    this->block = block;
    this->loc = loc;
}

NodeFor::~NodeFor() {
    for(int i=0; i<presets.size(); i++) if(presets[i] != nullptr) delete presets[i];
    if(cond != nullptr) delete cond;
    if(block != nullptr) delete block;
    for(int i=0; i<afters.size(); i++) if(afters[i] != nullptr) delete afters[i];
}

Node* NodeFor::copy() {
    std::vector<Node*> presets;
    std::vector<Node*> afters;
    for(int i=0; i<this->presets.size(); i++) presets.push_back(this->presets[i]->copy());
    for(int i=0; i<this->afters.size(); i++) afters.push_back(this->afters[i]->copy());
    return new NodeFor(presets, (NodeBinary*)(this->cond->copy()), this->afters, (NodeBlock*)(this->block->copy()), this->loc);
}

Node* NodeFor::comptime() {return this;}
void NodeFor::check() {isChecked = true;}
Type* NodeFor::getType() {return typeVoid;}

RaveValue NodeFor::generate() {
    for(int i=0; i<this->presets.size(); i++) {
        this->presets[i]->check();
        this->presets[i]->generate();
    }

    for(int i=0; i<this->afters.size(); i++) this->block->nodes.push_back(this->afters[i]);

    NodeWhile* nwhile = new NodeWhile(this->cond, this->block, this->loc);
    nwhile->check();
    RaveValue result = nwhile->generate();

    for(int i=0; i<this->presets.size(); i++) {
        if(instanceof<NodeVar>(this->presets[i])) {
            NodeVar* nvar = (NodeVar*)this->presets[i];
            currScope->remove(nvar->name);
        }
    }

    return result;
}

void NodeFor::optimize() {
    block->optimize();
}