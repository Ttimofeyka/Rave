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

Node* NodeForeach::copy() {
    return new NodeForeach((NodeIden*)this->elName->copy(), this->varData->copy(), (varLength != nullptr ? this->varLength->copy() : nullptr), (NodeBlock*)this->block->copy(), this->loc);
}

Node* NodeForeach::comptime() {return nullptr;}
void NodeForeach::check() {this->isChecked = true;}
Type* NodeForeach::getType() {return new TypeVoid();}

RaveValue NodeForeach::generate() {
    if(this->varLength == nullptr) {
        if(instanceof<NodeIden>(varData)) {
            NodeIden* niVarData = (NodeIden*)varData;
            if(instanceof<TypeStruct>(niVarData->getType())) {
                TypeStruct* ts = (TypeStruct*)niVarData->getType();
                if(AST::structTable.find(ts->name) != AST::structTable.end()) {
                    NodeStruct* ns = (NodeStruct*)AST::structTable[ts->name];
                    if(ns->dataVar == "" || ns->lengthVar == "") {
                        generator->error("structure '" + ts->name + "' doesn't contain the parameters data or length!", this->loc);
                        return {};
                    }
                    this->varLength = new NodeGet(niVarData, ns->lengthVar, false, this->loc);
                    this->varData = new NodeGet(niVarData, ns->dataVar, false, this->loc);
                }
            }
        }
    }

    NodeVar* nv = new NodeVar("__RAVE_FOREACH_N" + std::to_string(this->loc), new NodeInt(0), false, false, false, {}, this->loc, new TypeBasic(BasicType::Int), false);
    nv->check();
    nv->generate();

    NodeBlock* oldBlock = (NodeBlock*)this->block->copy();
    this->block->nodes = {
        new NodeVar(
            elName->name, new NodeIndex(varData,
                {new NodeIden("__RAVE_FOREACH_N" + std::to_string(this->loc), this->loc)
            }, this->loc),
        false, false, false, {}, this->loc, new TypeAuto())
    };

    for(int i=0; i<oldBlock->nodes.size(); i++) this->block->nodes.push_back(oldBlock->nodes[i]);

    this->block->nodes.push_back(new NodeBinary(TokType::PluEqu, new NodeIden("__RAVE_FOREACH_N" + std::to_string(this->loc), this->loc), new NodeInt(1), this->loc));

    NodeWhile* nwhile = new NodeWhile(new NodeBinary(TokType::Less, new NodeIden("__RAVE_FOREACH_N" + std::to_string(this->loc), this->loc), varLength, this->loc), this->block, this->loc, currScope->funcName);
    nwhile->check();
    RaveValue result = nwhile->generate();

    currScope->remove("__RAVE_FOREACH_N" + std::to_string(this->loc));

    return {};
}

void NodeForeach::optimize() {
    for(int i=0; i<block->nodes.size(); i++) {
        if(instanceof<NodeIf>(block->nodes[i])) ((NodeIf*)block->nodes[i])->optimize();
        else if(instanceof<NodeFor>(block->nodes[i])) ((NodeFor*)block->nodes[i])->optimize();
        else if(instanceof<NodeWhile>(block->nodes[i])) ((NodeWhile*)block->nodes[i])->optimize();
        else if(instanceof<NodeForeach>(block->nodes[i])) ((NodeForeach*)block->nodes[i])->optimize();
        else if(instanceof<NodeVar>(block->nodes[i]) && !((NodeVar*)block->nodes[i])->isGlobal && !((NodeVar*)block->nodes[i])->isUsed) generator->warning("unused variable '" + ((NodeVar*)block->nodes[i])->name + "'!", loc);
    }
}