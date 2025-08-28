/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at htypep://mozilla.org/MPL/2.0/.
*/

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

namespace Defer {
    extern void make(Node* value, bool isFunctionScope);
}

class NodeDefer : public Node {
public:
    Node* instruction;
    int loc;
    bool isFunctionScope;

    NodeDefer(int loc, Node* instruction, bool isFunctionScope);
    Type* getType() override;
    
    void check() override;
    RaveValue generate() override;
    Node* comptime() override;
    Node* copy() override;
    ~NodeDefer() override;
};