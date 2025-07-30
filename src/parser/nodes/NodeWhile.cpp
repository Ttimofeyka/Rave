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

NodeWhile::NodeWhile(Node* cond, Node* body, int loc) {
    this->cond = cond;
    this->body = body;
    this->loc = loc;
}

NodeWhile::~NodeWhile() {
    if (cond != nullptr) delete cond;
    if (body != nullptr) delete body;
}

Type* NodeWhile::getType() {return typeVoid;}
Node* NodeWhile::comptime() {return this;}
Node* NodeWhile::copy() {return new NodeWhile(this->cond->copy(), this->body->copy(), this->loc);}

void NodeWhile::check() {isChecked = true;}

RaveValue NodeWhile::generate() {
    auto& function = generator->functions[currScope->funcName];
    LLVMBasicBlockRef condBlock = LLVM::makeBlock("cond", function.value);
    LLVMBasicBlockRef whileBlock = LLVM::makeBlock("while", function.value);
    currScope->blockExit = LLVM::makeBlock("exit", function.value);

    LLVMBuildBr(generator->builder, condBlock);
    LLVM::Builder::atEnd(condBlock);

    LLVMBuildCondBr(generator->builder, this->cond->generate().value, whileBlock, currScope->blockExit);
    LLVM::Builder::atEnd(whileBlock);

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
    
    if (!generator->activeLoops[selfNumber].hasEnd) LLVMBuildBr(generator->builder, condBlock);

    LLVMPositionBuilderAtEnd(generator->builder, generator->activeLoops[selfNumber].end);
    generator->currBB = generator->activeLoops[selfNumber].end;
    generator->activeLoops.erase(selfNumber);

    delete currScope;
    currScope = oldScope;

    return {};
}

void NodeWhile::optimize() {
    if (body != nullptr)
        body->optimize();
}