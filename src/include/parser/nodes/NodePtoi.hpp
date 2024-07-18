/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <llvm-c/Core.h>
#include "Node.hpp"
#include "../Types.hpp"

class NodePtoi : public Node {
public:
    Node* value;
    long loc;

    NodePtoi(Node* value, long loc);
    ~NodePtoi() {delete this->value;}
    Type* getType() override;
    LLVMValueRef generate() override;
    Node* copy() override;
    Node* comptime() override;
    void check() override;
};