/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <llvm-c/Core.h>
#include "Node.hpp"
#include "../Type.hpp"
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
    int loc;
    std::string origname;
    std::vector<NodeFunc*>  constructors;
    NodeFunc* destructor;
    NodeFunc* with;
    std::vector<NodeFunc*> methods;
    std::string extends;
    std::map<char, std::map<std::string, NodeFunc*>> operators;
    std::vector<Node*> oldElements;
    std::vector<std::string> templateNames;
    std::vector<DeclarMod> mods;
    std::string dataVar;
    std::string lengthVar;

    bool noCompile = false;
    bool isComdat = false;
    bool isPacked = false;
    bool isTemplated = false;
    bool isLinkOnce = false;
    bool hasDefaultConstructor = false;
    bool isImported = false;

    NodeStruct(std::string name, std::vector<Node*> elements, int loc, std::string extends, std::vector<std::string> templateNames, std::vector<DeclarMod> mods);
    
    LLVMTypeRef asConstType();
    std::vector<LLVMTypeRef> getParameters(bool isLinkOnce);
    std::vector<NodeVar*> getVariables();
    LLVMTypeRef genWithTemplate(std::string sTypes, std::vector<Type*> types);
    RaveValue generate() override;
    Type* getType() override;
    
    Node* comptime() override;
    Node* copy() override;
    void check() override;
    long getSize();
    bool hasPredefines();
    std::vector<Node*> copyElements();
};