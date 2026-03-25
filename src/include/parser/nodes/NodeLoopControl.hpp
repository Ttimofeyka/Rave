/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <llvm-c/Core.h>
#include "Node.hpp"
#include "../Types.hpp"

// Unified loop control node for both break and continue statements
// This replaces the separate NodeBreak and NodeContinue classes
enum class LoopControlKind {
    Break,
    Continue
};

class NodeLoopControl : public Node {
public:
    LoopControlKind kind;
    int loc;

    NodeLoopControl(LoopControlKind kind, int loc);

    int getWhileLoop();
    RaveValue generate() override;
    Type* getType() override;

    Node* comptime() override;
    Node* copy() override;
    void check() override;
};