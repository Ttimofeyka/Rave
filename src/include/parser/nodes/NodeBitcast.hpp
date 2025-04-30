/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include "../Type.hpp"
#include "../Types.hpp"
#include "Node.hpp"

class NodeBitcast : public Node {
public:
    Type* type;
    Node* value;
    int loc;

    NodeBitcast(Type* type, Node* value, int loc);
    ~NodeBitcast() override;
    
    void check() override;
    Type* getType() override;
    Node* copy() override;
    Node* comptime() override;
    RaveValue generate() override;
};
