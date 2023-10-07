/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include "../../llvm-c/Core.h"
#include "Node.hpp"
#include "../Types.hpp"
#include <vector>
#include <string>

class NodeImport : public Node {
public:
    std::string file;
    std::vector<std::string> functions;
    long loc;

    NodeImport(std::string file, std::vector<std::string> functions, long loc);
    Type* getType() override;
    void check() override;
    LLVMValueRef generate() override;
    Node* comptime() override;
    Node* copy() override;
};

class NodeImports : public Node {
public:
    std::vector<NodeImport*> imports;
    long loc;

    NodeImports(std::vector<NodeImport*> imports, long loc);
    Type* getType() override;
    void check() override;
    LLVMValueRef generate() override;
    Node* comptime() override;
    Node* copy() override;
};