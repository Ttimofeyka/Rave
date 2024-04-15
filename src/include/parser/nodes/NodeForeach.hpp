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

class NodeIden;

class NodeForeach : public Node {
public:
    NodeIden* elName;
    NodeBlock* block;
    Node* varData;
    Node* varLength;
    std::string funcName;
    long loc;

    NodeForeach(NodeIden* elName, Node* varData, Node* varLength, NodeBlock* block, std::string funcName, long loc);
    bool isReleased(std::string varName);
    Type* getType() override;
    void check() override;
    LLVMValueRef generate() override;
    Node* comptime() override;
    Node* copy() override;
};