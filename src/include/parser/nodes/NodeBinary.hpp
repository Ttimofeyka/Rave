/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include "../../llvm-c/Core.h"
#include "Node.hpp"
#include "../Types.hpp"
#include <vector>
#include <string>

namespace Binary {
    LLVMValueRef castValue(LLVMValueRef from, LLVMTypeRef to, long loc = -1);
    LLVMValueRef sum(LLVMValueRef one, LLVMValueRef two, long loc = -1);
    LLVMValueRef sub(LLVMValueRef one, LLVMValueRef two, long loc = -1);
    LLVMValueRef mul(LLVMValueRef one, LLVMValueRef two, long loc = -1);
    LLVMValueRef div(LLVMValueRef one, LLVMValueRef two, long loc = -1);
    LLVMValueRef compare(LLVMValueRef one, LLVMValueRef two, char, long loc = -1);
}

class NodeBinary : public Node {
public:
    char op;
    Node* first;
    Node* second;
    bool isStatic = false;
    long loc;

    NodeBinary(char op, Node* first, Node* second, long loc, bool isStatic = false);

    LLVMValueRef generate() override;
    Type* getType() override;
    Node* comptime() override;
    Node* copy() override;
    void check() override;
};