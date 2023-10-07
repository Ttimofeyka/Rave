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
#include <map>
#include "../parser.hpp"

struct StructPredefined {
    int element;
    Node* value;
    bool isStruct;
    std::string name;
};

class NodeStruct : public Node {
public:
    std::string name;
    std::vector<Node*> elements;
    std::vector<StructPredefined> predefines;
    std::vector<std::string> namespacesNames;
    long loc;
    std::string origname;
    std::vector<NodeFunc*>  constructors;
    NodeFunc* destructor;
    NodeFunc* with;
    std::vector<NodeFunc*> methods;
    bool isImported = false;
    std::string extends;
    std::map<char, std::map<std::string, NodeFunc*>> operators;
    std::vector<Node*> oldElements;
    std::vector<std::string> templateNames;
    bool noCompile = false;
    bool isComdat = false;
    bool isPacked = false;
    bool isTemplated = false;
    bool isLinkOnce = false;
    bool hasDefaultConstructor = false;
    std::vector<DeclarMod> mods;

    NodeStruct(std::string name, std::vector<Node*> elements, long loc, std::string extends, std::vector<std::string> templateNames, std::vector<DeclarMod> mods);
    
    LLVMTypeRef asConstType();
    std::vector<LLVMTypeRef> getParameters(bool isLinkOnce);
    LLVMTypeRef genWithTemplate(std::string sTypes, std::vector<Type*> types);
    LLVMValueRef generate() override;
    Type* getType() override;
    Node* comptime() override;
    Node* copy() override;
    void check() override;
    long getSize();
    bool hasPredefines();
    std::vector<Node*> copyElements();
};