/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

// NodeFunc generation logic - LLVM IR generation for functions
// Split from NodeFunc.cpp for better maintainability

#include "../../include/parser/nodes/NodeFunc.hpp"
#include "../../include/parser/nodes/NodeRet.hpp"
#include "../../include/parser/nodes/NodeIden.hpp"
#include "../../include/parser/nodes/NodeVar.hpp"
#include "../../include/parser/nodes/NodeNull.hpp"
#include "../../include/parser/nodes/NodeArray.hpp"
#include "../../include/parser/nodes/NodeString.hpp"
#include "../../include/parser/nodes/NodeBool.hpp"
#include "../../include/parser/nodes/NodeStruct.hpp"
#include "../../include/parser/ast.hpp"
#include "../../include/parser/FuncRegistry.hpp"
#include "../../include/compiler.hpp"
#include "../../include/debug.hpp"
#include <llvm-c/Analysis.h>
#include <llvm-c/DebugInfo.h>
#include <llvm-c/Comdat.h>

// Helper: get type by size for Win64/cdecl64 ABI
Type* getTypeBySize(int size) {
    if (size == 8) return basicTypes[BasicType::Char];
    else if (size == 16) return basicTypes[BasicType::Short];
    else if (size == 32) return basicTypes[BasicType::Int];
    else if (size == 64) return basicTypes[BasicType::Long];
    else return basicTypes[BasicType::Cent];
}

void NodeFunc::processModifiers(int& callConv, NodeArray*& conditions) {
    DEBUG_LOG(Debug::Category::CodeGen, "Processing modifiers for function: " + name);

    for (size_t i = 0; i < mods.size(); i++) {
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
            if (!instanceof<NodeString>(newLinkName))
                generator->error("value type of \033[1mlinkname\033[22m must be a string!", loc);
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
    DEBUG_LOG(Debug::Category::CodeGen, "Creating LLVM function: " + name);

    generator->functions[name] = {LLVMAddFunction(
        generator->lModule, linkName.c_str(),
        LLVMFunctionType(generator->genType(tfunc->main, loc), genTypes.data(), genTypes.size(), isVararg)
    ), tfunc};

    if (isPrivate && !isExtern) {
        LLVMSetLinkage(generator->functions[name].value, LLVMInternalLinkage);
        if (generator->settings.optLevel > 2 && !generator->settings.noPrivateInlining) isInline = true;
    }

    LLVMSetFunctionCallConv(generator->functions[name].value, callConv);

    size_t inTCount = 0;
    for (size_t i = 0; i < args.size(); i++) {
        if (!(args[i].internalTypes.empty())) {
            for (size_t j = 0; j < args[i].internalTypes.size(); j++) {
                inTCount += 1;
                if (!(args[i].internalTypes.empty()) && instanceof<TypeByval>(args[i].internalTypes[0])) {
                    generator->addTypeAttr("byval", inTCount, generator->functions[name].value, loc, generator->genType(args[i].type, loc));
                    generator->addAttr("align", inTCount, generator->functions[name].value, loc, 8);
                }
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
    if (!generator->settings.outDebugInfo) return;

    std::vector<LLVMMetadataRef> mTypes;
    for (size_t i = 0; i < args.size(); i++) mTypes.push_back(debugInfo->genType(args[i].type, loc));

    LLVMMetadataRef funcType = LLVMDIBuilderCreateSubroutineType(
        debugInfo->diBuilder, debugInfo->diFile, mTypes.data(), mTypes.size(), LLVMDIFlagZero);

    diFuncScope = LLVMDIBuilderCreateFunction(debugInfo->diBuilder, debugInfo->diScope,
        name.c_str(), name.length(), linkName.c_str(), linkName.length(), debugInfo->diFile,
        loc, funcType, isPrivate, !isExtern, loc, LLVMDIFlagZero, generator->settings.optLevel > 0);
}

LLVMTypeRef* NodeFunc::getParameters(int callConv) {
    std::vector<LLVMTypeRef> buffer;

    for (size_t i = 0; i < args.size(); i++) {
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
                                    arg.internalTypes[0] = isCdecl64
                                        ? new TypeDivided(new TypeVector(basicTypes[BasicType::Float], 2), {basicTypes[BasicType::Float], basicTypes[BasicType::Float]})
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
                        if (isCdecl64) arg.internalTypes[0] = new TypeByval(arg.type);
                        else arg.type = new TypePointer(arg.type);
                    }
                }
            }
            else if (block != nullptr) {
                block->nodes.insert(block->nodes.begin(),
                    new NodeVar(oldName, new NodeIden(arg.name, loc), false, false, false, {}, loc, arg.type, false, false, false));
            }
        }

        if (isCdecl64) {
            for (int j = 0; j < arg.internalTypes.size(); j++) {
                if (instanceof<TypeDivided>(arg.internalTypes[j]))
                    buffer.push_back(generator->genType(((TypeDivided*)arg.internalTypes[j])->mainType, loc));
                else buffer.push_back(generator->genType(arg.internalTypes[j], loc));
            }
        }
        else buffer.push_back(generator->genType(arg.type, loc));

        args[i] = arg;
    }

    genTypes = buffer;
    return genTypes.data();
}

RaveValue NodeFunc::generate() {
    DEBUG_LOG(Debug::Category::CodeGen, "Generating function: " + name);

    if (noCompile) return {};
    if (!templateNames.empty() && !isTemplate) {
        isExtern = false;
        return {};
    }

    Types::replaceComptime(type);
    for (size_t i = 0; i < args.size(); i++) Types::replaceComptime(args[i].type);

    linkName = generator->mangle(name, true, isMethod);
    int callConv = LLVMCCallConv;
    NodeArray* conditions = nullptr;

    if (name == "main") {
        linkName = "main";
        if (instanceof<TypeVoid>(type)) type = basicTypes[BasicType::Int];
    }

    processModifiers(callConv, conditions);

    if (!isTemplate && isCtargs) return {};
    if (isForwardDeclaration) return {};

    std::map<std::string, Type*> oldReplace = std::map<std::string, Type*>(generator->toReplace);
    if (isTemplate) {
        generator->toReplace.clear();
        for (size_t i = 0; i < templateNames.size(); i++)
            generator->toReplace[templateNames[i]] = templateTypes[i];
    }

    if (conditions != nullptr) {
        for (Node* node : conditions->values) {
            Node* result = node->comptime();
            if (instanceof<NodeBool>(result) && !((NodeBool*)result)->value) {
                generator->error("The conditions were failed when generating the function \033[1m" + name + "\033[22m!", loc);
                return {};
            }
        }
    }

    getParameters(callConv);
    Types::replaceTemplates(&type);

    TypeFunc* tfunc = new TypeFunc(type, {}, isVararg);

    // Build function type arguments
    if (name.find('<') != std::string::npos && name.find("([]") != std::string::npos) {
        args[0].type = new TypePointer(new TypeStruct(name.substr(0, name.find('('))));
        tfunc->args.push_back(new TypeFuncArg(args[0].type, args[0].name));

        for (size_t i = 1; i < args.size(); i++) {
            Type* cp = args[i].type;
            Types::replaceTemplates(&cp);
            tfunc->args.push_back(new TypeFuncArg(cp, args[i].name));
        }
    }
    else {
        for (size_t i = 0; i < args.size(); i++) {
            if (!args[i].internalTypes.empty() && instanceof<TypeDivided>(args[i].internalTypes[0])) {
                tfunc->args.push_back(new TypeFuncArg(((TypeDivided*)args[i].internalTypes[0])->mainType, args[i].name));
                continue;
            }
            Type* cp = args[i].type;
            Types::replaceTemplates(&cp);
            tfunc->args.push_back(new TypeFuncArg(cp, args[i].name));
        }
    }

    LLVMValueRef fn = LLVMGetNamedFunction(generator->lModule, linkName.c_str());
    if (fn != nullptr) {
        generator->functions[name] = {fn, tfunc};
        return {};
    }

    createLLVMFunction(tfunc, callConv);
    createDebugInfo();

    if (!isExtern) {
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
        for (size_t i = 0; i < args.size(); i++) {
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

        // Ensure all basic blocks have terminators
        uint32_t bbLength = LLVMCountBasicBlocks(generator->functions[currScope->funcName].value);
        LLVMBasicBlockRef* basicBlocks = (LLVMBasicBlockRef*)malloc(sizeof(LLVMBasicBlockRef) * bbLength);
        LLVMGetBasicBlocks(generator->functions[currScope->funcName].value, basicBlocks);
        for (size_t i = 0; i < bbLength; i++) {
            if (basicBlocks[i] != nullptr && std::string(LLVMGetBasicBlockName(basicBlocks[i])) != "exit" &&
                LLVMGetBasicBlockTerminator(basicBlocks[i]) == nullptr) {
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
    DEBUG_LOG(Debug::Category::CodeGen, "Generating function with ctargs: " + name);

    std::vector<FuncArgSet> newArgs;
    for (size_t i = 0; i < args.size(); i++) {
        Type* newType = args[i];
        if (instanceof<TypePointer>(newType) && instanceof<TypeArray>(newType->getElType()))
            newType = new TypePointer(newType->getElType()->getElType());
        newArgs.push_back(FuncArgSet{.name = "_RaveArg" + std::to_string(i), .type = newType, .internalTypes = {newType}});
    }

    auto activeLoops = std::map<int32_t, Loop>(generator->activeLoops);
    auto builder = generator->builder;
    auto currBB = generator->currBB;
    auto _scope = currScope;

    std::vector<DeclarMod> _mods;
    for (size_t i = 0; i < mods.size(); i++) if (mods[i].name != "ctargs") _mods.push_back(mods[i]);

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
    DEBUG_LOG(Debug::Category::Template, "Generating function with template: " + all);

    auto activeLoops = std::map<int32_t, Loop>(generator->activeLoops);
    auto builder = generator->builder;
    auto currBB = generator->currBB;
    auto _scope = currScope;

    NodeFunc* _f = new NodeFunc(all, args, (NodeBlock*)block->copy(), isExtern, mods, loc, type->copy(), templateNames);
    _f->isTemplate = true;
    _f->isMethod = isMethod;
    _f->structContext = structContext;
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