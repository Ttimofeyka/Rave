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

class NodeNull : public Node {
public:
    Type* type;
    int loc;

    NodeNull(Type* type, int loc);
    Type* getType() override;
    RaveValue generate() override;
    void check() override;
    Node* comptime() override;
    Node* copy() override;
};