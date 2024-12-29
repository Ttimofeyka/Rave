/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <llvm-c/Core.h>
#include "Node.hpp"
#include "../Types.hpp"

class NodeGet : public Node {
public:
    Node* base;
    std::string field;
    int loc;
    bool isMustBePtr = false;
    bool isPtrForIndex = false;
    bool elementIsConst = false;

    NodeGet(Node* base, std::string field, bool isMustBePtr, int loc);
    RaveValue checkStructure(RaveValue ptr);
    RaveValue checkIn(std::string structure);
    RaveValue generate() override;
    Type* getType() override;
    Node* comptime() override;
    Node* copy() override;
    void check() override;
    ~NodeGet() override;
};