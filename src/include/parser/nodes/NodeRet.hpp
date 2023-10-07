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

class NodeRet : public Node {
public:
    Node* value;
    std::string parent;
    long loc;

    NodeRet(Node* value, std::string parent, long loc);

    Type* getType() override;
    LLVMValueRef generate() override;
    Node* copy() override;
    Node* comptime() override;
    void check() override;
    Loop getParentBlock(int n = -1);
    void setParentBlock(Loop value, int n = -1);
};

struct LoopReturn {
    NodeRet* ret;
    int loopId;
};

struct Loop {
    bool isActive;
    LLVMBasicBlockRef start;
    LLVMBasicBlockRef end;
    bool hasEnd;
    bool isIf;
    std::vector<LoopReturn> loopRets;
};