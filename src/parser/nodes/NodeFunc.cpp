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

    for(int i=0; i<mods.size(); i++) {
        if(mods[i].name == "private") this->isPrivate = true;
        else if(mods[i].name == "noCopy") this->isNoCopy = true;
        else if(this->mods[i].name == "ctargs") this->isCtargs = true;
    }
}

NodeFunc::~NodeFunc() {
    if(block != nullptr) delete block;
    if(type != nullptr && !instanceof<TypeVoid>(type) && !instanceof<TypeBasic>(type)) {delete type; type = nullptr;}
    for(int i=0; i<args.size(); i++) if(args[i].type != nullptr && !instanceof<TypeVoid>(args[i].type) && !instanceof<TypeBasic>(args[i].type)) delete args[i].type;
}

void NodeFunc::check() {
    bool oldCheck = this->isChecked;
    this->isChecked = true;

    if(!oldCheck) {
        for(int i=0; i<this->mods.size(); i++) {
            if(this->mods[i].name == "method") {
                Type* structure = this->args[0].type;
                this->name = "{" + structure->toString() + "}" + this->name;
            }
            else if(mods[i].name == "noNamespaces") this->isNoNamespaces = true;
        }

        if(this->namespacesNames.size() > 0 && !this->isNoNamespaces) name = namespacesToString(namespacesNames, this->name);

        if(!instanceof<TypeBasic>(this->type) && !AST::aliasTypes.empty()) {
            Type* nt = this->type->check(nullptr);
            if(nt != nullptr) this->type = nt;
        }

        for(int i=0; i<this->args.size(); i++) {
            if(this->args[i].type != nullptr && generator != nullptr) {
                while(generator->toReplace.find(this->args[i].type->toString()) != generator->toReplace.end())
                    this->args[i].type = generator->toReplace[this->args[i].type->toString()];
            }
        }

        if(instanceof<TypeStruct>(this->type) && generator != nullptr) {
            TypeStruct* ts = (TypeStruct*)this->type;
            for(int i=0; i<ts->types.size(); i++) {
                if(ts->types[i] != nullptr) {
                    if(generator->toReplace.find(ts->types[i]->toString() + "@") != generator->toReplace.end()) ts->types[i] = generator->toReplace[ts->types[i]->toString() + "@"];
                    else while(generator->toReplace.find(ts->types[i]->toString()) != generator->toReplace.end()) {
                        ts->types[i] = generator->toReplace[ts->types[i]->toString()];
                    }
                }
            }
            if(ts->types.size() > 0) ts->updateByTypes();
            if(ts != nullptr && generator != nullptr) {
                while(generator->toReplace.find(this->type->toString()) != generator->toReplace.end())
                    this->type = generator->toReplace[this->type->toString()];
            }
        }

        if(AST::funcTable.find(this->name) != AST::funcTable.end()) {
            std::string toAdd = typesToString(args);
            if(typesToString(AST::funcTable[name]->args) == toAdd) {
                if(this->isCtargs || this->isCtargsPart || this->isTemplate) {
                    this->noCompile = true;
                    return;
                }
                else {
                    AST::checkError("a function with '" + this->name + "' name already exists on " + std::to_string(AST::funcTable[this->name]->loc) + " line!", this->loc);
                    return;
                }
            }
            this->name += toAdd;
        }

        AST::funcTable[this->name] = this;
        for(int i=0; i<this->block->nodes.size(); i++) {
            this->block->nodes[i]->check();
        }
    }
}

LLVMTypeRef* NodeFunc::getParameters(int callConv) {
    std::vector<LLVMTypeRef> buffer;

    for(int i=0; i<args.size(); i++) {
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

    linkName = generator->mangle(this->name, true, this->isMethod);
    int callConv = LLVMCCallConv;
    NodeArray* conditions = nullptr;

    if(this->name == "main") {
        linkName = "main";
        if(instanceof<TypeVoid>(this->type)) this->type = basicTypes[BasicType::Int];
    }

    for(int i=0; i<this->mods.size(); i++) {
        while(AST::aliasTable.find(mods[i].name) != AST::aliasTable.end()) {
            if(instanceof<NodeArray>(AST::aliasTable[this->mods[i].name])) {
                NodeArray* array = (NodeArray*)AST::aliasTable[this->mods[i].name];
                this->mods[i].name = ((NodeString*)array->values[0])->value;
                this->mods[i].value = array->values[1];
            }
            else this->mods[i].name = ((NodeString*)AST::aliasTable[this->mods[i].name])->value;
        }

        if(this->mods[i].name == "C") linkName = this->name;
        else if(this->mods[i].name == "vararg") this->isVararg = true;
        else if(this->mods[i].name == "fastcc") callConv = LLVMFastCallConv;
        else if(this->mods[i].name == "coldcc") callConv = LLVMColdCallConv;
        else if(this->mods[i].name == "stdcc") callConv = LLVMX86StdcallCallConv;
        else if(this->mods[i].name == "armapcs") callConv = LLVMARMAPCSCallConv;
        else if(this->mods[i].name == "armaapcs") callConv = LLVMARMAAPCSCallConv;
        else if(this->mods[i].name == "cdecl64") {callConv = LLVMCCallConv; this->isCdecl64 = true;}
        else if(this->mods[i].name == "win64") {callConv = LLVMCCallConv; this->isWin64 = true;}
        else if(this->mods[i].name == "inline") this->isInline = true;
        else if(this->mods[i].name == "linkname") {
            Node* newLinkName = this->mods[i].value->comptime();
            if(!instanceof<NodeString>(newLinkName)) generator->error("value type of 'linkname' must be a string!", loc);
            linkName = ((NodeString*)newLinkName)->value;
        }
        else if(this->mods[i].name == "comdat") this->isComdat = true;
        else if(this->mods[i].name == "nochecks") this->isNoChecks = true;
        else if(this->mods[i].name == "noOptimize") this->isNoOpt = true;
        else if(this->mods[i].name == "arrayable") this->isArrayable = true;
        else if(this->mods[i].name == "conditions") conditions = (NodeArray*)this->mods[i].value;
    }

    if(!this->isTemplate && this->isCtargs) return {};

    std::map<std::string, Type*> oldReplace = std::map<std::string, Type*>(generator->toReplace);
    if(this->isTemplate) {
        generator->toReplace.clear();
        for(int i=0; i<this->templateNames.size(); i++) generator->toReplace[this->templateNames[i]] = this->templateTypes[i];
    }

    if(conditions != nullptr) for(Node* node : conditions->values) {
        Node* result = node->comptime();
        if(instanceof<NodeBool>(result) && !((NodeBool*)result)->value) {
            generator->error("The conditions were failed when generating the function '" + this->name + "'!", this->loc);
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
            Template::replaceTemplates(&cp);

            tfunc->args.push_back(new TypeFuncArg(cp, args[i].name));
        }
    }
    else for(int i=0; i<args.size(); i++) {
        if(!args[i].internalTypes.empty() && instanceof<TypeDivided>(args[i].internalTypes[0])) {
            tfunc->args.push_back(new TypeFuncArg(((TypeDivided*)args[i].internalTypes[0])->mainType, args[i].name));
            continue;
        }

        Type* cp = args[i].type;
        Template::replaceTemplates(&cp);

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

    for(int i=0; i<args.size(); i++) {
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
        for(int i=0; i<args.size(); i++) mTypes.push_back(debugInfo->genType(args[i].type, loc));

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
        for(int i=0; i<this->args.size(); i++) {
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
            for(int i=0; i<bbLength; i++) {
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
        generator->error("LLVM errors into the function '" + this->name + "'! Content:\n" + content, this->loc);
    }

    return generator->functions[this->name];
}

std::string NodeFunc::generateWithCtargs(std::vector<Type*> args) {
    std::vector<FuncArgSet> newArgs;
    for(int i=0; i<args.size(); i++) {
        Type* newType = args[i];
        if(instanceof<TypePointer>(newType) && instanceof<TypeArray>(newType->getElType())) newType = new TypePointer(newType->getElType()->getElType());
    
        newArgs.push_back(FuncArgSet{.name = "_RaveArg" + std::to_string(i), .type = newType, .internalTypes = {newType}});
    }

    auto activeLoops = std::map<int32_t, Loop>(generator->activeLoops);
    auto builder = generator->builder;
    auto currBB = generator->currBB;
    auto _scope = currScope;

    std::vector<DeclarMod> _mods;
    for(int i=0; i<mods.size(); i++) if(mods[i].name != "ctargs") _mods.push_back(mods[i]);

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

Node* NodeFunc::copy() {return new NodeFunc(this->name, this->args, (NodeBlock*)this->block->copy(), this->isExtern, this->mods, this->loc, this->type->copy(), this->templateNames);}

Type* NodeFunc::getInternalArgType(LLVMValueRef value) {
    std::string __n = LLVMPrintValueToString(value);
    int n = std::stoi(__n.substr(__n.find(" %") + 2));
    int nArg = 0;

    for(int i=0; i<args.size(); i++) {
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
    for(int i=0; i<args.size(); i++) {
        if(args[i].name == name) return args[i].type;
    }
    return nullptr;
}