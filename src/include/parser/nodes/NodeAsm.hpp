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

class NodeAsm : public Node {
public:
    std::string line;
    std::string additions = "";
    std::vector<Node*> values;
    bool isVolatile = false;
    Type* type;
    long loc;

    NodeAsm(std::string line, bool isVolatile, Type* type, std::string additions, std::vector<Node*> values, long loc);
    Type* getType() override;
    void check() override;
    LLVMValueRef generate() override;
    Node* comptime() override;
    Node* copy() override;
};