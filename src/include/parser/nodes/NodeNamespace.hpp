/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include "../Types.hpp"
#include <llvm-c/Core.h>
#include "Node.hpp"
#include <vector>
#include <string>

class NodeNamespace : public Node {
public:
    std::vector<std::string> names;
    std::vector<Node*> nodes;
    long loc;
    bool isImported = false;
    bool hidePrivated = false;

    NodeNamespace(std::string name, std::vector<Node*> nodes, long loc);
    NodeNamespace(std::vector<std::string> names, std::vector<Node*> nodes, long loc);
    Type* getType() override;
    LLVMValueRef generate() override;
    Node* copy() override;
    Node* comptime() override;
    void check() override;
};