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

class NodeBool : public Node {
public:
    bool value;

    NodeBool(bool value);
    Type* getType() override;
    Type* getLType() override;
    void check() override;
    LLVMValueRef generate() override;
    Node* comptime() override;
    Node* copy() override;
};