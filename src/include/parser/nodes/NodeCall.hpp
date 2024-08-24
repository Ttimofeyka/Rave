/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <llvm-c/Core.h>
#include "Node.hpp"
#include "../Types.hpp"

class NodeFunc;

class NodeCall : public Node {
public:
    int loc;
    Node* func;
    std::vector<Node*> args;
    bool isInverted = false;
    bool isCdecl64 = false;
    NodeFunc* calledFunc = nullptr;
    int _offset = 0;

    NodeCall(int loc, Node* func, std::vector<Node*> args);
    RaveValue generate() override;
    Type* getType() override;
    
    std::vector<Type*> getTypes();
    std::vector<LLVMValueRef> getParameters(NodeFunc* nfunc, bool isVararg, std::vector<FuncArgSet> fas = std::vector<FuncArgSet>());
    std::vector<LLVMValueRef> correctByLLVM(std::vector<LLVMValueRef> values, std::vector<FuncArgSet> fas);
    Node* comptime() override;
    Node* copy() override;
    void check() override;
};