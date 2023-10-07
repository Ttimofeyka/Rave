/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "../../include/parser/nodes/NodeRet.hpp"
#include "../../include/utils.hpp"
#include "../../include/parser/ast.hpp"
#include "../../include/parser/nodes/NodeFunc.hpp"
#include "../../include/parser/nodes/NodeNull.hpp"
#include "../../include/parser/nodes/NodeLambda.hpp"

namespace AST {
    extern std::map<std::string, NodeFunc*> funcTable;
}

NodeRet::NodeRet(Node* value, std::string parent, long loc) {
    this->value = (value == nullptr ? nullptr : value->copy());
    this->parent = parent;
    this->loc = loc;
}

Node* NodeRet::copy() {return new NodeRet(this->value->copy(), this->parent, this->loc);}
Type* NodeRet::getType() {return this->value->getType();}
Node* NodeRet::comptime() {return this;}

void NodeRet::check() {
    bool oldCheck = this->isChecked;
    this->isChecked = true;
    if(!oldCheck && AST::funcTable.find(this->parent) != AST::funcTable.end()) AST::funcTable[this->parent]->rets.push_back(this);
}

Loop NodeRet::getParentBlock(int n) {
    if(generator->activeLoops.size() == 0) return Loop{.isActive = false, .start = nullptr, .end = nullptr, .hasEnd = false, .isIf = false, .loopRets = std::vector<LoopReturn>()};
    return generator->activeLoops[generator->activeLoops.size()-1];
}

void NodeRet::setParentBlock(Loop value, int n) {
    if(generator->activeLoops.size() > 0) {
        if(n == -1) generator->activeLoops[generator->activeLoops.size()-1] = value;
        else generator->activeLoops[n] = value;
    }
}

LLVMValueRef NodeRet::generate() {
    if(AST::funcTable.find(this->parent) != AST::funcTable.end()) {
        if(this->value != nullptr) {
            LLVMBasicBlockRef _start = generator->currBB;
            if(generator->activeLoops.size() != 0) {
                generator->activeLoops[generator->activeLoops.size()-1].hasEnd = true;
                generator->activeLoops[generator->activeLoops.size()-1].loopRets.push_back(LoopReturn{.ret = this, .loopId = (int)(generator->activeLoops.size()-1)});
                _start = generator->activeLoops[generator->activeLoops.size()-1].start;
            }
            if(instanceof<NodeNull>(this->value)) {
                ((NodeNull*)this->value)->type = AST::funcTable[this->parent]->type;
                LLVMValueRef retval = this->value->generate();
                AST::funcTable[this->parent]->genRets.push_back(RetGenStmt{.where = _start, .value = retval});
			    LLVMBuildBr(generator->builder, AST::funcTable[this->parent]->exitBlock);
                return retval;
            }
            LLVMValueRef retval = this->value->generate();
            std::string retvalType = std::string(LLVMPrintTypeToString(LLVMTypeOf(retval)));
            if(retvalType.substr(0,retvalType.size()-1) == std::string(LLVMPrintTypeToString(generator->genType(AST::funcTable[this->parent]->type,loc)))) retval = LLVMBuildLoad(
                generator->builder,retval,"loadNodeRet"
            );
            AST::funcTable[this->parent]->genRets.push_back(RetGenStmt{.where = _start, .value = retval});
			LLVMBuildBr(generator->builder, AST::funcTable[this->parent]->exitBlock);
            return retval;
        }
        else LLVMBuildBr(generator->builder, AST::funcTable[this->parent]->exitBlock);
    }
    else if(this->parent == "lambda"+std::to_string(generator->lambdas-1)) {
        if(this->value != nullptr) {
            if(generator->activeLoops.size() != 0) {
                generator->activeLoops[generator->activeLoops.size()-1].hasEnd = true;
                generator->activeLoops[generator->activeLoops.size()-1].loopRets.push_back(LoopReturn{.ret = this, .loopId = (int)(generator->activeLoops.size()-1)});
            }
            LLVMValueRef retval = this->value->generate();
            AST::lambdaTable[this->parent]->genRets.push_back(RetGenStmt{.where = generator->currBB, .value = retval});
			LLVMBuildBr(generator->builder, AST::lambdaTable[this->parent]->exitBlock);
            return retval;
        }
        else LLVMBuildBr(generator->builder, AST::lambdaTable[this->parent]->exitBlock);
    }
    else return this->value->generate();
    currScope->funcHasRet = true;
    return nullptr;
}