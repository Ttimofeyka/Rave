/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

// NodeFunc class - Function definition and checking
// Split for better maintainability - see also NodeFuncGen.cpp for generation

#include "../../include/parser/nodes/NodeFunc.hpp"
#include "../../include/parser/nodes/NodeRet.hpp"
#include "../../include/parser/nodes/NodeIden.hpp"
#include "../../include/parser/nodes/NodeVar.hpp"
#include "../../include/parser/ast.hpp"
#include "../../include/parser/FuncRegistry.hpp"
#include "../../include/debug.hpp"

NodeFunc::NodeFunc(const std::string& name, std::vector<FuncArgSet> args, NodeBlock* block,
                   bool isExtern, std::vector<DeclarMod> mods, int loc, Type* type, std::vector<std::string> templateNames) {
    this->name = name;
    this->origName = name;
    this->args = std::vector<FuncArgSet>(args);
    this->origArgs = std::vector<FuncArgSet>(args);
    this->block = block != nullptr ? new NodeBlock(block->nodes) : block;
    this->isExtern = isExtern;
    this->mods = mods;
    this->loc = loc;
    this->type = type;
    this->templateNames = templateNames;
    this->isArrayable = false;
    this->isNoCopy = false;
    this->diFuncScope = nullptr;
    this->isExplicit = false;

    for (size_t i = 0; i < mods.size(); i++) {
        if (mods[i].name == "private") this->isPrivate = true;
        else if (mods[i].name == "noCopy") this->isNoCopy = true;
        else if (this->mods[i].name == "ctargs") this->isCtargs = true;
    }
}

NodeFunc::~NodeFunc() {
    if (block != nullptr) delete block;
    if (type != nullptr && !instanceof<TypeVoid>(type) && !instanceof<TypeBasic>(type)) { delete type; type = nullptr; }
    for (size_t i = 0; i < args.size(); i++)
        if (args[i].type != nullptr && !instanceof<TypeVoid>(args[i].type) && !instanceof<TypeBasic>(args[i].type))
            delete args[i].type;
}

void NodeFunc::check() {
    if (isChecked) return;
    isChecked = true;
    DEBUG_LOG(Debug::Category::Parser, "Checking function: " + name);

    // Handle possible templates and void
    Types::replaceTemplates(&type);

    for (auto& arg : args) {
        Types::replaceTemplates(&arg.type);
        if (instanceof<TypeVoid>(arg.type))
            AST::checkError("using \033[1mvoid\033[22m as a variable type is prohibited!", loc);
    }

    // Process modifiers
    for (const auto& mod : mods) {
        if (mod.name == "method") {
            name = "{" + args[0].type->toString() + "}" + name;
            structContext = args[0].type->toString();
        }
        else if (mod.name == "noNamespaces") isNoNamespaces = true;
        else if (mod.name == "vararg") isVararg = true;
        else if (mod.name == "cdecl64") isCdecl64 = true;
        else if (mod.name == "win64") isWin64 = true;
    }

    // Handle namespaces
    if (!namespacesNames.empty() && !isNoNamespaces) name = namespacesToString(namespacesNames, name);

    // Check for existing function
    if (auto it = AST::funcTable.find(name); it != AST::funcTable.end()) {
        if (it->second->isForwardDeclaration) {
            isInfluencedByFD = true;
            AST::funcTable[name] = this;

            for (size_t i = 0; i < AST::funcVersionsTable[name].size(); i++) {
                if (AST::funcVersionsTable[name][i]->isForwardDeclaration) {
                    AST::funcVersionsTable[name].erase(AST::funcVersionsTable[name].begin() + i);
                    break;
                }
            }

            mangledName = name;
            if (!structContext.empty()) FuncRegistry::instance().registerMethod(this, structContext);
            else FuncRegistry::instance().registerFunc(this);

            for (auto node : block->nodes) node->check();
            return;
        }

        std::string argTypes = typesToString(args);
        if (typesToString(it->second->args) == argTypes) {
            if (isCtargs || isCtargsPart || isTemplate) {
                noCompile = true;
                return;
            }
            if (!it->second->isForwardDeclaration)
                AST::checkError("a function with name \033[1m" + name + "\033[22m already exists on \033[1m" +
                    std::to_string(it->second->loc) + "\033[22m line!", loc);
            AST::funcVersionsTable[name].push_back(this);
        }
        else {
            AST::funcVersionsTable[name].push_back(this);
            name += argTypes;
        }
    }
    else AST::funcVersionsTable[name].push_back(this);

    // Store the mangled name for LLVM
    mangledName = name;

    // Register function in legacy table
    AST::funcTable[name] = this;

    // Register with FuncRegistry
    if (!structContext.empty()) FuncRegistry::instance().registerMethod(this, structContext);
    else FuncRegistry::instance().registerFunc(this);

    // Check block contents
    if (block == nullptr) {
        if (!isExtern) isForwardDeclaration = true;
    }
    else {
        for (auto node : block->nodes) node->check();
    }
}

Node* NodeFunc::comptime() { return this; }
Type* NodeFunc::getType() { return type; }

Node* NodeFunc::copy() {
    std::vector<FuncArgSet> args = this->args;
    for (size_t i = 0; i < args.size(); i++) args[i].type = args[i].type->copy();

    NodeFunc* fn = new NodeFunc(name, args, (block == nullptr ? nullptr : (NodeBlock*)block->copy()),
        isExtern, mods, loc, type->copy(), templateNames);
    fn->isExplicit = isExplicit;
    return fn;
}

Type* NodeFunc::getInternalArgType(LLVMValueRef value) {
    std::string __n = LLVMPrintValueToString(value);
    int n = std::stoi(__n.substr(__n.find(" %") + 2));
    int nArg = 0;

    for (size_t i = 0; i < args.size(); i++) {
        for (int j = 0; j < args[i].internalTypes.size(); j++) {
            if (nArg == n) return args[i].internalTypes[j];
            nArg += 1;
        }
    }
    return nullptr;
}

Type* NodeFunc::getArgType(int n) { return args[n].type; }

Type* NodeFunc::getArgType(std::string name) {
    for (size_t i = 0; i < args.size(); i++) {
        if (args[i].name == name) return args[i].type;
    }
    return nullptr;
}

FuncSignature NodeFunc::getSignature() const {
    std::vector<Type*> paramTypes;
    for (const auto& arg : args) paramTypes.push_back(arg.type);
    return FuncSignature(origName, paramTypes, structContext);
}

std::string NodeFunc::computeMangledName() const {
    if (!mangledName.empty()) return mangledName;

    std::string result;

    if (!structContext.empty()) result = "{" + structContext + "}";
    result += origName;

    if (!args.empty()) result += typesToString(const_cast<std::vector<FuncArgSet>&>(args));

    return result;
}