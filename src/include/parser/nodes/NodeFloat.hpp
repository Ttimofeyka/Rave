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
#include "../../r128.h"

class NodeFloat : public Node {
public:
    std::string value;
    TypeBasic* type = nullptr;
    bool isMustBeFloat = false;

    NodeFloat(double value);
    NodeFloat(std::string value);
    NodeFloat(double value, bool isDouble);
    NodeFloat(double value, TypeBasic* type);
    NodeFloat(std::string value, TypeBasic* type);
    ~NodeFloat() {if(this->type != nullptr) delete this->type;}
    Type* getType() override;
    
    RaveValue generate() override;
    Node* copy() override;
    Node* comptime() override;
    void check() override;
};