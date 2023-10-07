/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "../../include/parser/nodes/NodeFor.hpp"
#include "../../include/parser/nodes/NodeWhile.hpp"
#include "../../include/parser/nodes/NodeBinary.hpp"
#include "../../include/utils.hpp"
#include "../../include/parser/ast.hpp"

NodeFor::NodeFor(std::vector<Node*> presets, NodeBinary* cond, std::vector<Node*> afters, NodeBlock* block, std::string funcName, long loc) {
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
    return new NodeFor(presets, (NodeBinary*)this->cond->copy(), this->afters, (NodeBlock*)this->block->copy(), this->funcName, this->loc);
}

Node* NodeFor::comptime() {return nullptr;}
void NodeFor::check() {this->isChecked = true;}
Type* NodeFor::getType() {return new TypeVoid();}

LLVMValueRef NodeFor::generate() {
    auto oldLocalVars = std::map<std::string, NodeVar*>(currScope->localVars);
    auto oldLocalScope = std::map<std::string, LLVMValueRef>(currScope->localScope);

    for(int i=0; i<this->presets.size(); i++) {
        this->presets[i]->check();
        this->presets[i]->generate();
    }
    for(int i=0; i<this->afters.size(); i++) this->block->nodes.push_back(this->afters[i]);

    LLVMValueRef toret = (new NodeWhile(this->cond, this->block, this->loc, this->funcName))->generate();

    currScope->localVars = oldLocalVars;
    currScope->localScope = oldLocalScope;

    return toret;
}