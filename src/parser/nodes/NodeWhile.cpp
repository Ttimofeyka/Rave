/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "../../include/parser/nodes/NodeWhile.hpp"
#include "../../include/parser/nodes/NodeRet.hpp"
#include "../../include/parser/nodes/NodeBool.hpp"
#include "../../include/parser/nodes/NodeFunc.hpp"
#include "../../include/parser/nodes/NodeIf.hpp"
#include "../../include/parser/nodes/NodeFor.hpp"
#include "../../include/parser/nodes/NodeForeach.hpp"
#include "../../include/parser/nodes/NodeVar.hpp"
#include "../../include/parser/nodes/NodeUnary.hpp"
#include "../../include/parser/nodes/NodeIden.hpp"
#include "../../include/utils.hpp"

NodeWhile::NodeWhile(Node* cond, Node* body, int loc, std::string funcName) {
    this->cond = cond;
    this->body = body;
    this->loc = loc;
    this->funcName = funcName;
}

Type* NodeWhile::getType() {return new TypeVoid();}
Node* NodeWhile::comptime() {return this;}
Node* NodeWhile::copy() {return new NodeWhile(this->cond->copy(), this->body->copy(), this->loc, this->funcName);}

void NodeWhile::check() {
    bool oldCheck = this->isChecked;
    this->isChecked = true;

    if(this->body != nullptr && !oldCheck) {
        if(instanceof<NodeRet>(this->body)) ((NodeRet*)this->body)->parent = this->funcName;
        else if(instanceof<NodeBlock>(this->body)) {
            NodeBlock* nb = (NodeBlock*)this->body;
            for(int i=0; i<nb->nodes.size(); i++) {
                if(instanceof<NodeRet>(nb->nodes[i])) ((NodeRet*)nb->nodes[i])->parent = this->funcName;
                else if(instanceof<NodeIf>(nb->nodes[i])) ((NodeIf*)nb->nodes[i])->funcName = this->funcName;
                else if(instanceof<NodeWhile>(nb->nodes[i])) ((NodeWhile*)nb->nodes[i])->funcName = this->funcName;
                else if(instanceof<NodeFor>(nb->nodes[i])) ((NodeFor*)nb->nodes[i])->funcName = this->funcName;
            }
        }
    }
}

LLVMValueRef NodeWhile::generate() {
    auto& function = generator->functions[currScope->funcName];
    LLVMBasicBlockRef condBlock = LLVMAppendBasicBlock(function, "cond");
    LLVMBasicBlockRef whileBlock = LLVMAppendBasicBlock(function, "while");
    currScope->blockExit = LLVMAppendBasicBlock(function, "exit");

    LLVMBuildBr(generator->builder, condBlock);
    LLVMPositionBuilderAtEnd(generator->builder, condBlock);
    LLVMBuildCondBr(generator->builder, this->cond->generate(), whileBlock, currScope->blockExit);
    LLVMPositionBuilderAtEnd(generator->builder, whileBlock);

    size_t selfNumber = generator->activeLoops.size();
    generator->activeLoops[selfNumber] = {
        .isActive = true, .start = condBlock,
        .end = currScope->blockExit, .hasEnd = false,
        .isIf = false, .owner = this
    };

    auto oldScope = currScope;
    currScope = copyScope(currScope);

    generator->currBB = whileBlock;
    this->body->generate();
    
    if(!generator->activeLoops[selfNumber].hasEnd) LLVMBuildBr(generator->builder, condBlock);

    if(auto* nb = dynamic_cast<NodeBlock*>(this->body)) {
        for(const auto& node : nb->nodes) {
            if(auto* nodeVar = dynamic_cast<NodeVar*>(node)) {
                if(nodeVar->isAllocated) generator->warning("@detectMemoryLeaks: the variable '" + nodeVar->name + "' is not released!", nodeVar->loc);
            }
        }
    }

    LLVMPositionBuilderAtEnd(generator->builder, generator->activeLoops[selfNumber].end);
    generator->currBB = generator->activeLoops[selfNumber].end;
    generator->activeLoops.erase(selfNumber);

    delete currScope;
    currScope = oldScope;

    return nullptr;
}

bool NodeWhile::isReleased(std::string varName) {
    if(instanceof<NodeBlock>(this->body)) {
        NodeBlock* nblock = (NodeBlock*)this->body;
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