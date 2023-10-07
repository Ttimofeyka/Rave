/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include "../../llvm-c/Core.h"
#include "Node.hpp"
#include "NodeBlock.hpp"
#include "../../lexer/tokens.hpp"
#include "../Types.hpp"
#include <vector>
#include <string>

class NodeRet;

class NodeLambda : public Node {
public:
    long loc;
    std::string name;
    LLVMValueRef f;
    LLVMBuilderRef builder;
    LLVMBasicBlockRef exitBlock;
    TypeFunc* tf;
    Type* type;
    NodeBlock* block;
    std::vector<NodeRet*> rets;
    std::vector<RetGenStmt> genRets;

    NodeLambda(long loc, TypeFunc* tf, NodeBlock* block, std::string name = "");
};