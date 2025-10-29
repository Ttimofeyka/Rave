/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <llvm-c/Core.h>
#include "Node.hpp"
#include "NodeBlock.hpp"
#include "../Types.hpp"
#include <vector>
#include <string>

class NodeType;
class NodeBool;
class BigInt;

class NodeBuiltin : public Node {
public:
    std::string name;
    std::vector<Node*> args;
    int loc;
    Type* type = nullptr;
    NodeBlock* block;
    bool isImport = false;
    bool isTopLevel = false;
    int CTId = 0;

    NodeBuiltin(std::string name, std::vector<Node*> args, int loc, NodeBlock* block);
    NodeBuiltin(std::string name, std::vector<Node*> args, int loc, NodeBlock* block, Type* type, bool isImport, bool isTopLevel, int CTId);
    ~NodeBuiltin() override;

    NodeType* asType(int n, bool isCompTime = false);
    Type* asClearType(int n);
    BigInt asNumber(int n);
    NodeBool* asBool(int n);
    std::string getAliasName(int n);
    std::string asStringIden(int n);
    Type* getType() override;
    
    void check() override;
    RaveValue generate() override;
    Node* comptime() override;
    Node* copy() override;

private:
    void requireMinArgs(int n);
    int handleAbstractBool();
    int handleAbstractInt();
};