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

typedef struct {
    std::string file;
    bool isGlobal;
} ImportFile;

// Class of importing another files.

class NodeImport : public Node {
public:
    ImportFile file;
    std::vector<std::string> functions;
    int loc;

    NodeImport(ImportFile file, std::vector<std::string> functions, int loc);
    Type* getType() override;
    void check() override;
    RaveValue generate() override;
    Node* comptime() override;
    Node* copy() override;
};

// Multiple NodeImport instances.

class NodeImports : public Node {
public:
    std::vector<NodeImport*> imports;
    int loc;

    NodeImports(std::vector<NodeImport*> imports, int loc);
    Type* getType() override;
    void check() override;
    RaveValue generate() override;
    Node* comptime() override;
    Node* copy() override;
};