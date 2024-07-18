/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <llvm-c/Core.h>
#include "Node.hpp"
#include "../Types.hpp"

class NodeIndex : public Node {
public:
    Node* element;
    std::vector<Node*> indexes;
    long loc;
    bool isMustBePtr = false;
    bool elementIsConst = false;

    NodeIndex(Node* element, std::vector<Node*> indexes, long loc);
    Type* getType() override;
    LLVMValueRef generate() override;
    Node* copy() override;
    Node* comptime() override;
    void check() override;
    std::vector<LLVMValueRef> generateIndexes();
    bool isElementConst(Type* type);
};