/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <llvm-c/Core.h>
#include "Node.hpp"
#include "../Types.hpp"
#include <vector>
#include <string>

class NodeChar : public Node {
public:
    std::string value;
    bool isWide = false;

    NodeChar(std::string value, bool isWide);
    Type* getType() override;
    Type* getLType() override;
    LLVMValueRef generate() override;
    Node* copy() override;
    Node* comptime() override;
    void check() override;
};