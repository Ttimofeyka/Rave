/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "../../include/parser/ast.hpp"
#include <vector>
#include <string>
#include "../../include/parser/nodes/NodeFunc.hpp"
#include "../../include/parser/nodes/NodeRet.hpp"
#include "../../include/parser/nodes/NodeIden.hpp"
#include "../../include/parser/nodes/NodeVar.hpp"
#include "../../include/parser/nodes/NodeBuiltin.hpp"
#include "../../include/parser/nodes/NodeInt.hpp"
#include "../../include/parser/nodes/NodeArray.hpp"
#include "../../include/parser/nodes/NodeString.hpp"
#include "../../include/parser/nodes/NodeIf.hpp"
#include "../../include/parser/nodes/NodeWhile.hpp"
#include "../../include/parser/nodes/NodeUnary.hpp"
#include "../../include/parser/nodes/NodeFor.hpp"
#include "../../include/parser/nodes/NodeNull.hpp"
#include "../../include/parser/nodes/NodeDone.hpp"
#include "../../include/parser/nodes/NodeBool.hpp"
#include "../../include/parser/nodes/NodeStruct.hpp"
#include "../../include/parser/nodes/NodeBinary.hpp"
#include "../../include/parser/nodes/NodeGet.hpp"
#include "../../include/parser/nodes/NodeIndex.hpp"
#include "../../include/parser/nodes/NodeCast.hpp"
#include "../../include/parser/nodes/NodeCall.hpp"
#include "../../include/parser/nodes/NodeImport.hpp"
#include "../../include/parser/nodes/NodeForeach.hpp"
#include <llvm-c/Analysis.h>
#include <llvm-c/DebugInfo.h>
#include <fstream>
#include <llvm-c/Comdat.h>
#include "../../include/compiler.hpp"
#include "../../include/llvm.hpp"

Type* getTypeBySize(int size) {
    if (size == 8) return basicTypes[BasicType::Char];
    else if (size == 16) return basicTypes[BasicType::Short];
    else if (size == 32) return basicTypes[BasicType::Int];
    else if (size == 64) return basicTypes[BasicType::Long];
    else return basicTypes[BasicType::Cent];
}

void NodeFunc::processModifiers(int& callConv, NodeArray*& conditions) {
    for (size_t i=0; i<mods.size(); i++) {
        while (AST::aliasTable.find(mods[i].name) != AST::aliasTable.end()) {
            if (instanceof<NodeArray>(AST::aliasTable[mods[i].name])) {
                NodeArray* array = (NodeArray*)AST::aliasTable[mods[i].name];
                mods[i].name = ((NodeString*)array->values[0])->value;
                mods[i].value = array->values[1];
            }
            else mods[i].name = ((NodeString*)AST::aliasTable[mods[i].name])->value;
        }

        if (mods[i].name == "C") linkName = name;
        else if (mods[i].name == "vararg") isVararg = true;
        else if (mods[i].name == "fastcc") callConv = LLVMFastCallConv;
        else if (mods[i].name == "coldcc") callConv = LLVMColdCallConv;
        else if (mods[i].name == "stdcc") callConv = LLVMX86StdcallCallConv;
        else if (mods[i].name == "armapcs") callConv = LLVMARMAPCSCallConv;
        else if (mods[i].name == "armaapcs") callConv = LLVMARMAAPCSCallConv;
        else if (mods[i].name == "cdecl64") { callConv = LLVMCCallConv; isCdecl64 = true; }
        else if (mods[i].name == "win64") { callConv = LLVMCCallConv; isWin64 = true; }
        else if (mods[i].name == "inline") isInline = true;
        else if (mods[i].name == "linkname") {
            Node* newLinkName = mods[i].value->comptime();
            if (!instanceof<NodeString>(newLinkName)) generator->error("value type of \033[1mlinkname\033[22m must be a string!", loc);
            linkName = ((NodeString*)newLinkName)->value;
        }
        else if (mods[i].name == "comdat") isComdat = true;
        else if (mods[i].name == "nochecks") isNoChecks = true;
        else if (mods[i].name == "noOptimize") isNoOpt = true;
        else if (mods[i].name == "arrayable") isArrayable = true;
        else if (mods[i].name == "conditions") conditions = (NodeArray*)mods[i].value;
        else if (mods[i].name == "explicit") isExplicit = true;
    }
}

void NodeFunc::createLLVMFunction(TypeFunc* tfunc, int callConv) {
    generator->functions[name] = {LLVMAddFunction(
        generator->lModule, linkName.c_str(),
        LLVMFunctionType(generator->genType(tfunc->main, loc), genTypes.data(), genTypes.size(), isVararg)
    ), tfunc};

    if (isPrivate && !isExtern) {
        LLVMSetLinkage(generator->functions[name].value, LLVMInternalLinkage);

        // Inlining of private functions
        if (generator->settings.optLevel > 2 && !generator->settings.noPrivateInlining) isInline = true;
    }

    LLVMSetFunctionCallConv(generator->functions[name].value, callConv);

    size_t inTCount = 0;

    for (size_t i=0; i<args.size(); i++) {
        if (!(args[i].internalTypes.empty())) for (size_t j=0; j<args[i].internalTypes.size(); j++) {
            inTCount += 1;
            if (!(args[i].internalTypes.empty()) && instanceof<TypeByval>(args[i].internalTypes[0])) {
                generator->addTypeAttr("byval", inTCount, generator->functions[name].value, loc, generator->genType(args[i].type, loc));
                generator->addAttr("align", inTCount, generator->functions[name].value, loc, 8);
            }
        }
        else inTCount += 1;
    }

    if (isInline) generator->addAttr("alwaysinline", LLVMAttributeFunctionIndex, generator->functions[name].value, loc);
    else if (isNoOpt) {
        generator->addAttr("noinline", LLVMAttributeFunctionIndex, generator->functions[name].value, loc);
        generator->addAttr("optnone", LLVMAttributeFunctionIndex, generator->functions[name].value, loc);
    }

    if (isTemplatePart || isTemplate || isCtargsPart || isComdat) {
        LLVMComdatRef comdat = LLVMGetOrInsertComdat(generator->lModule, linkName.c_str());
        LLVMSetComdatSelectionKind(comdat, LLVMAnyComdatSelectionKind);
        LLVMSetComdat(generator->functions[name].value, comdat);
        LLVMSetLinkage(generator->functions[name].value, LLVMLinkOnceODRLinkage);
        isExtern = false;
    }
}

void NodeFunc::createDebugInfo() {
    if (generator->settings.outDebugInfo) {
        std::vector<LLVMMetadataRef> mTypes;
        for (size_t i=0; i<args.size(); i++) mTypes.push_back(debugInfo->genType(args[i].type, loc));

        LLVMMetadataRef funcType = LLVMDIBuilderCreateSubroutineType(debugInfo->diBuilder, debugInfo->diFile, mTypes.data(), mTypes.size(), LLVMDIFlagZero);

        diFuncScope = LLVMDIBuilderCreateFunction(debugInfo->diBuilder, debugInfo->diScope, name.c_str(), name.length(), linkName.c_str(), linkName.length(), debugInfo->diFile,
            loc, funcType, isPrivate, !isExtern, loc, LLVMDIFlagZero, generator->settings.optLevel > 0);
    }
}

NodeFunc::NodeFunc(const std::string& name, std::vector<FuncArgSet> args, NodeBlock* block, bool isExtern, std::vector<DeclarMod> mods, int loc, Type* type, std::vector<std::string> templateNames) {
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

    for (size_t i=0; i<mods.size(); i++) {
        if (mods[i].name == "private") this->isPrivate = true;
        else if (mods[i].name == "noCopy") this->isNoCopy = true;
        else if (this->mods[i].name == "ctargs") this->isCtargs = true;
    }
}

NodeFunc::~NodeFunc() {
    if (block != nullptr) delete block;
    if (type != nullptr && !instanceof<TypeVoid>(type) && !instanceof<TypeBasic>(type)) {delete type; type = nullptr;}
    for (size_t i=0; i<args.size(); i++) if (args[i].type != nullptr && !instanceof<TypeVoid>(args[i].type) && !instanceof<TypeBasic>(args[i].type)) delete args[i].type;
}

void NodeFunc::check() {
    if (isChecked) return;
    isChecked = true;

    // Handle possible templates and void
    Types::replaceTemplates(&type);

    for (auto& arg : args) {
        Types::replaceTemplates(&arg.type);

        if (instanceof<TypeVoid>(arg.type)) AST::checkError("using \033[1mvoid\033[22m as a variable type is prohibited!", loc);
    }

    // Process modifiers
    for (const auto& mod : mods) {
        if (mod.name == "method") name = "{" + args[0].type->toString() + "}" + name;
        else if (mod.name == "noNamespaces") isNoNamespaces = true;
    }

    // Handle namespaces
    if (!namespacesNames.empty() && !isNoNamespaces) name = namespacesToString(namespacesNames, name);

    // Check for existing function
    if (auto it = AST::funcTable.find(name); it != AST::funcTable.end()) {
        std::string argTypes = typesToString(args);
        if (typesToString(it->second->args) == argTypes) {
            if (isCtargs || isCtargsPart || isTemplate) {
                noCompile = true;
                return;
            }

            if (!it->second->isForwardDeclaration) AST::checkError("a function with name \033[1m" + name + "\033[22m already exists on \033[1m" + std::to_string(it->second->loc) + "\033[22m line!", loc);
            AST::funcVersionsTable[name].push_back(this);
        }
        else {
            AST::funcVersionsTable[name].push_back(this);
            name += argTypes;
        }
    }
    else AST::funcVersionsTable[name].push_back(this);

    // Register function
    AST::funcTable[name] = this;

    // Check block contents
    if (block == nullptr) {
        if (!isExtern) isForwardDeclaration = true;
    }
    else {
        for (auto node : block->nodes) node->check();
    }
}

LLVMTypeRef* NodeFunc::getParameters(int callConv) {
    std::vector<LLVMTypeRef> buffer;

    for (size_t i=0; i<args.size(); i++) {
        FuncArgSet arg = args[i];

        while (instanceof<TypeConst>(arg.type)) arg.type = ((TypeConst*)arg.type)->instance;

        if (instanceof<TypeStruct>(arg.type) || instanceof<TypeBasic>(arg.type)) {
            std::string oldName = arg.name;
            arg.name = "_RaveArgument" + arg.name;

            if (isCdecl64 || isWin64) {
                if (instanceof<TypeStruct>(arg.type)) {
                    int tSize = arg.type->getSize();

                    if (((TypeStruct*)arg.type)->isSimple()) {
                        TypeBasic* tArgType = (TypeBasic*)AST::structTable[arg.type->toString()]->variables[0]->type;
                        int tElCount = ((TypeStruct*)arg.type)->getElCount();

                        switch (tElCount) {
                            case 2:
                                if (tArgType->isFloat())
                                    arg.internalTypes[0] = isCdecl64 ? new TypeDivided(new TypeVector(basicTypes[BasicType::Float], 2), {basicTypes[BasicType::Float], basicTypes[BasicType::Float]})
                                        : new TypeDivided(basicTypes[BasicType::Long], {basicTypes[BasicType::Float], basicTypes[BasicType::Float]});
                                else arg.internalTypes[0] = new TypeDivided(getTypeBySize(tSize), {getTypeBySize(tSize / 2), getTypeBySize(tSize / 2)});
                                break;
                            case 3: case 4:
                                if (isWin64 && tSize >= 96) arg.type = new TypePointer(arg.type);
                                else arg.internalTypes[0] = new TypeDivided(getTypeBySize(tSize), {getTypeBySize(tSize / tElCount), getTypeBySize(tSize / tElCount), getTypeBySize(tSize / tElCount)});
                                break;
                            default:
                                if ((tSize >= 128) || (tSize >= 96 && isWin64)) {
                                    if (isCdecl64) arg.internalTypes[0] = new TypeByval(arg.type);
                                    else arg.type = new TypePointer(arg.type);
                                }
                                break;
                        }
                    }
                    else if (tSize >= 128) {
                        // 'byval' pointer
                        if (isCdecl64) arg.internalTypes[0] = new TypeByval(arg.type);
                        else arg.type = new TypePointer(arg.type);
                    }
                }
            }
            else if (block != nullptr) block->nodes.emplace(
                block->nodes.begin(),
                new NodeVar(oldName, new NodeIden(arg.name, loc), false, false, false, {}, loc, arg.type, false, false, false)
            );
        }

        if (isCdecl64) for (int j=0; j<arg.internalTypes.size(); j++) {
            if (instanceof<TypeDivided>(arg.internalTypes[j])) buffer.push_back(generator->genType(((TypeDivided*)arg.internalTypes[j])->mainType, loc));
            else buffer.push_back(generator->genType(arg.internalTypes[j], loc));
        }
        else buffer.push_back(generator->genType(arg.type, loc));

        args[i] = arg;
    }

    genTypes = buffer;
    return genTypes.data();
}

RaveValue NodeFunc::generate() {
    if (noCompile) return {};
    if (!templateNames.empty() && !isTemplate) {
        isExtern = false;
        return {};
    }

    Types::replaceComptime(type);

    for (size_t i=0; i<args.size(); i++) Types::replaceComptime(args[i].type);

    linkName = generator->mangle(name, true, isMethod);
    int callConv = LLVMCCallConv;
    NodeArray* conditions = nullptr;

    if (name == "main") {
        linkName = "main";
        if (instanceof<TypeVoid>(type)) type = basicTypes[BasicType::Int];
    }

    processModifiers(callConv, conditions);

    if (!isTemplate && isCtargs) return {};

    std::map<std::string, Type*> oldReplace = std::map<std::string, Type*>(generator->toReplace);
    if (isTemplate) {
        generator->toReplace.clear();
        for (size_t i=0; i<templateNames.size(); i++) generator->toReplace[templateNames[i]] = templateTypes[i];
    }

    if (conditions != nullptr) for (Node* node : conditions->values) {
        Node* result = node->comptime();
        if (instanceof<NodeBool>(result) && !((NodeBool*)result)->value) {
            generator->error("The conditions were failed when generating the function \033[1m" + name + "\033[22m!", loc);
            return {};
        }
    }

    getParameters(callConv);

    Types::replaceTemplates(&type);

    TypeFunc* tfunc = new TypeFunc(type, {}, isVararg);

    if (name.find('<') != std::string::npos && name.find("([]") != std::string::npos) {
        args[0].type = new TypePointer(new TypeStruct(name.substr(0, name.find('('))));
        tfunc->args.push_back(new TypeFuncArg(args[0].type, args[0].name));

        for (size_t i=1; i<args.size(); i++) {
            Type* cp = args[i].type;
            Types::replaceTemplates(&cp);

            tfunc->args.push_back(new TypeFuncArg(cp, args[i].name));
        }
    }
    else for (size_t i=0; i<args.size(); i++) {
        if (!args[i].internalTypes.empty() && instanceof<TypeDivided>(args[i].internalTypes[0])) {
            tfunc->args.push_back(new TypeFuncArg(((TypeDivided*)args[i].internalTypes[0])->mainType, args[i].name));
            continue;
        }

        Type* cp = args[i].type;
        Types::replaceTemplates(&cp);

        tfunc->args.push_back(new TypeFuncArg(cp, args[i].name));
    }

    LLVMValueRef fn = LLVMGetNamedFunction(generator->lModule, linkName.c_str());
    if (fn != nullptr) return {};

    createLLVMFunction(tfunc, callConv);
    createDebugInfo();

    if (!isExtern) {
        if (isForwardDeclaration) return {};

        int oldCurrentBuiltinArg = generator->currentBuiltinArg;
        if (isCtargsPart || isCtargs) generator->currentBuiltinArg = 0;

        LLVMBasicBlockRef entry = LLVM::makeBlock("entry", name);
        exitBlock = LLVM::makeBlock("exit", name);
        builder = LLVMCreateBuilderInContext(generator->context);

        LLVMBuilderRef oldBuilder = generator->builder;
        generator->builder = builder;

        LLVMBasicBlockRef oldCurrBB = generator->currBB;
        LLVM::Builder::atEnd(entry);

        if (!Compiler::settings.noFastMath) LLVM::setFastMath(generator->builder, true, false, true, true);

        std::map<std::string, int> indexes;
        std::map<std::string, NodeVar*> vars;
        for (size_t i=0; i<args.size(); i++) {
            indexes.insert({args[i].name, i});
            vars.insert({args[i].name, new NodeVar(args[i].name, nullptr, false, true, false, {}, loc, args[i].type, false, false, false)});
        }

        Scope* oldScope = currScope;

        currScope = new Scope(name, indexes, vars);
        generator->currBB = entry;
        currScope->fnEnd = exitBlock;

        if (!instanceof<TypeVoid>(type)) {
            block->nodes.insert(block->nodes.begin(), new NodeVar("return", new NodeNull(type, loc), false, false, false, {}, loc, type, false, false, false));
            ((NodeVar*)block->nodes[0])->isUsed = true;
        }

        block->generate();

        if (!isCtargs && !isCtargsPart && !Compiler::settings.disableWarnings) block->optimize();

        currScope->fnEnd = exitBlock;

        if (!currScope->funcHasRet) LLVMBuildBr(generator->builder, exitBlock);

        LLVMMoveBasicBlockAfter(exitBlock, LLVMGetLastBasicBlock(generator->functions[name].value));

        uint32_t bbLength = LLVMCountBasicBlocks(generator->functions[currScope->funcName].value);
        LLVMBasicBlockRef* basicBlocks = (LLVMBasicBlockRef*)malloc(sizeof(LLVMBasicBlockRef) * bbLength);
        LLVMGetBasicBlocks(generator->functions[currScope->funcName].value, basicBlocks);
            for (size_t i=0; i<bbLength; i++) {
                if (basicBlocks[i] != nullptr && std::string(LLVMGetBasicBlockName(basicBlocks[i])) != "exit" && LLVMGetBasicBlockTerminator(basicBlocks[i]) == nullptr) {
                    LLVM::Builder::atEnd(basicBlocks[i]);
                    LLVMBuildBr(generator->builder, exitBlock);
                }
            }
        free(basicBlocks);

        LLVM::Builder::atEnd(exitBlock);

        if (!instanceof<TypeVoid>(type)) LLVMBuildRet(generator->builder, currScope->get("return", loc).value);
        else LLVMBuildRetVoid(generator->builder);

        currScope = oldScope;
        generator->builder = oldBuilder;
        generator->currBB = oldCurrBB;
        generator->currentBuiltinArg = oldCurrentBuiltinArg;
    }

    if (isTemplate) generator->toReplace = std::map<std::string, Type*>(oldReplace);

    if (generator->settings.outDebugInfo) LLVMDIBuilderFinalizeSubprogram(debugInfo->diBuilder, diFuncScope);

    if (LLVMVerifyFunction(generator->functions[name].value, LLVMPrintMessageAction)) {
        std::string content = LLVMPrintValueToString(generator->functions[name].value);
        if (content.length() > 12000) content = content.substr(0, 12000) + "...";
        generator->error("LLVM errors into the function \033[1m" + name + "\033[22m! Content:\n" + content, loc);
    }

    return generator->functions[name];
}

std::string NodeFunc::generateWithCtargs(std::vector<Type*> args) {
    std::vector<FuncArgSet> newArgs;
    for (size_t i=0; i<args.size(); i++) {
        Type* newType = args[i];
        if (instanceof<TypePointer>(newType) && instanceof<TypeArray>(newType->getElType())) newType = new TypePointer(newType->getElType()->getElType());
    
        newArgs.push_back(FuncArgSet{.name = "_RaveArg" + std::to_string(i), .type = newType, .internalTypes = {newType}});
    }

    auto activeLoops = std::map<int32_t, Loop>(generator->activeLoops);
    auto builder = generator->builder;
    auto currBB = generator->currBB;
    auto _scope = currScope;

    std::vector<DeclarMod> _mods;
    for (size_t i=0; i<mods.size(); i++) if (mods[i].name != "ctargs") _mods.push_back(mods[i]);

    NodeFunc* _f = new NodeFunc(name + typesToString(newArgs), newArgs, (NodeBlock*)block->copy(), false, _mods, loc, type->copy(), {});
    _f->args = std::vector<FuncArgSet>(newArgs);
    _f->isCtargsPart = true;
    std::string _n = _f->name;
    _f->check();
    _f->generate();

    generator->activeLoops = activeLoops;
    generator->builder = builder;
    generator->currBB = currBB;
    currScope = _scope;

    return _n;
}

RaveValue NodeFunc::generateWithTemplate(std::vector<Type*>& types, const std::string& all) {
    auto activeLoops = std::map<int32_t, Loop>(generator->activeLoops);
    auto builder = generator->builder;
    auto currBB = generator->currBB;
    auto _scope = currScope;

    NodeFunc* _f = new NodeFunc(all, args, (NodeBlock*)block->copy(), isExtern, mods, loc, type->copy(), templateNames);
    _f->isTemplate = true;
    _f->isMethod = isMethod;
    _f->check();
    _f->templateTypes = std::move(types);
    RaveValue v = _f->generate();

    if (isMethod) {
        Type* structType = instanceof<TypePointer>(args[0].type) ? args[0].type->getElType() : args[0].type;
        AST::methodTable[std::pair<std::string, std::string>(structType->toString(), all)] = _f;
    }

    generator->activeLoops = activeLoops;
    generator->builder = builder;
    generator->currBB = currBB;
    currScope = _scope;

    return v;
}

Node* NodeFunc::comptime() { return this; }
Type* NodeFunc::getType() { return type; }

Node* NodeFunc::copy() {
    std::vector<FuncArgSet> args = this->args;
    for (size_t i=0; i<args.size(); i++) args[i].type = args[i].type->copy();

    NodeFunc* fn = new NodeFunc(name, args, (block == nullptr ? nullptr : (NodeBlock*)block->copy()), isExtern, mods, loc, type->copy(), templateNames);
    fn->isExplicit = isExplicit;
    return fn;
}

Type* NodeFunc::getInternalArgType(LLVMValueRef value) {
    std::string __n = LLVMPrintValueToString(value);
    int n = std::stoi(__n.substr(__n.find(" %") + 2));
    int nArg = 0;

    for (size_t i=0; i<args.size(); i++) {
        for (int j=0; j<args[i].internalTypes.size(); j++) {
            if (nArg == n) return args[i].internalTypes[j];
            nArg += 1;
        }
    }

    return nullptr;
}

Type* NodeFunc::getArgType(int n) { return args[n].type; }

Type* NodeFunc::getArgType(std::string name) {
    for (size_t i=0; i<args.size(); i++) {
        if (args[i].name == name) return args[i].type;
    }
    return nullptr;
}
