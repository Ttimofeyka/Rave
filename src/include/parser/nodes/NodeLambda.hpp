/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <llvm-c/Core.h>
#include "Node.hpp"
#include "NodeBlock.hpp"
#include "NodeRet.hpp"
#include "../../lexer/tokens.hpp"
#include "../Type.hpp"
#include "../Types.hpp"
#include <vector>
#include <string>

class NodeLambda : public Node {
public:
    int loc;
    std::string name;
    LLVMValueRef f;
    LLVMBuilderRef builder;
    LLVMBasicBlockRef exitBlock;
    TypeFunc* tf;
    Type* type;
    NodeBlock* block;

    NodeLambda(int loc, TypeFunc* tf, NodeBlock* block, std::string name = "");
    Type* getType() override;
    RaveValue generate() override;
    Node* copy() override;
    Node* comptime() override;
    void check() override;
    ~NodeLambda() override;
    std::vector<LLVMTypeRef> generateTypes(); 
};