/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include "../../llvm-c/Core.h"
#include "Node.hpp"
#include "NodeBlock.hpp"
#include "../parser.hpp"
#include "../Types.hpp"
#include <vector>
#include <string>

class NodeRet;
struct RetGenStmt;

class NodeFunc : public Node {
public:
    std::string name;
    std::string origName;
    std::string linkName;
    std::vector<std::string> namespacesNames;
    std::vector<std::string> templateNames;
    std::vector<Type*> templateTypes;

    std::vector<FuncArgSet> args;
    std::vector<FuncArgSet> origArgs;

    NodeBlock* block;

    std::vector<NodeRet*> rets;
    std::vector<RetGenStmt> genRets;
    std::vector<DeclarMod> mods;
    long loc;
    Type* type;

    LLVMBasicBlockRef exitBlock;
    LLVMBuilderRef builder;
    std::vector<LLVMTypeRef> genTypes;

    bool isMethod = false;
    bool isVararg = false;
    bool isInline = false;
    bool isTemplatePart = false;
    bool isTemplate = false;
    bool isCtargs = false;
    bool isCtargsPart = false;
    bool isComdat = false;
    bool isSafe = false;
    bool isFakeMethod = false;
    bool isNoNamespaces = false;
    bool isNoChecks = false;
    bool isPrivate = false;
    bool isNoOpt = false;
    bool noCompile = false;
    bool isExtern = false;
    bool isCdecl64 = false;

    NodeFunc(std::string name, std::vector<FuncArgSet> args, NodeBlock* block, bool isExtern, std::vector<DeclarMod> mods, long loc, Type* type, std::vector<std::string> templateNames);
    LLVMTypeRef* getParameters(int callConv);
    LLVMValueRef generate() override;
    Type* getType() override;
    Node* comptime() override;
    Node* copy() override;
    void check() override;
    bool isReleased(int n);
    std::string generateWithCtargs(std::vector<LLVMTypeRef> args);
    LLVMValueRef generateWithTemplate(std::vector<Type*> types, std::string all);
};