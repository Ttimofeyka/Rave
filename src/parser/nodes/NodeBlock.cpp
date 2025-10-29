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

NodeBlock::NodeBlock(std::vector<Node*> nodes) : nodes(std::vector<Node*>(nodes)) {}

Node* NodeBlock::copy() {
    std::vector<Node*> newNodes;
    for (size_t i=0; i<nodes.size(); i++) { if (nodes[i]) newNodes.push_back(nodes[i]->copy()); }
    return new NodeBlock(newNodes);
}

void NodeBlock::check() {
    if (isChecked) return;
    isChecked = true;

    for (size_t i=0; i<nodes.size(); i++) { if (nodes[i]) nodes[i]->check(); }
}

void NodeBlock::optimize() {
    for (Node* nd: nodes) {
        NodeVar* ndvar = dynamic_cast<NodeVar*>(nd);
        if (!ndvar) nd->optimize();
        else if (!ndvar->isGlobal && !ndvar->isUsed) generator->warning("unused variable \033[1m" + ndvar->name + "\033[22m!", ndvar->loc);
    }
}

Node* NodeBlock::comptime() { return this; }

Type* NodeBlock::getType() { return typeVoid; }

RaveValue NodeBlock::generate() {
    for (Node* nd: nodes) { if (nd) nd->generate(); }
    return {};
}

NodeBlock::~NodeBlock() { for (Node* nd: nodes) { if (nd) delete nd; } }
