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

class NodeAliasType : public Node {
public:
    long loc;
    std::string name;
    std::string origName;
    Type* value;
    std::vector<std::string> namespacesNames;

    NodeAliasType(std::string name, Type* value, long loc);
    Type* getType() override;
    Type* getLType() override;
    void check() override;
    LLVMValueRef generate() override;
    Node* comptime() override;
    Node* copy() override;
};