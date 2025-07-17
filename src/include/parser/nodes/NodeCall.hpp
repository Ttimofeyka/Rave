/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <llvm-c/Core.h>
#include "Node.hpp"
#include "../Types.hpp"

class NodeFunc;

struct CallSettings {
    bool isCW64;
    bool isVararg;
    int loc;
};

namespace Template {
    extern std::string fromTypes(std::vector<Type*>& types);
}

namespace Call {
    extern std::vector<Type*> getParamsTypes(std::vector<RaveValue>& params);
    extern std::vector<Type*> getTypes(std::vector<Node*>& arguments);
    extern std::vector<RaveValue> genParameters(std::vector<Node*>& arguments, std::vector<int>& byVals, std::vector<FuncArgSet>& fas, CallSettings settings);
    extern std::vector<RaveValue> genParameters(std::vector<Node*>& arguments, std::vector<int>& byVals, NodeFunc* function, int loc);
    extern NodeVar* findVarFunction(std::string structName, std::string variable);
    extern std::map<std::pair<std::string, std::string>, NodeFunc*>::iterator findMethod(std::string structName, std::string methodName, std::vector<Node*>& arguments, int loc);
    extern RaveValue make(int loc, Node* function, std::vector<Node*> arguments);
}

class NodeCall : public Node {
public:
    int loc;
    Node* func;
    std::vector<Node*> args;
    bool isCW64 = false;
    NodeFunc* calledFunc = nullptr;
    int _offset = 0;

    NodeCall(int loc, Node* func, std::vector<Node*> args);
    RaveValue generate() override;
    Type* __getType(Node* fn);
    Type* getType() override;
    
    Node* comptime() override;
    Node* copy() override;
    void check() override;
    ~NodeCall() override;
};