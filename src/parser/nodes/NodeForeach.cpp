/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "../../include/parser/nodes/NodeForeach.hpp"
#include "../../include/parser/nodes/NodeWhile.hpp"
#include "../../include/parser/nodes/NodeFor.hpp"
#include "../../include/parser/nodes/NodeIden.hpp"
#include "../../include/parser/nodes/NodeUnary.hpp"
#include "../../include/parser/nodes/NodeBinary.hpp"
#include "../../include/parser/nodes/NodeVar.hpp"
#include "../../include/utils.hpp"
#include "../../include/parser/nodes/NodeBlock.hpp"
#include "../../include/parser/ast.hpp"
#include "../../include/parser/nodes/NodeStruct.hpp"
#include "../../include/parser/nodes/NodeGet.hpp"
#include "../../include/parser/nodes/NodeIndex.hpp"
#include "../../include/parser/nodes/NodeInt.hpp"
#include "../../include/parser/nodes/NodeIf.hpp"
#include <iostream>

NodeForeach::NodeForeach(NodeIden* elName, Node* varData, Node* varLength, NodeBlock* block, int loc) {
    this->elName = elName;
    this->varData = varData;
    this->varLength = varLength;
    this->block = block;
    this->loc = loc;
}

NodeForeach::~NodeForeach() {
    if (elName != nullptr) delete elName;
    if (varData != nullptr) delete varData;
    if (varLength != nullptr) delete varLength;
    if (block != nullptr) delete block;
}

Node* NodeForeach::copy() {
    return new NodeForeach((NodeIden*)elName->copy(), varData->copy(), (varLength != nullptr ? varLength->copy() : nullptr), (NodeBlock*)block->copy(), loc);
}

Node* NodeForeach::comptime() { return nullptr; }

void NodeForeach::check() { isChecked = true; }

Type* NodeForeach::getType() { return typeVoid; }

RaveValue NodeForeach::generate() {
    if (varLength == nullptr) {
        if (instanceof<NodeIden>(varData)) {
            NodeIden* niVarData = (NodeIden*)varData;
            if (instanceof<TypeStruct>(niVarData->getType())) {
                TypeStruct* ts = (TypeStruct*)niVarData->getType();
                if (AST::structTable.find(ts->name) != AST::structTable.end()) {
                    NodeStruct* ns = (NodeStruct*)AST::structTable[ts->name];
                    if (ns->dataVar == "" || ns->lengthVar == "") {
                        generator->error("structure \033[1m" + ts->name + "\033[22m does not contain the parameters data or length!", loc);
                        return {};
                    }
                    varLength = new NodeGet(niVarData, ns->lengthVar, false, loc);
                    varData = new NodeGet(niVarData, ns->dataVar, false, loc);
                }
            }
        }
    }

    NodeVar* nv = new NodeVar("__RAVE_FOREACH_N" + std::to_string(loc), new NodeInt(0), false, false, false, {}, loc, basicTypes[BasicType::Int], false, false, false);
    nv->check();
    nv->generate();

    NodeBlock* oldBlock = (NodeBlock*)block->copy();
    block->nodes = {
        new NodeVar(
            elName->name, new NodeIndex(varData,
                {new NodeIden("__RAVE_FOREACH_N" + std::to_string(loc), loc)
            }, loc),
        false, false, false, {}, loc, new TypeAuto(), false, false, false)
    };

    for (size_t i=0; i<oldBlock->nodes.size(); i++) block->nodes.push_back(oldBlock->nodes[i]);

    block->nodes.push_back(new NodeBinary(TokType::PluEqu, new NodeIden("__RAVE_FOREACH_N" + std::to_string(loc), loc), new NodeInt(1), loc));

    NodeWhile* nwhile = new NodeWhile(new NodeBinary(TokType::Less, new NodeIden("__RAVE_FOREACH_N" + std::to_string(loc), loc), varLength, loc), block, loc);
    nwhile->check();
    RaveValue result = nwhile->generate();

    currScope->remove("__RAVE_FOREACH_N" + std::to_string(loc));

    return {};
}

void NodeForeach::optimize() {
    for (Node *nd: block->nodes) {
        if (instanceof<NodeVar>(nd) && !((NodeVar*)nd)->isGlobal && !((NodeVar*)nd)->isUsed) generator->warning("unused variable \033[1m" + ((NodeVar*)nd)->name + "\033[22m!", loc);
        else nd->optimize();
    }
}