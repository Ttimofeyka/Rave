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
#include <iostream>

NodeForeach::NodeForeach(NodeIden* elName, Node* varData, Node* varLength, NodeBlock* block, std::string funcName, long loc) {
    this->elName = elName;
    this->varData = varData;
    this->varLength = varLength;
    this->block = block;
    this->funcName = funcName;
    this->loc = loc;
}

Node* NodeForeach::copy() {
    return new NodeForeach((NodeIden*)this->elName->copy(), this->varData->copy(), this->varLength->copy(), (NodeBlock*)this->block->copy(), this->funcName, this->loc);
}

Node* NodeForeach::comptime() {return nullptr;}
void NodeForeach::check() {this->isChecked = true;}
Type* NodeForeach::getType() {return new TypeVoid();}

LLVMValueRef NodeForeach::generate() {
    if(this->varLength == nullptr) {
        if(instanceof<NodeIden>(varData)) {
            NodeIden* niVarData = (NodeIden*)varData;
            if(instanceof<TypeStruct>(niVarData->getType())) {
                TypeStruct* ts = (TypeStruct*)niVarData->getType();
                if(AST::structTable.find(ts->name) != AST::structTable.end()) {
                    NodeStruct* ns = (NodeStruct*)AST::structTable[ts->name];
                    if(ns->dataVar == "" || ns->lengthVar == "") {
                        generator->error("structure '"+ts->name+"' doesn't contain the parameters data or length!", this->loc);
                        return nullptr;
                    }
                    this->varLength = new NodeGet(niVarData, ns->lengthVar, false, this->loc);
                    this->varData = new NodeGet(niVarData, ns->dataVar, false, this->loc);
                }
            }
        }
    }

    if(instanceof<NodeGet>(this->varData)) {

    }

    NodeVar* nv = new NodeVar("__RAVE_FOREACH_N"+std::to_string(this->loc), new NodeInt(0), false, false, false, {}, this->loc, new TypeBasic(BasicType::Int), false);
    nv->check();
    nv->generate();

    NodeBlock* oldBlock = (NodeBlock*)this->block->copy();
    this->block->nodes = {
        new NodeVar(
            elName->name, new NodeIndex(varData,
                {new NodeIden("__RAVE_FOREACH_N"+std::to_string(this->loc), this->loc)
            }, this->loc),
        false, false, false, {}, this->loc, new TypeAuto())
    };

    for(int i=0; i<oldBlock->nodes.size(); i++) this->block->nodes.push_back(oldBlock->nodes[i]);

    this->block->nodes.push_back(new NodeBinary(TokType::PluEqu, new NodeIden("__RAVE_FOREACH_N"+std::to_string(this->loc), this->loc), new NodeInt(1), this->loc));

    NodeWhile* nwhile = new NodeWhile(new NodeBinary(TokType::Less, new NodeIden("__RAVE_FOREACH_N"+std::to_string(this->loc), this->loc), varLength, this->loc), this->block, this->loc, this->funcName);
    nwhile->check();
    LLVMValueRef result = nwhile->generate();

    currScope->remove("__RAVE_FOREACH_N"+std::to_string(this->loc));

    return nullptr;
}

bool NodeForeach::isReleased(std::string varName) {
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