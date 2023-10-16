/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "../../include/parser/nodes/NodeIf.hpp"
#include "../../include/parser/nodes/NodeRet.hpp"
#include "../../include/parser/nodes/NodeBool.hpp"
#include "../../include/parser/nodes/NodeFunc.hpp"
#include "../../include/parser/nodes/NodeWhile.hpp"
#include "../../include/parser/nodes/NodeFor.hpp"
#include "../../include/utils.hpp"

NodeIf::NodeIf(Node* cond, Node* body, Node* _else, long loc, std::string funcName, bool isStatic) {
    this->cond = cond;
    this->body = body;
    this->_else = _else;
    this->loc = loc;
    this->funcName = funcName;
    this->isStatic = isStatic;
}

Type* NodeIf::getType() {return new TypeVoid();}

void NodeIf::check() {
    bool oldCheck = this->isChecked;
    this->isChecked = true;

    if(!oldCheck) {
        if(this->body != nullptr) {
            if(instanceof<NodeRet>(this->body)) {
                ((NodeRet*)this->body)->parent = this->funcName;
                this->hasRets[0] = true;
            }
            else if(instanceof<NodeBlock>(this->body)) {
                NodeBlock* nb = (NodeBlock*)this->body;
                for(int i=0; i<nb->nodes.size(); i++) {
                    if(instanceof<NodeRet>(nb->nodes[i])) {
                        ((NodeRet*)nb->nodes[i])->parent = this->funcName;
                        this->hasRets[0] = true;
                    }
                    else if(instanceof<NodeIf>(nb->nodes[i])) ((NodeIf*)nb->nodes[i])->funcName = this->funcName;
                    else if(instanceof<NodeWhile>(nb->nodes[i])) ((NodeWhile*)nb->nodes[i])->funcName = this->funcName;
                    else if(instanceof<NodeFor>(nb->nodes[i])) ((NodeFor*)nb->nodes[i])->funcName = this->funcName;
                }
            }
        }

        if(this->_else != nullptr) {
            if(instanceof<NodeRet>(this->_else)) {
                ((NodeRet*)this->_else)->parent = this->funcName;
                this->hasRets[1] = true;
            }
            else if(instanceof<NodeBlock>(this->_else)) {
                NodeBlock* nb = (NodeBlock*)this->_else;
                for(int i=0; i<nb->nodes.size(); i++) {
                    if(instanceof<NodeRet>(nb->nodes[i])) {
                        ((NodeRet*)nb->nodes[i])->parent = this->funcName;
                        this->hasRets[1] = true;
                    }
                    else if(instanceof<NodeIf>(nb->nodes[i])) ((NodeIf*)nb->nodes[i])->funcName = this->funcName;
                    else if(instanceof<NodeWhile>(nb->nodes[i])) ((NodeWhile*)nb->nodes[i])->funcName = this->funcName;
                    else if(instanceof<NodeFor>(nb->nodes[i])) ((NodeFor*)nb->nodes[i])->funcName = this->funcName;
                }
            }
        }

        this->cond->check();
        if(this->body != nullptr) this->body->check();
        if(this->_else != nullptr) this->_else->check();
    }
}

LLVMValueRef NodeIf::generate() {
    if(isStatic) {
        this->comptime();
        return nullptr;
    }
    LLVMBasicBlockRef thenBlock = LLVMAppendBasicBlockInContext(generator->context, generator->functions[currScope->funcName], "then");
	LLVMBasicBlockRef elseBlock = LLVMAppendBasicBlockInContext(generator->context, generator->functions[currScope->funcName], "else");
	LLVMBasicBlockRef endBlock = LLVMAppendBasicBlockInContext(generator->context, generator->functions[currScope->funcName], "end");

    LLVMBuildCondBr(generator->builder, this->cond->generate(), thenBlock, elseBlock);

    int selfNum = generator->activeLoops.size();
    generator->activeLoops[selfNum] = Loop{.isActive = true, .start = thenBlock, .end = endBlock, .hasEnd = false, .isIf = true, .loopRets = std::vector<LoopReturn>()};

    LLVMPositionBuilderAtEnd(generator->builder, thenBlock);
    generator->currBB = thenBlock;
    if(this->body != nullptr) this->body->generate();
    if(!generator->activeLoops[selfNum].hasEnd) LLVMBuildBr(generator->builder, endBlock);

    if(generator->activeLoops[selfNum].loopRets.size() > 0) AST::funcTable[currScope->funcName]->rets.push_back(generator->activeLoops[selfNum].loopRets[0].ret);

    bool hasEnd1 = generator->activeLoops[selfNum].hasEnd;

    generator->activeLoops[selfNum] = Loop{.isActive = true, .start = elseBlock, .end = endBlock, .hasEnd = false, .isIf = true, .loopRets = std::vector<LoopReturn>()};

    LLVMPositionBuilderAtEnd(generator->builder, elseBlock);
	generator->currBB = elseBlock;
    if(this->_else != nullptr) this->_else->generate();

    if(!generator->activeLoops[selfNum].hasEnd) LLVMBuildBr(generator->builder, endBlock);

    bool hasEnd2 = generator->activeLoops[selfNum].hasEnd;

    if(generator->activeLoops[selfNum].loopRets.size() > 0) AST::funcTable[currScope->funcName]->rets.push_back(generator->activeLoops[selfNum].loopRets[0].ret);

    LLVMPositionBuilderAtEnd(generator->builder, endBlock);
    generator->currBB = endBlock;
    generator->activeLoops.erase(selfNum);

    if(hasEnd1 && hasEnd2 && generator->activeLoops.size() == 0) LLVMBuildRet(generator->builder, LLVMConstNull(generator->genType(AST::funcTable[currScope->funcName]->type, this->loc)));

    return nullptr;
}

Node* NodeIf::comptime() {
    NodeBool* val = (NodeBool*)cond->comptime();
    if(val->value && this->body != nullptr) {
        this->body->check();
        this->body->generate();
    }
    else if(!val->value && this->_else != nullptr) {
        this->_else->check();
        this->_else->generate();
    }
    return nullptr;
}

Node* NodeIf::copy() {return new NodeIf(this->cond->copy(), (this->body == nullptr ? nullptr : this->body->copy()), (this->_else == nullptr ? nullptr : this->_else->copy()), this->loc, this->funcName, this->isStatic);}