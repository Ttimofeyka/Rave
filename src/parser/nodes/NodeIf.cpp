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
#include "../../include/parser/nodes/NodeNamespace.hpp"
#include "../../include/parser/nodes/NodeBuiltin.hpp"
#include "../../include/parser/nodes/NodeComptime.hpp"
#include "../../include/parser/nodes/NodeStruct.hpp"
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

    if(!oldCheck && !isStatic) {
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
    if(body != nullptr) body->optimize();
    if(_else != nullptr) _else->optimize();
}

Node* NodeIf::comptime() {
    NodeBool* val = (NodeBool*)cond->comptime();

    if(this->body == nullptr) return this;

    Node* node = body;

    if(!val->value) {
        if(this->_else != nullptr) node = this->_else;
        else return this;
    }

    if(this->isImported) {
        if(instanceof<NodeIf>(node)) ((NodeIf*)node)->isImported = true;
        else if(instanceof<NodeFunc>(node)) ((NodeFunc*)node)->isExtern = true;
        else if(instanceof<NodeComptime>(node)) ((NodeComptime*)node)->isImported = true;
        else if(instanceof<NodeVar>(node)) ((NodeVar*)node)->isExtern = true;
        else if(instanceof<NodeNamespace>(node)) ((NodeNamespace*)node)->isImported = true;
        else if(instanceof<NodeBuiltin>(node)) ((NodeBuiltin*)node)->isImport = true;
        else if(instanceof<NodeStruct>(node)) ((NodeStruct*)node)->isImported = true;
        else if(instanceof<NodeBlock>(node)) {
            NodeBlock* block = (NodeBlock*)node;

            for(Node* nd: block->nodes) {
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

    node->check();
    node->generate();

    return nullptr;
}

Node* NodeIf::copy() {
    NodeIf* _if = new NodeIf(this->cond->copy(), (this->body == nullptr ? nullptr : this->body->copy()), (this->_else == nullptr ? nullptr : this->_else->copy()), this->loc, this->isStatic);
    _if->isLikely = this->isLikely;
    _if->isUnlikely = this->isUnlikely;
    return _if;
}

NodeIf::~NodeIf() {
    if(this->cond != nullptr) delete this->cond;
    if(this->body != nullptr) delete this->body;
    if(this->_else != nullptr) delete this->_else;
}