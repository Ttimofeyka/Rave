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

class NodeArray : public Node {
public:
    long loc;
    std::vector<Node*> values;
    LLVMTypeRef type;
    bool isConst = true;

    NodeArray(long loc, std::vector<Node*> values);

    Type* getType() override;
    std::vector<LLVMValueRef> getValues();
    LLVMValueRef generate() override;
    void check() override;
    Node* comptime() override;
    Node* copy() override;
};