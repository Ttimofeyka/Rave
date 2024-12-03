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
#include "../../include/parser/nodes/NodeForeach.hpp"
#include "../../include/parser/nodes/NodeVar.hpp"
#include "../../include/utils.hpp"

NodeIf::NodeIf(Node* cond, Node* body, Node* _else, int loc, bool isStatic) {
    this->cond = cond;
    this->body = body;
    this->_else = _else;
    this->loc = loc;
    this->isStatic = isStatic;
    this->isLikely = false;
    this->isUnlikely = false;
}

Type* NodeIf::getType() {return typeVoid;}

void NodeIf::check() {
    bool oldCheck = this->isChecked;
    this->isChecked = true;

    if(!oldCheck) {
        this->cond->check();
        if(this->body != nullptr) this->body->check();
        if(this->_else != nullptr) this->_else->check();
    }
}

RaveValue NodeIf::generate() {
    if(isStatic) {
        this->comptime();
        return {};
    }

    LLVMBasicBlockRef thenBlock = LLVM::makeBlock("then", currScope->funcName);
	LLVMBasicBlockRef elseBlock = LLVM::makeBlock("else", currScope->funcName);
	LLVMBasicBlockRef endBlock = nullptr;

    if(this->_else != nullptr && instanceof<NodeIf>(this->_else)) {
        if(currScope->elseIfEnd == nullptr) currScope->elseIfEnd = LLVM::makeBlock("elseIfEnd", currScope->funcName);
        endBlock = currScope->elseIfEnd;
    }
    else {
        endBlock = LLVM::makeBlock("end", currScope->funcName);
        currScope->elseIfEnd = nullptr;
    }

    auto origScope = currScope;

    RaveValue condValue = cond->generate();

    if(isLikely) LLVM::call(generator->functions["llvm.expect.i1"], {condValue, {LLVM::makeInt(1, 1, false), basicTypes[BasicType::Bool]}}, "likely");
    else if(isUnlikely) LLVM::call(generator->functions["llvm.expect.i1"], {condValue, {LLVM::makeInt(1, 0, false), basicTypes[BasicType::Bool]}}, "unlikely");

    LLVMBuildCondBr(generator->builder, condValue.value, thenBlock, elseBlock);

    int selfNum = generator->activeLoops.size();
    generator->activeLoops[selfNum] = Loop{.isActive = true, .start = thenBlock, .end = endBlock, .hasEnd = false, .isIf = true, .loopRets = std::vector<LoopReturn>(), .owner = this};

    LLVMPositionBuilderAtEnd(generator->builder, thenBlock);
    generator->currBB = thenBlock;

    currScope = copyScope(origScope);

    if(this->body != nullptr) this->body->generate();
    if(!generator->activeLoops[selfNum].hasEnd) LLVMBuildBr(generator->builder, endBlock);

    bool hasEnd1 = generator->activeLoops[selfNum].hasEnd;

    generator->activeLoops[selfNum] = Loop{.isActive = true, .start = elseBlock, .end = endBlock, .hasEnd = false, .isIf = true, .loopRets = std::vector<LoopReturn>(), .owner = this};

    delete currScope;
    currScope = copyScope(origScope);

    LLVMPositionBuilderAtEnd(generator->builder, elseBlock);
	generator->currBB = elseBlock;
    if(this->_else != nullptr) this->_else->generate();

    if(!generator->activeLoops[selfNum].hasEnd) LLVMBuildBr(generator->builder, endBlock);

    bool hasEnd2 = generator->activeLoops[selfNum].hasEnd;

    currScope = origScope;

    LLVMPositionBuilderAtEnd(generator->builder, endBlock);
    generator->currBB = endBlock;
    generator->activeLoops.erase(selfNum);

    LLVMValueRef lastInstr = LLVMGetLastInstruction(endBlock);
    if(lastInstr != nullptr && LLVMGetInstructionOpcode(lastInstr) == LLVMBr && std::string(LLVMPrintValueToString(LLVMGetOperand(lastInstr, 0))).find("elseIfEnd") != std::string::npos) LLVMInstructionEraseFromParent(lastInstr);

    lastInstr = LLVMGetLastInstruction(thenBlock);
    if(lastInstr != nullptr && LLVMGetInstructionOpcode(lastInstr) == LLVMBr &&
       LLVMGetPreviousInstruction(lastInstr) != nullptr &&
       LLVMGetInstructionOpcode(LLVMGetPreviousInstruction(lastInstr)) == LLVMBr) LLVMInstructionEraseFromParent(lastInstr);

    if(hasEnd1 && hasEnd2 && generator->activeLoops.size() == 0) LLVMBuildRet(generator->builder, LLVMConstNull(generator->genType(AST::funcTable[currScope->funcName]->type, this->loc)));
    
    return {};
}

void NodeIf::optimize() {
    if(body != nullptr) {
        if(instanceof<NodeIf>(body)) ((NodeIf*)body)->optimize();
        else if(instanceof<NodeFor>(body)) ((NodeFor*)body)->optimize();
        else if(instanceof<NodeWhile>(body)) ((NodeWhile*)body)->optimize();
        else if(instanceof<NodeForeach>(body)) ((NodeForeach*)body)->optimize();
        else if(instanceof<NodeBlock>(body)) {
            NodeBlock* nblock = (NodeBlock*)body;
            for(int i=0; i<nblock->nodes.size(); i++) {
                if(instanceof<NodeIf>(nblock->nodes[i])) ((NodeIf*)nblock->nodes[i])->optimize();
                else if(instanceof<NodeFor>(nblock->nodes[i])) ((NodeFor*)nblock->nodes[i])->optimize();
                else if(instanceof<NodeWhile>(nblock->nodes[i])) ((NodeWhile*)nblock->nodes[i])->optimize();
                else if(instanceof<NodeForeach>(nblock->nodes[i])) ((NodeForeach*)nblock->nodes[i])->optimize();
                else if(instanceof<NodeVar>(nblock->nodes[i]) && !((NodeVar*)nblock->nodes[i])->isGlobal && !((NodeVar*)nblock->nodes[i])->isUsed) generator->warning("unused variable '" + ((NodeVar*)nblock->nodes[i])->name + "'!", loc);
            }
        }
    }

    if(_else != nullptr) {
        if(instanceof<NodeIf>(_else)) ((NodeIf*)_else)->optimize();
        else if(instanceof<NodeFor>(_else)) ((NodeFor*)_else)->optimize();
        else if(instanceof<NodeWhile>(_else)) ((NodeWhile*)_else)->optimize();
        else if(instanceof<NodeForeach>(_else)) ((NodeForeach*)_else)->optimize();
        else if(instanceof<NodeBlock>(_else)) {
            NodeBlock* nblock = (NodeBlock*)_else;
            for(int i=0; i<nblock->nodes.size(); i++) {
                if(instanceof<NodeIf>(nblock->nodes[i])) ((NodeIf*)nblock->nodes[i])->optimize();
                else if(instanceof<NodeFor>(nblock->nodes[i])) ((NodeFor*)nblock->nodes[i])->optimize();
                else if(instanceof<NodeWhile>(nblock->nodes[i])) ((NodeWhile*)nblock->nodes[i])->optimize();
                else if(instanceof<NodeForeach>(nblock->nodes[i])) ((NodeForeach*)nblock->nodes[i])->optimize();
                else if(instanceof<NodeVar>(nblock->nodes[i]) && !((NodeVar*)nblock->nodes[i])->isGlobal && !((NodeVar*)nblock->nodes[i])->isUsed) generator->warning("unused variable '" + ((NodeVar*)nblock->nodes[i])->name + "'!", loc);
            }
        }
    }
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

Node* NodeIf::copy() {
    NodeIf* _if = new NodeIf(this->cond->copy(), (this->body == nullptr ? nullptr : this->body->copy()), (this->_else == nullptr ? nullptr : this->_else->copy()), this->loc, this->isStatic);
    _if->isLikely = this->isLikely;
    _if->isUnlikely = this->isUnlikely;
    return _if;
}