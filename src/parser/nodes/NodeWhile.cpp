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
    this->isChecked = true;
}

RaveValue NodeWhile::generate() {
    auto& function = generator->functions[currScope->funcName];
    LLVMBasicBlockRef condBlock = LLVM::makeBlock("cond", function.value);
    LLVMBasicBlockRef whileBlock = LLVM::makeBlock("while", function.value);
    currScope->blockExit = LLVM::makeBlock("exit", function.value);

    LLVMBuildBr(generator->builder, condBlock);
    LLVMPositionBuilderAtEnd(generator->builder, condBlock);
    LLVMBuildCondBr(generator->builder, this->cond->generate().value, whileBlock, currScope->blockExit);
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

    LLVMPositionBuilderAtEnd(generator->builder, generator->activeLoops[selfNumber].end);
    generator->currBB = generator->activeLoops[selfNumber].end;
    generator->activeLoops.erase(selfNumber);

    delete currScope;
    currScope = oldScope;

    return {};
}

void NodeWhile::optimize() {
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
}