/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "../../include/parser/nodes/NodeComptime.hpp"
#include "../../include/parser/nodes/NodeFunc.hpp"
#include "../../include/parser/nodes/NodeVar.hpp"
#include "../../include/parser/nodes/NodeNamespace.hpp"
#include "../../include/parser/nodes/NodeBuiltin.hpp"
#include "../../include/parser/nodes/NodeIf.hpp"
#include "../../include/parser/nodes/NodeStruct.hpp"
#include "../../include/utils.hpp"
#include "../../include/parser/ast.hpp"

NodeComptime::NodeComptime(Node* node) {
    this->node = node;
}

Node* NodeComptime::copy() {
    return new NodeComptime(this->node->copy());
}

void NodeComptime::check() {
    this->isChecked = true;
}

Type* NodeComptime::getType() {
    return this->node->getType();
}

Node* NodeComptime::comptime() {
    if(this->isImported) {
        if(instanceof<NodeIf>(this->node)) ((NodeIf*)this->node)->isImported = true;
        else if(instanceof<NodeFunc>(this->node)) ((NodeFunc*)this->node)->isExtern = true;
        else if(instanceof<NodeComptime>(this->node)) ((NodeComptime*)this->node)->isImported = true;
        else if(instanceof<NodeVar>(this->node)) ((NodeVar*)this->node)->isExtern = true;
        else if(instanceof<NodeNamespace>(this->node)) ((NodeNamespace*)this->node)->isImported = true;
        else if(instanceof<NodeBuiltin>(this->node)) ((NodeBuiltin*)this->node)->isImport = true;
        else if(instanceof<NodeStruct>(this->node)) ((NodeStruct*)this->node)->isImported = true;
        else if(instanceof<NodeBlock>(this->node)) {
            NodeBlock* block = (NodeBlock*)this->node;

            for(Node *nd: block->nodes) {
                if(instanceof<NodeIf>(nd)) ((NodeIf*)nd)->isImported = true;
                else if(instanceof<NodeFunc>(nd)) ((NodeFunc*)nd)->isExtern = true;
                else if(instanceof<NodeComptime>(nd)) ((NodeComptime*)nd)->isImported = true;
                else if(instanceof<NodeVar>(nd)) ((NodeVar*)nd)->isExtern = true;
                else if(instanceof<NodeNamespace>(nd)) ((NodeNamespace*)nd)->isImported = true;
                else if(instanceof<NodeBuiltin>(nd)) ((NodeBuiltin*)nd)->isImport = true;
                else if(instanceof<NodeStruct>(nd)) ((NodeStruct*)nd)->isImported = true;
            }
        }
    }

    return this->node->comptime();
}

RaveValue NodeComptime::generate() {
    this->comptime();
    return {};
}