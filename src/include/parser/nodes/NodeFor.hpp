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

class NodeFor : public Node {
public:
    std::vector<Node*> presets;
    Node* cond;
    std::vector<Node*> afters;
    NodeBlock* block;
    int loc;

    NodeFor(std::vector<Node*> presets, Node* cond, std::vector<Node*> afters, NodeBlock* block, int loc);
    void optimize() override;
    Type* getType() override;
    
    void check() override;
    RaveValue generate() override;
    Node* comptime() override;
    Node* copy() override;
    ~NodeFor() override;
};