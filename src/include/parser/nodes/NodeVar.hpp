/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <llvm-c/Core.h>
#include "Node.hpp"
#include "../parser.hpp"
#include "../Types.hpp"
#include <vector>
#include <string>

class NodeVar : public Node {
public:
    std::string name;
    std::string origName;
    std::string linkName;
    Node* value;
    std::vector<DeclarMod> mods;
    int loc;
    Type* type;
    std::vector<std::string> namespacesNames;

    bool isConst;
    bool isGlobal;
    bool isExtern;
    bool isVolatile = false;
    bool isChanged = false;
    bool noZeroInit = false;
    bool isPrivate = false;
    bool isComdat = false;
    bool isAllocated = false;
    bool isNoOperators = false;
    bool isNoCopy = false;
    bool isInternal = false;

    NodeVar(std::string name, Node* value, bool isExtern, bool isConst, bool isGlobal, std::vector<DeclarMod> mods, int loc, Type* type, bool isVolatile = false);
    NodeVar(std::string name, Node* value, bool isExtern, bool isConst, bool isGlobal, std::vector<DeclarMod> mods, int loc, Type* type, bool isVolatile, bool isChanged, bool noZeroInit);
    NodeVar(std::string name, Node* value, bool isExtern, bool isConst, bool isGlobal, std::vector<DeclarMod> mods, int loc, Type* type, bool isVolatile, bool isChanged, bool noZeroInit, bool isInternal);

    RaveValue generate() override;
    Type* getType() override;
    
    Node* comptime() override;
    Node* copy() override;
    void check() override;
};