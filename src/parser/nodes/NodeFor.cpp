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
#include "../../include/parser/ast.hpp"
#include <iostream>

NodeFor::NodeFor(std::vector<Node*> presets, Node* cond, std::vector<Node*> afters, NodeBlock* block, std::string funcName, int loc) {
    this->presets = std::vector<Node*>(presets);
    this->cond = cond;
    this->afters = std::vector<Node*>(afters);
    this->block = block;
    this->funcName = funcName;
    this->loc = loc;
}

Node* NodeFor::copy() {
    std::vector<Node*> presets;
    std::vector<Node*> afters;
    for(int i=0; i<this->presets.size(); i++) presets.push_back(this->presets[i]->copy());
    for(int i=0; i<this->afters.size(); i++) afters.push_back(this->afters[i]->copy());
    return new NodeFor(presets, (NodeBinary*)(this->cond->copy()), this->afters, (NodeBlock*)(this->block->copy()), this->funcName, this->loc);
}

Node* NodeFor::comptime() {return this;}
void NodeFor::check() {this->isChecked = true;}
Type* NodeFor::getType() {return new TypeVoid();}

LLVMValueRef NodeFor::generate() {
    for(int i=0; i<this->presets.size(); i++) {
        this->presets[i]->check();
        this->presets[i]->generate();
    }
    for(int i=0; i<this->afters.size(); i++) this->block->nodes.push_back(this->afters[i]);

    NodeWhile* nwhile = new NodeWhile(this->cond, this->block, this->loc, this->funcName);
    nwhile->check();
    LLVMValueRef result = nwhile->generate();

    for(int i=0; i<this->presets.size(); i++) {
        if(instanceof<NodeVar>(this->presets[i])) {
            NodeVar* nvar = (NodeVar*)this->presets[i];
            currScope->remove(nvar->name);
        }
    }

    return result;
}

bool NodeFor::isReleased(std::string varName) {
    if(instanceof<NodeBlock>(this->block)) {
        NodeBlock* nblock = (NodeBlock*)this->block;
        for(int i=0; i<nblock->nodes.size(); i++) {
            if(instanceof<NodeUnary>(nblock->nodes[i]) && ((NodeUnary*)nblock->nodes[i])->type == TokType::Destructor
            && instanceof<NodeIden>(((NodeUnary*)nblock->nodes[i])->base) && ((NodeIden*)((NodeUnary*)nblock->nodes[i])->base)->name == varName) return true;
            if(instanceof<NodeWhile>(nblock->nodes[i])) {
                if(((NodeWhile*)nblock->nodes[i])->isReleased(varName)) return true;
            }
            if(instanceof<NodeFor>(nblock->nodes[i])) {
                if(((NodeFor*)nblock->nodes[i])->isReleased(varName)) return true;
            }
            if(instanceof<NodeForeach>(nblock->nodes[i])) {
                if(((NodeForeach*)nblock->nodes[i])->isReleased(varName)) return true;
            }
        }
    }
    return false;
}