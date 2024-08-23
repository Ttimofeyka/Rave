/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <string>
#include <map>
#include <llvm-c/Core.h>
#include <llvm-c/Initialization.h>
#include <llvm-c/lto.h>
#include <llvm-c/Target.h>
#include "../utils.hpp"
#include "./Type.hpp"
#include "./Types.hpp"
#include "./nodes/Node.hpp"
#include "../json.hpp"
#include "../llvm.hpp"
#include <vector>

class NodeVar;
class NodeFunc;
class NodeLambda;
class NodeStruct;
class NodeCall;
class TypeFunc;
struct Loop;

struct StructMember {
    int number;
    NodeVar* var;
};

struct FuncArgSet {
    std::string name;
    Type* type;
    std::vector<Type*> internalTypes;
};

namespace AST {
    extern std::map<std::string, Type*> aliasTypes;
    extern std::map<std::string, Node*> aliasTable;
    extern std::map<std::string, NodeVar*> varTable;
    extern std::map<std::string, NodeFunc*> funcTable;
    extern std::map<std::string, NodeStruct*> structTable;
    extern std::map<std::string, NodeLambda*> lambdaTable;
    extern std::map<std::pair<std::string, std::string>, NodeFunc*> methodTable;
    extern std::map<std::pair<std::string, std::string>, StructMember> structsNumbers;
    extern std::vector<std::string> importedFiles;
    extern std::vector<std::string> addToImport;
    extern std::map<std::string, std::vector<Node*>> parsed;
    extern std::map<int, Node*> condStack;
    extern std::string mainFile;
    extern bool debugMode;

    void checkError(std::string message, int loc);
}

extern std::string typesToString(std::vector<FuncArgSet> args);
extern std::string typesToString(std::vector<Type*> args);
extern std::vector<Type*> parametersToTypes(std::vector<LLVMValueRef> params);

class LLVMGen {
public:
    LLVMModuleRef lModule;
    LLVMContextRef context;
    LLVMBuilderRef builder;
    LLVMTargetDataRef targetData;
    std::string file;
    genSettings settings;
    nlohmann::json options;
    
    std::map<std::string, LLVMValueRef> globals;
    std::map<std::string, LLVMValueRef> functions;
    std::map<std::string, LLVMTypeRef> structures;
    std::map<int32_t, Loop> activeLoops;

    std::map<std::string, std::string> neededFunctions;
    std::map<std::string, Type*> toReplace;

    LLVMBasicBlockRef currBB;

    long lambdas = 0;
    int currentBuiltinArg = 0;

    void error(std::string msg, int line);
    void warning(std::string msg, int line);

    LLVMGen(std::string file, genSettings settings, nlohmann::json options);

    std::string mangle(std::string name, bool isFunc, bool isMethod);

    LLVMTypeRef genType(Type* type, int loc);
    LLVMValueRef byIndex(LLVMValueRef value, std::vector<LLVMValueRef> indexes);
    void addAttr(std::string name, LLVMAttributeIndex index, LLVMValueRef ptr, int loc, unsigned long value = 0);
    void addStrAttr(std::string name, LLVMAttributeIndex index, LLVMValueRef ptr, int loc, std::string value = "");
    Type* setByTypeList(std::vector<Type*> list);

    int getAlignment(Type* type);
};

class Scope {
public:
    std::map<std::string, RaveValue> localScope;
    std::map<std::string, int> args;
    std::string funcName;
    LLVMBasicBlockRef blockExit;
    bool funcHasRet = false;
    std::map<std::string, NodeVar*> localVars;
    std::map<std::string, NodeVar*> argVars;
    std::map<std::string, Node*> aliasTable;
    LLVMBasicBlockRef fnEnd;
    LLVMBasicBlockRef elseIfEnd = nullptr;
    bool detectMemoryLeaks = false;

    Scope(std::string funcName, std::map<std::string, int> args, std::map<std::string, NodeVar*> argVars);

    // Old functions (not recommended)

    LLVMValueRef get(std::string name, int loc = -1);
    LLVMValueRef getWithoutLoad(std::string name, int loc = -1);
    NodeVar* getVar(std::string name, int loc = -1);

    // New functions (recommended)

    

    bool has(std::string name);
    bool hasAtThis(std::string name);
    bool locatedAtThis(std::string name);
    void hasChanged(std::string name);
    void remove(std::string name);
};

extern LLVMGen* generator;
extern Scope* currScope;
extern LLVMTargetDataRef dataLayout;

extern Type* lTypeToType(LLVMTypeRef t, LLVMValueRef value);
extern TypeFunc* callToTFunc(NodeCall* call);
extern Scope* copyScope(Scope* original);
std::string typeToString(LLVMTypeRef type);