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
#include <llvm-c/Comdat.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/DebugInfo.h>
#include <fstream>
#include "../../include/compiler.hpp"
#include "../../include/llvm.hpp"

Type* getTypeBySize(int size) {
    if(size == 8) return basicTypes[BasicType::Char];
    else if(size == 16) return basicTypes[BasicType::Short];
    else if(size == 32) return basicTypes[BasicType::Int];
    else if(size == 64) return basicTypes[BasicType::Long];
    else return basicTypes[BasicType::Cent];
}

NodeFunc::NodeFunc(const std::string& name, std::vector<FuncArgSet> args, NodeBlock* block, bool isExtern, std::vector<DeclarMod> mods, int loc, Type* type, std::vector<std::string> templateNames) {
    this->name = name;
    this->origName = name;
    this->args = std::vector<FuncArgSet>(args);
    this->origArgs = std::vector<FuncArgSet>(args);
    this->block = new NodeBlock(block->nodes);
    this->isExtern = isExtern;
    this->mods = mods;
    this->loc = loc;
    this->type = type;
    this->templateNames = templateNames;
    this->isArrayable = false;
    this->isNoCopy = false;
    this->diFuncScope = nullptr;
    this->isExplicit = false;

    for(size_t i=0; i<mods.size(); i++) {
        if(mods[i].name == "private") this->isPrivate = true;
        else if(mods[i].name == "noCopy") this->isNoCopy = true;
        else if(this->mods[i].name == "ctargs") this->isCtargs = true;
    }
}

NodeFunc::~NodeFunc() {
    if(block != nullptr) delete block;
    if(type != nullptr && !instanceof<TypeVoid>(type) && !instanceof<TypeBasic>(type)) {delete type; type = nullptr;}
    for(size_t i=0; i<args.size(); i++) if(args[i].type != nullptr && !instanceof<TypeVoid>(args[i].type) && !instanceof<TypeBasic>(args[i].type)) delete args[i].type;
}

void NodeFunc::check() {
    if(isChecked) return;
    isChecked = true;

    // Handle possible templates and void
    Types::replaceTemplates(&type);

    for(auto& arg : args) {
        Types::replaceTemplates(&arg.type);

        if(instanceof<TypeVoid>(arg.type)) AST::checkError("using \033[1mvoid\033[22m as a variable type is prohibited!", loc);
    }

    // Process modifiers
    for(const auto& mod : mods) {
        if(mod.name == "method") name = "{" + args[0].type->toString() + "}" + name;
        else if (mod.name == "noNamespaces") isNoNamespaces = true;
    }

    // Handle namespaces
    if(!namespacesNames.empty() && !isNoNamespaces) name = namespacesToString(namespacesNames, name);

    // Check for existing function
    if(auto it = AST::funcTable.find(name); it != AST::funcTable.end()) {
        std::string argTypes = typesToString(args);
        if(typesToString(it->second->args) == argTypes) {
            if (isCtargs || isCtargsPart || isTemplate) {
                noCompile = true;
                return;
            }

            AST::checkError("a function with name \033[1m" + name + "\033[22m already exists on \033[1m" + std::to_string(it->second->loc) + "\033[22m line!", loc);
        }

        AST::funcVersionsTable[name].push_back(this);
        name += argTypes;
    }
    else AST::funcVersionsTable[name].push_back(this);

    // Register function
    AST::funcTable[name] = this;

    // Check block contents
    for(auto node : block->nodes) node->check();
}

LLVMTypeRef* NodeFunc::getParameters(int callConv) {
    std::vector<LLVMTypeRef> buffer;

    for(size_t i=0; i<args.size(); i++) {
        FuncArgSet arg = args[i];

        while(instanceof<TypeConst>(arg.type)) arg.type = ((TypeConst*)arg.type)->instance;

        if(instanceof<TypeStruct>(arg.type) || instanceof<TypeBasic>(arg.type)) {
            std::string oldName = arg.name;
            arg.name = "_RaveArgument" + arg.name;

            if(isCdecl64 || isWin64) {
                if(instanceof<TypeStruct>(arg.type)) {
                    int tSize = arg.type->getSize();

                    if(((TypeStruct*)arg.type)->isSimple()) {
                        TypeBasic* tArgType = (TypeBasic*)AST::structTable[arg.type->toString()]->variables[0]->type;
                        int tElCount = ((TypeStruct*)arg.type)->getElCount();

                        if(tElCount == 2) {
                            if(tArgType->isFloat()) {
                                if(isCdecl64)
                                    arg.internalTypes[0] = new TypeDivided(new TypeVector(basicTypes[BasicType::Float], 2), {basicTypes[BasicType::Float], basicTypes[BasicType::Float]});
                                else
                                    arg.internalTypes[0] = new TypeDivided(basicTypes[BasicType::Long], {basicTypes[BasicType::Float], basicTypes[BasicType::Float]});
                            }
                            else arg.internalTypes[0] = new TypeDivided(getTypeBySize(tSize), {getTypeBySize(tSize / 2), getTypeBySize(tSize / 2)});
                        }
                        else if(tElCount == 3) {
                            if(isWin64 && tSize >= 96) arg.type = new TypePointer(arg.type);
                            else arg.internalTypes[0] = new TypeDivided(getTypeBySize(tSize), {getTypeBySize(tSize / 3), getTypeBySize(tSize / 3), getTypeBySize(tSize / 3)});
                        }
                        else if(tElCount == 4) {
                            if(isWin64 && tSize >= 96) arg.type = new TypePointer(arg.type);
                            else arg.internalTypes[0] = new TypeDivided(getTypeBySize(tSize), {getTypeBySize(tSize / 4), getTypeBySize(tSize / 4), getTypeBySize(tSize / 4), getTypeBySize(tSize / 4)});
                        }
                        else if((tSize >= 128) || (tSize >= 96 && isWin64)) {
                            if(isCdecl64) arg.internalTypes[0] = new TypeByval(arg.type);
                            else arg.type = new TypePointer(arg.type);
                        }
                    }
                    else if(tSize >= 128) {
                        // 'byval' pointer
                        if(isCdecl64) arg.internalTypes[0] = new TypeByval(arg.type);
                        else arg.type = new TypePointer(arg.type);
                    }
                }
            }
            else this->block->nodes.emplace(
                this->block->nodes.begin(),
                new NodeVar(oldName, new NodeIden(arg.name, this->loc), false, false, false, {}, this->loc, arg.type, false, false, false)
            );
        }

        if(isCdecl64) for(int j=0; j<arg.internalTypes.size(); j++) {
            if(instanceof<TypeDivided>(arg.internalTypes[j])) buffer.push_back(generator->genType(((TypeDivided*)arg.internalTypes[j])->mainType, loc));
            else buffer.push_back(generator->genType(arg.internalTypes[j], loc));
        }
        else buffer.push_back(generator->genType(arg.type, loc));

        args[i] = arg;
    }

    this->genTypes = buffer;
    return this->genTypes.data();
}

RaveValue NodeFunc::generate() {
    if(this->noCompile) return {};
    if(!this->templateNames.empty() && !this->isTemplate) {
        this->isExtern = false;
        return {};
    }

    Types::replaceComptime(type);

    for(size_t i=0; i<args.size(); i++) Types::replaceComptime(args[i].type);

    linkName = generator->mangle(this->name, true, this->isMethod);
    int callConv = LLVMCCallConv;
    NodeArray* conditions = nullptr;

    if(this->name == "main") {
        linkName = "main";
        if(instanceof<TypeVoid>(this->type)) this->type = basicTypes[BasicType::Int];
    }

    for(size_t i=0; i<mods.size(); i++) {
        while(AST::aliasTable.find(mods[i].name) != AST::aliasTable.end()) {
            if(instanceof<NodeArray>(AST::aliasTable[mods[i].name])) {
                NodeArray* array = (NodeArray*)AST::aliasTable[mods[i].name];
                mods[i].name = ((NodeString*)array->values[0])->value;
                mods[i].value = array->values[1];
            }
            else mods[i].name = ((NodeString*)AST::aliasTable[mods[i].name])->value;
        }

        if(mods[i].name == "C") linkName = name;
        else if(mods[i].name == "vararg") isVararg = true;
        else if(mods[i].name == "fastcc") callConv = LLVMFastCallConv;
        else if(mods[i].name == "coldcc") callConv = LLVMColdCallConv;
        else if(mods[i].name == "stdcc") callConv = LLVMX86StdcallCallConv;
        else if(mods[i].name == "armapcs") callConv = LLVMARMAPCSCallConv;
        else if(mods[i].name == "armaapcs") callConv = LLVMARMAAPCSCallConv;
        else if(mods[i].name == "cdecl64") {callConv = LLVMCCallConv; isCdecl64 = true;}
        else if(mods[i].name == "win64") {callConv = LLVMCCallConv; isWin64 = true;}
        else if(mods[i].name == "inline") isInline = true;
        else if(mods[i].name == "linkname") {
            Node* newLinkName = mods[i].value->comptime();
            if(!instanceof<NodeString>(newLinkName)) generator->error("value type of \033[1mlinkname\033[22m must be a string!", loc);
            linkName = ((NodeString*)newLinkName)->value;
        }
        else if(mods[i].name == "comdat") isComdat = true;
        else if(mods[i].name == "nochecks") isNoChecks = true;
        else if(mods[i].name == "noOptimize") isNoOpt = true;
        else if(mods[i].name == "arrayable") isArrayable = true;
        else if(mods[i].name == "conditions") conditions = (NodeArray*)mods[i].value;
        else if(mods[i].name == "explicit") isExplicit = true;
    }

    if(!this->isTemplate && this->isCtargs) return {};

    std::map<std::string, Type*> oldReplace = std::map<std::string, Type*>(generator->toReplace);
    if(this->isTemplate) {
        generator->toReplace.clear();
        for(size_t i=0; i<this->templateNames.size(); i++) generator->toReplace[this->templateNames[i]] = this->templateTypes[i];
    }

    if(conditions != nullptr) for(Node* node : conditions->values) {
        Node* result = node->comptime();
        if(instanceof<NodeBool>(result) && !((NodeBool*)result)->value) {
            generator->error("The conditions were failed when generating the function \033[1m" + this->name + "\033[22m!", this->loc);
            return {};
        }
    }
    
    this->getParameters(callConv);

    TypeFunc* tfunc = new TypeFunc(this->type, {}, this->isVararg);

    Type* parent = nullptr;
    Type* ty = type;

    while(instanceof<TypePointer>(ty) || instanceof<TypeArray>(ty)) {
        parent = ty;
        ty = ty->getElType();
    }

    if(instanceof<TypeStruct>(ty)) {
        if(generator->toReplace.find(ty->toString()) != generator->toReplace.end()) {
            if(parent == nullptr) this->type = generator->toReplace[ty->toString()];
            else if(instanceof<TypePointer>(parent)) ((TypePointer*)parent)->instance = generator->toReplace[ty->toString()];
            else ((TypeArray*)parent)->element = generator->toReplace[ty->toString()];
        }
    }

    if(name.find('<') != std::string::npos && name.find("([]") != std::string::npos) {
        args[0].type = new TypePointer(new TypeStruct(name.substr(0, name.find('('))));
        tfunc->args.push_back(new TypeFuncArg(args[0].type, args[0].name));

        for(int i=1; i<args.size(); i++) {
            Type* cp = args[i].type;
            Types::replaceTemplates(&cp);

            tfunc->args.push_back(new TypeFuncArg(cp, args[i].name));
        }
    }
    else for(size_t i=0; i<args.size(); i++) {
        if(!args[i].internalTypes.empty() && instanceof<TypeDivided>(args[i].internalTypes[0])) {
            tfunc->args.push_back(new TypeFuncArg(((TypeDivided*)args[i].internalTypes[0])->mainType, args[i].name));
            continue;
        }

        Type* cp = args[i].type;
        Types::replaceTemplates(&cp);

        tfunc->args.push_back(new TypeFuncArg(cp, args[i].name));
    }

    tfunc->main = this->type;

    LLVMValueRef fn = LLVMGetNamedFunction(generator->lModule, linkName.c_str());
    if(fn != nullptr) return {};

    generator->functions[this->name] = {LLVMAddFunction(
        generator->lModule, linkName.c_str(),
        LLVMFunctionType(generator->genType(this->type, loc), this->genTypes.data(), this->genTypes.size(), this->isVararg)
    ), tfunc};

    if(isPrivate && !isExtern) {
        LLVMSetLinkage(generator->functions[this->name].value, LLVMInternalLinkage);

        // Inlining of private functions
        if(generator->settings.optLevel > 2 && generator->settings.noPrivateInlining) this->isInline = true;
    }

    LLVMSetFunctionCallConv(generator->functions[this->name].value, callConv);

    int inTCount = 0;

    for(size_t i=0; i<args.size(); i++) {
        if(!(args[i].internalTypes.empty())) for(int j=0; j<args[i].internalTypes.size(); j++) {
            inTCount += 1;
            if(!(args[i].internalTypes.empty()) && instanceof<TypeByval>(args[i].internalTypes[0])) {
                generator->addTypeAttr("byval", inTCount, generator->functions[this->name].value, this->loc, generator->genType(args[i].type, loc));
                generator->addAttr("align", inTCount, generator->functions[this->name].value, this->loc, 8);
            }
        }
        else inTCount += 1;
    }

    if(this->isInline) generator->addAttr("alwaysinline", LLVMAttributeFunctionIndex, generator->functions[this->name].value, this->loc);
    else if(this->isNoOpt) {
        generator->addAttr("noinline", LLVMAttributeFunctionIndex, generator->functions[this->name].value, this->loc);
        generator->addAttr("optnone", LLVMAttributeFunctionIndex, generator->functions[this->name].value, this->loc);
    }

    if(this->isTemplatePart || this->isTemplate || this->isCtargsPart || this->isComdat) {
        LLVMComdatRef comdat = LLVMGetOrInsertComdat(generator->lModule, linkName.c_str());
        LLVMSetComdatSelectionKind(comdat, LLVMAnyComdatSelectionKind);
        LLVMSetComdat(generator->functions[this->name].value, comdat);
        LLVMSetLinkage(generator->functions[this->name].value, LLVMLinkOnceODRLinkage);
        this->isExtern = false;
    }

    if(generator->settings.outDebugInfo) {
        std::vector<LLVMMetadataRef> mTypes;
        for(size_t i=0; i<args.size(); i++) mTypes.push_back(debugInfo->genType(args[i].type, loc));

        LLVMMetadataRef funcType = LLVMDIBuilderCreateSubroutineType(debugInfo->diBuilder, debugInfo->diFile, mTypes.data(), mTypes.size(), LLVMDIFlagZero);

        diFuncScope = LLVMDIBuilderCreateFunction(debugInfo->diBuilder, debugInfo->diScope, name.c_str(), name.length(), linkName.c_str(), linkName.length(), debugInfo->diFile,
            loc, funcType, isPrivate, !isExtern, loc, LLVMDIFlagZero, generator->settings.optLevel > 0);
    }

    if(!this->isExtern) {
        int oldCurrentBuiltinArg = generator->currentBuiltinArg;
        if(this->isCtargsPart || this->isCtargs) generator->currentBuiltinArg = 0;

        LLVMBasicBlockRef entry = LLVM::makeBlock("entry", name);
        this->exitBlock = LLVM::makeBlock("exit", name);
        this->builder = LLVMCreateBuilderInContext(generator->context);

        LLVMBuilderRef oldBuilder = generator->builder;
        generator->builder = this->builder;

        LLVMBasicBlockRef oldCurrBB = generator->currBB;
        LLVM::Builder::atEnd(entry);

        if(!Compiler::settings.noFastMath) LLVM::setFastMath(generator->builder, true, false, true, true);

        std::map<std::string, int> indexes;
        std::map<std::string, NodeVar*> vars;
        for(size_t i=0; i<this->args.size(); i++) {
            indexes.insert({this->args[i].name, i});
            vars.insert({this->args[i].name, new NodeVar(this->args[i].name, nullptr, false, true, false, {}, this->loc, this->args[i].type)});
        }

        Scope* oldScope = currScope;

        currScope = new Scope(this->name, indexes, vars);
        generator->currBB = entry;
        currScope->fnEnd = this->exitBlock;

        if(!instanceof<TypeVoid>(this->type)) {
            this->block->nodes.insert(this->block->nodes.begin(), new NodeVar("return", new NodeNull(this->type, this->loc), false, false, false, {}, this->loc, this->type, false));
            ((NodeVar*)this->block->nodes[0])->isUsed = true;
        }

        this->block->generate();

        if(!this->isCtargs && !this->isCtargsPart && !Compiler::settings.disableWarnings) block->optimize();

        currScope->fnEnd = this->exitBlock;

        if(!currScope->funcHasRet) LLVMBuildBr(generator->builder, this->exitBlock);

        LLVMMoveBasicBlockAfter(this->exitBlock, LLVMGetLastBasicBlock(generator->functions[this->name].value));

        uint32_t bbLength = LLVMCountBasicBlocks(generator->functions[currScope->funcName].value);
        LLVMBasicBlockRef* basicBlocks = (LLVMBasicBlockRef*)malloc(sizeof(LLVMBasicBlockRef) * bbLength);
        LLVMGetBasicBlocks(generator->functions[currScope->funcName].value, basicBlocks);
            for(size_t i=0; i<bbLength; i++) {
                if(basicBlocks[i] != nullptr && std::string(LLVMGetBasicBlockName(basicBlocks[i])) != "exit" && LLVMGetBasicBlockTerminator(basicBlocks[i]) == nullptr) {
                    LLVM::Builder::atEnd(basicBlocks[i]);
                    LLVMBuildBr(generator->builder, this->exitBlock);
                }
            }
        free(basicBlocks);

        LLVM::Builder::atEnd(exitBlock);

        if(!instanceof<TypeVoid>(this->type)) LLVMBuildRet(generator->builder, currScope->get("return", this->loc).value);
        else LLVMBuildRetVoid(generator->builder);

        generator->builder = oldBuilder;
        currScope = oldScope;
        generator->currBB = oldCurrBB;

        generator->currentBuiltinArg = oldCurrentBuiltinArg;
    }

    if(this->isTemplate) generator->toReplace = std::map<std::string, Type*>(oldReplace);

    if(generator->settings.outDebugInfo) LLVMDIBuilderFinalizeSubprogram(debugInfo->diBuilder, diFuncScope);

    if(LLVMVerifyFunction(generator->functions[this->name].value, LLVMPrintMessageAction)) {
        std::string content = LLVMPrintValueToString(generator->functions[this->name].value);
        if(content.length() > 12000) content = content.substr(0, 12000) + "...";
        generator->error("LLVM errors into the function \033[1m" + this->name + "\033[22m! Content:\n" + content, this->loc);
    }

    return generator->functions[this->name];
}

std::string NodeFunc::generateWithCtargs(std::vector<Type*> args) {
    std::vector<FuncArgSet> newArgs;
    for(size_t i=0; i<args.size(); i++) {
        Type* newType = args[i];
        if(instanceof<TypePointer>(newType) && instanceof<TypeArray>(newType->getElType())) newType = new TypePointer(newType->getElType()->getElType());
    
        newArgs.push_back(FuncArgSet{.name = "_RaveArg" + std::to_string(i), .type = newType, .internalTypes = {newType}});
    }

    auto activeLoops = std::map<int32_t, Loop>(generator->activeLoops);
    auto builder = generator->builder;
    auto currBB = generator->currBB;
    auto _scope = currScope;

    std::vector<DeclarMod> _mods;
    for(size_t i=0; i<mods.size(); i++) if(mods[i].name != "ctargs") _mods.push_back(mods[i]);

    NodeFunc* _f = new NodeFunc(name + typesToString(newArgs), newArgs, (NodeBlock*)this->block->copy(), false, _mods, this->loc, this->type->copy(), {});
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

    NodeFunc* _f = new NodeFunc(all, this->args, (NodeBlock*)this->block->copy(), false, this->mods, this->loc, this->type->copy(), this->templateNames);
    _f->isTemplate = true;
    _f->check();
    _f->templateTypes = std::move(types);
    RaveValue v = _f->generate();

    generator->activeLoops = activeLoops;
    generator->builder = builder;
    generator->currBB = currBB;
    currScope = _scope;

    return v;
}

Node* NodeFunc::comptime() {return this;}
Type* NodeFunc::getType() {return this->type;}

Node* NodeFunc::copy() {
    std::vector<FuncArgSet> args = this->args;
    for(size_t i=0; i<args.size(); i++) args[i].type = args[i].type->copy();

    NodeFunc* fn = new NodeFunc(this->name, args, (NodeBlock*)this->block->copy(), this->isExtern, this->mods, this->loc, this->type->copy(), this->templateNames);
    fn->isExplicit = isExplicit;
    return fn;
}

Type* NodeFunc::getInternalArgType(LLVMValueRef value) {
    std::string __n = LLVMPrintValueToString(value);
    int n = std::stoi(__n.substr(__n.find(" %") + 2));
    int nArg = 0;

    for(size_t i=0; i<args.size(); i++) {
        for(int j=0; j<args[i].internalTypes.size(); j++) {
            if(nArg == n) return args[i].internalTypes[j];
            nArg += 1;
        }
    }

    return nullptr;
}

Type* NodeFunc::getArgType(int n) {
    return args[n].type;;
}

Type* NodeFunc::getArgType(std::string name) {
    for(size_t i=0; i<args.size(); i++) {
        if(args[i].name == name) return args[i].type;
    }
    return nullptr;
}
