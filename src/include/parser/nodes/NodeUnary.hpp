/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include "../../llvm-c/Core.h"
#include "Node.hpp"
#include "../../lexer/tokens.hpp"
#include "../Types.hpp"
#include <vector>
#include <string>

class NodeUnary : public Node {
public:
    long loc;
    char type;
    Node* base;
    bool isEqu = false;

    NodeUnary(long loc, char type, Node* base);

    LLVMValueRef generatePtr();
    LLVMValueRef generateConst();
    void check() override;
    Type* getType() override;
    Node* comptime() override;
    LLVMValueRef generate() override;
    Node* copy() override;
};