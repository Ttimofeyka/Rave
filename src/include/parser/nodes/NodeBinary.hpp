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

namespace Binary {
    extern void store(RaveValue pointer, RaveValue value, int loc = -1);
    extern RaveValue operation(char op, Node* First, Node* second, int loc);
    extern std::pair<std::string, std::string> isOperatorOverload(Node* firstNode, Node* secondNode, RaveValue first, RaveValue second, char op);
}

class NodeBinary : public Node {
public:
    char op;
    Node* first;
    Node* second;
    bool isStatic = false;
    int loc;

    NodeBinary(char op, Node* first, Node* second, int loc, bool isStatic = false);

    RaveValue generate() override;
    Type* getType() override;
    
    Node* comptime() override;
    Node* copy() override;
    void check() override;
    std::pair<std::string, std::string> isOperatorOverload(RaveValue first, RaveValue second, char op);
    ~NodeBinary() override;
};