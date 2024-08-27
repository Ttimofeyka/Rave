/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <llvm-c/Core.h>
#include "Node.hpp"
#include "NodeBlock.hpp"
#include "../Types.hpp"
#include <vector>
#include <string>

class NodeIf : public Node {
public:
    Node* cond = nullptr;
    Node* body = nullptr;
    Node* _else = nullptr;
    int loc;
    std::string funcName;
    bool isStatic = false;
    bool hasRets[2];

    NodeIf(Node* cond, Node* body, Node* _else, int loc, std::string func, bool isStatic);
    Type* getType() override;
    void check() override;
    RaveValue generate() override;
    Node* comptime() override;
    Node* copy() override;
};