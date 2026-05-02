/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <string>
#include <unordered_map>
#include <llvm-c/Core.h>
#include <llvm-c/DebugInfo.h>

#ifndef LLVM_VERSION
#error LLVM_VERSION is required
#endif

#if LLVM_VERSION < 17
#include <llvm-c/Initialization.h>
#endif

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
class TypeBasic;
class TypeStruct;
class TypeArray;
struct Loop;

struct StructMember {
    size_t number;
    NodeVar* var;
};

struct FuncArgSet {
    std::string name;
    Type* type;
    std::vector<Type*> internalTypes;
};

namespace std {
    template<> struct hash<std::pair<std::string, std::string>> {
        size_t operator()(const std::pair<std::string, std::string>& p) const {
            size_t h1 = hash<std::string>{}(p.first);
            size_t h2 = hash<std::string>{}(p.second);
            return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
        }
    };
}

namespace AST {
    extern std::unordered_map<std::string, Type*> aliasTypes;
    extern std::unordered_map<std::string, Node*> aliasTable;
    extern std::unordered_map<std::string, NodeVar*> varTable;
    extern std::unordered_map<std::string, NodeFunc*> funcTable;
    extern std::unordered_map<std::string, std::vector<NodeFunc*>> funcVersionsTable;
    extern std::unordered_map<std::string, NodeStruct*> structTable;
    extern std::unordered_map<std::pair<std::string, std::string>, StructMember> structMembersTable;
    extern std::unordered_map<std::string, NodeLambda*> lambdaTable;
    extern std::unordered_map<std::pair<std::string, std::string>, NodeFunc*> methodTable;
    extern std::vector<std::string> importedFiles;
    extern std::vector<std::string> addToImport;
    extern std::unordered_map<std::string, std::vector<Node*>> parsed;
    extern std::string mainFile;
    extern std::string currentFile;
    extern bool debugMode;

    extern void checkError(std::string message, int loc);
}

extern std::string typesToString(std::vector<FuncArgSet>& args);
extern std::string typesToString(std::vector<Type*>& args);
extern std::vector<Type*> parametersToTypes(std::vector<RaveValue> params);

class DebugGen {
public:
    LLVMDIBuilderRef diBuilder;
    LLVMMetadataRef diFile;
    LLVMMetadataRef diScope;
    std::vector<LLVMMetadataRef> scopeStack;

    LLVMMetadataRef genBasicType(TypeBasic* type, int loc);
    LLVMMetadataRef genPointer(std::string previousName, LLVMMetadataRef type, int loc);
    LLVMMetadataRef genArray(int size, LLVMMetadataRef type, int loc);
    LLVMMetadataRef genStruct(TypeStruct* type, int loc);
    LLVMMetadataRef genType(Type* type, int loc);

    void pushScope(LLVMMetadataRef scope);
    void popScope();
    LLVMMetadataRef currentScope();
    void setInstrLoc(int line);
    void genGlobalVariable(const char* name, LLVMMetadataRef type, LLVMValueRef variable, int line);

    DebugGen(genSettings settings, std::string file, LLVMModuleRef module);
    ~DebugGen();
};

extern DebugGen* debugInfo;

class LLVMGen {
public:
    LLVMModuleRef lModule;
    LLVMContextRef context;
    LLVMBuilderRef builder;
    LLVMTargetDataRef targetData;
    std::string file;
    genSettings settings;
    nlohmann::json options;

    std::unordered_map<std::string, RaveValue> globals;
    std::unordered_map<std::string, RaveValue> functions;
    std::unordered_map<std::string, LLVMTypeRef> structures;
    std::unordered_map<int32_t, Loop> activeLoops;

    std::unordered_map<std::string, Type*> toReplace;
    std::unordered_map<std::string, Node*> toReplaceValues;
    std::unordered_map<std::string, LLVMTypeRef> typeCache;

    LLVMBasicBlockRef currBB;

    long lambdas = 0;
    int currentBuiltinArg = 0;

    void error(std::string msg, int line);
    void warning(std::string msg, int line);

    LLVMGen(std::string file, genSettings settings, nlohmann::json options);
    ~LLVMGen();

    std::string mangle(std::string name, bool isFunc, bool isMethod);

    LLVMTypeRef genType(Type* type, int loc);

    LLVMTypeRef genBasicType(TypeBasic* basicType);
    LLVMTypeRef genPointerType(Type* instance, int loc);
    LLVMTypeRef genArrayType(TypeArray* arrayType, int loc);
    LLVMTypeRef genStructType(TypeStruct* s, int loc);
    LLVMTypeRef genFuncType(TypeFunc* tf, int loc);
    Type* resolveTemplateType(TypeStruct* s);

    RaveValue byIndex(RaveValue value, std::vector<LLVMValueRef> indexes);
    void addAttr(std::string name, LLVMAttributeIndex index, LLVMValueRef ptr, int loc, unsigned long value = 0);
    void addTypeAttr(std::string name, LLVMAttributeIndex index, LLVMValueRef ptr, int loc, LLVMTypeRef type);
    void addStrAttr(std::string name, LLVMAttributeIndex index, LLVMValueRef ptr, int loc, std::string value = "");
    Type* setByTypeList(std::vector<Type*> list);

    int getAlignment(Type* type);
};

class Scope {
public:
    std::unordered_map<std::string, RaveValue> localScope;
    std::unordered_map<std::string, int> args;
    std::string funcName;
    LLVMBasicBlockRef blockExit;
    bool funcHasRet = false;
    std::unordered_map<std::string, NodeVar*> localVars;
    std::unordered_map<std::string, NodeVar*> argVars;
    std::unordered_map<std::string, Node*> aliasTable;
    LLVMBasicBlockRef fnEnd;
    std::vector<Node*> defers;

    Scope(std::string funcName, std::unordered_map<std::string, int> args, std::unordered_map<std::string, NodeVar*> argVars);

    RaveValue get(std::string name, int loc = -1);
    RaveValue getWithoutLoad(std::string name, int loc = -1);
    NodeVar* getVar(std::string name, int loc = -1);

    // Helper: get the struct type of "this" variable, returns nullptr if not applicable
    TypeStruct* getThisStructType(int loc = -1);

    bool has(std::string name);
    bool hasAtThis(std::string name);
    bool locatedAtThis(std::string name);
    void hasChanged(std::string name);
    void remove(std::string name);
};

extern LLVMGen* generator;
extern Scope* currScope;
extern LLVMTargetDataRef dataLayout;

extern TypeFunc* callToTFunc(NodeCall* call);
extern Scope* copyScope(Scope* original);
extern std::string typeToString(LLVMTypeRef type);
