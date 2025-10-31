/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "../../include/parser/nodes/NodeIf.hpp"
#include "../../include/parser/nodes/NodeRet.hpp"
#include "../../include/parser/nodes/NodeBool.hpp"
#include "../../include/parser/nodes/NodeFunc.hpp"
#include "../../include/parser/nodes/NodeVar.hpp"
#include "../../include/parser/nodes/NodeNamespace.hpp"
#include "../../include/parser/nodes/NodeBuiltin.hpp"
#include "../../include/parser/nodes/NodeComptime.hpp"
#include "../../include/parser/nodes/NodeStruct.hpp"
#include "../../include/utils.hpp"

NodeIf::NodeIf(Node* cond, Node* body, Node* _else, int loc, bool isStatic) : cond(cond), body(body), _else(_else), loc(loc), isStatic(isStatic), isLikely(false), isUnlikely(false) {}

Type* NodeIf::getType() { return typeVoid; }

void NodeIf::check() {
    if (!isChecked && !isStatic) {
        isChecked = true;

        cond->check();
        if (body) body->check();
        if (_else) _else->check();
    }
}

RaveValue NodeIf::generate() {
    if (isStatic) { comptime(); return {}; }

    LLVMBasicBlockRef thenBlock = LLVM::makeBlock("then", currScope->funcName);
	LLVMBasicBlockRef elseBlock = LLVM::makeBlock("else", currScope->funcName);
	LLVMBasicBlockRef endBlock = nullptr;
    
    endBlock = LLVM::makeBlock("end", currScope->funcName);

    auto origScope = currScope;

    RaveValue condValue = cond->generate();

    if (isLikely || isUnlikely) {
        if (generator->functions.find("llvm.expect.i1") == generator->functions.end()) {
            generator->functions["llvm.expect.i1"] = {LLVMAddFunction(generator->lModule, "llvm.expect.i1", LLVMFunctionType(
            LLVMInt1TypeInContext(generator->context),
            std::vector<LLVMTypeRef>({LLVMInt1TypeInContext(generator->context), LLVMInt1TypeInContext(generator->context)}).data(),
            2, false
            )), new TypeFunc(basicTypes[BasicType::Bool], {new TypeFuncArg(basicTypes[BasicType::Bool], "v1"), new TypeFuncArg(basicTypes[BasicType::Bool], "v2")}, false)};
        }

        LLVM::call(generator->functions["llvm.expect.i1"], {condValue, {LLVM::makeInt(1, isLikely ? 1 : 0, false), basicTypes[BasicType::Bool]}}, isLikely ? "likely" : "unlikely");
    }

    LLVMBuildCondBr(generator->builder, condValue.value, thenBlock, elseBlock);

    int selfNum = generator->activeLoops.size();
    generator->activeLoops[selfNum] = Loop{.isActive = true, .start = thenBlock, .end = endBlock, .hasEnd = false, .isIf = true, .loopRets = std::vector<LoopReturn>(), .owner = this};

    LLVM::Builder::atEnd(thenBlock);

    currScope = copyScope(origScope);

    if (body) body->generate();
    if (!generator->activeLoops[selfNum].hasEnd) LLVMBuildBr(generator->builder, endBlock);

    bool hasEnd1 = generator->activeLoops[selfNum].hasEnd;

    generator->activeLoops[selfNum] = Loop{.isActive = true, .start = elseBlock, .end = endBlock, .hasEnd = false, .isIf = true, .loopRets = std::vector<LoopReturn>(), .owner = this};

    delete currScope;
    currScope = copyScope(origScope);

    LLVM::Builder::atEnd(elseBlock);
    if (_else) _else->generate();

    if (!generator->activeLoops[selfNum].hasEnd) LLVMBuildBr(generator->builder, endBlock);

    bool hasEnd2 = generator->activeLoops[selfNum].hasEnd;

    currScope = origScope;

    LLVM::Builder::atEnd(endBlock);
    generator->activeLoops.erase(selfNum);

    LLVMValueRef lastInstr = LLVMGetLastInstruction(endBlock);
    if (lastInstr && LLVMGetInstructionOpcode(lastInstr) == LLVMBr) LLVMInstructionEraseFromParent(lastInstr);

    lastInstr = LLVMGetLastInstruction(thenBlock);
    if (lastInstr && LLVMGetInstructionOpcode(lastInstr) == LLVMBr &&
       LLVMGetPreviousInstruction(lastInstr) &&
       LLVMGetInstructionOpcode(LLVMGetPreviousInstruction(lastInstr)) == LLVMBr) LLVMInstructionEraseFromParent(lastInstr);

    if (hasEnd1 && hasEnd2 && generator->activeLoops.size() == 0) LLVMBuildRet(generator->builder, LLVMConstNull(generator->genType(AST::funcTable[currScope->funcName]->type, this->loc)));
    
    return {};
}

void NodeIf::optimize() {
    if (body) body->optimize();
    if (_else) _else->optimize();
}

Node* NodeIf::comptime() {
    if (!body) return this;

    NodeBool* val = (NodeBool*)cond->comptime();

    Node* node = body;

    if (!val->value) {
        if (_else) node = _else;
        else return this;
    }

    if (isImported) {
        if (instanceof<NodeIf>(node)) ((NodeIf*)node)->isImported = true;
        else if (instanceof<NodeFunc>(node)) ((NodeFunc*)node)->isExtern = true;
        else if (instanceof<NodeComptime>(node)) ((NodeComptime*)node)->isImported = true;
        else if (instanceof<NodeVar>(node)) ((NodeVar*)node)->isExtern = true;
        else if (instanceof<NodeNamespace>(node)) ((NodeNamespace*)node)->isImported = true;
        else if (instanceof<NodeBuiltin>(node)) ((NodeBuiltin*)node)->isImport = true;
        else if (instanceof<NodeStruct>(node)) ((NodeStruct*)node)->isImported = true;
        else if (instanceof<NodeBlock>(node)) {
            NodeBlock* block = (NodeBlock*)node;

            for (Node* nd: block->nodes) {
                if (instanceof<NodeIf>(nd)) ((NodeIf*)nd)->isImported = true;
                else if (instanceof<NodeFunc>(nd)) ((NodeFunc*)nd)->isExtern = true;
                else if (instanceof<NodeComptime>(nd)) ((NodeComptime*)nd)->isImported = true;
                else if (instanceof<NodeVar>(nd)) ((NodeVar*)nd)->isExtern = true;
                else if (instanceof<NodeNamespace>(nd)) ((NodeNamespace*)nd)->isImported = true;
                else if (instanceof<NodeBuiltin>(nd)) ((NodeBuiltin*)nd)->isImport = true;
                else if (instanceof<NodeStruct>(nd)) ((NodeStruct*)nd)->isImported = true;
            }
        }
    }

    node->check();
    node->generate();

    return nullptr;
}

Node* NodeIf::copy() {
    NodeIf* _if = new NodeIf(cond->copy(), (!body ? nullptr : body->copy()), (!_else ? nullptr : _else->copy()), loc, isStatic);
    _if->isLikely = isLikely;
    _if->isUnlikely = isUnlikely;
    return _if;
}

NodeIf::~NodeIf() {
    if (cond) delete cond;
    if (body) delete body;
    if (_else) delete _else;
}
