/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <llvm-c/Core.h>
#include "Node.hpp"
#include <vector>
#include <string>

class NodeBlock : public Node {
public:
    std::vector<Node*> nodes;

    NodeBlock(std::vector<Node*> nodes);
    RaveValue generate() override;
    void check() override;
    void optimize() override;
    Node* copy() override;
    Node* comptime() override;
    Type* getType() override;
    ~NodeBlock() override;
};
