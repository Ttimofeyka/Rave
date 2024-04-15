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
#include "../../include/llvm-c/Comdat.h"
#include "../../include/llvm-c/Analysis.h"

NodeFunc::NodeFunc(std::string name, std::vector<FuncArgSet> args, NodeBlock* block, bool isExtern, std::vector<DeclarMod> mods, long loc, Type* type, std::vector<std::string> templateNames) {
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
    for(int i=0; i<mods.size(); i++) {
        if(mods[i].name == "private") this->isPrivate = true;
    }
}

void NodeFunc::check() {
    bool oldCheck = this->isChecked;
    this->isChecked = true;
    if(!oldCheck) {
        for(int i=0; i<this->mods.size(); i++) {
            if(this->mods[i].name == "method") {
                Type* structure = this->args[0].type;
                this->name = "{"+structure->toString()+"}"+this->name;
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
                    while(generator->toReplace.find(ts->types[i]->toString()) != generator->toReplace.end()) {
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
                AST::checkError("a function with '"+this->name+"' name already exists on "+std::to_string(AST::funcTable[this->name]->loc)+" line!", this->loc);
                return;
            }
            this->name += toAdd;
        }

        AST::funcTable[this->name] = this;
        for(int i=0; i<this->block->nodes.size(); i++) {
            if(instanceof<NodeRet>(this->block->nodes[i])) ((NodeRet*)this->block->nodes[i])->parent = this->name;
            else if(instanceof<NodeIf>(this->block->nodes[i])) ((NodeIf*)this->block->nodes[i])->funcName = this->name;
            else if(instanceof<NodeWhile>(this->block->nodes[i])) ((NodeWhile*)this->block->nodes[i])->funcName = this->name;
            else if(instanceof<NodeFor>(this->block->nodes[i])) ((NodeFor*)this->block->nodes[i])->funcName = this->name;
            this->block->nodes[i]->check();
        }
    }
}

LLVMTypeRef* NodeFunc::getParameters(int callConv) {
    std::vector<LLVMTypeRef> buffer;
    for(int i=0; i<this->args.size(); i++) {
        if(instanceof<TypeStruct>(this->args[i].type)) {
            if(generator->toReplace.find(((TypeStruct*)(this->args[i].type))->name) == generator->toReplace.end()) {
                std::string oldName = this->args[i].name;
                this->args[i].name = "_RaveStructArgument"+oldName;
                if(callConv == -1) {
                    // Cdecl64
                    int size = this->args[i].type->getSize();
                    Type* ty = nullptr;
                    switch(size) {
                        case 8: ty = new TypeBasic(BasicType::Char); break;
                        case 16: ty = new TypeBasic(BasicType::Short); break;
                        case 32: ty = new TypeBasic(BasicType::Int); break;
                        case 40: ty = new TypeBasic(BasicType::Long); break;
                        default: this->block->nodes.insert(this->block->nodes.begin(), new NodeVar(oldName, new NodeIden(this->args[i].name, this->loc), false, false, false, {}, this->loc, this->args[i].type)); break;
                    }
                    if(ty != nullptr) {
                        this->args[i].type = ty;
                        this->block->nodes.insert(this->block->nodes.begin(), new NodeVar(oldName, new NodeIden(this->args[i].name, this->loc), false, false, false, {}, this->loc, ty));
                        buffer.push_back(generator->genType(ty, this->loc));
                        continue;
                    }
                }
                else this->block->nodes.insert(this->block->nodes.begin(), new NodeVar(oldName, new NodeIden(this->args[i].name, this->loc), false, false, false, {}, this->loc, this->args[i].type));
            }
        }
        else if(!instanceof<TypePointer>(this->args[i].type) && !instanceof<TypeConst>(this->args[i].type) && !instanceof<TypeFunc>(this->args[i].type)) {
            std::string oldName = this->args[i].name;
            this->args[i].name = "_RaveTempVariable"+oldName;
            this->block->nodes.insert(this->block->nodes.begin(), new NodeVar(oldName, new NodeIden(this->args[i].name, this->loc), false, false, false, {}, this->loc, this->args[i].type, false));
        }
        buffer.push_back(generator->genType(this->args[i].type, this->loc));
    }
    this->genTypes = std::vector<LLVMTypeRef>(buffer);
    return buffer.data();
}

LLVMValueRef NodeFunc::generate() {
    if(this->noCompile) return nullptr;
    if(!this->templateNames.empty() && !this->isTemplate) {
        this->isExtern = false;
        return nullptr;
    }

    linkName = generator->mangle(this->name, true, this->isMethod);
    int callConv = LLVMCCallConv;
    std::map<std::string, NodeBuiltin*> builtins;

    if(this->name == "main") {
        linkName = "main";
        if(instanceof<TypeVoid>(this->type)) this->type = new TypeBasic(BasicType::Int);
    }

    for(int i=0; i<this->mods.size(); i++) {
        while(AST::aliasTable.find(mods[i].name) != AST::aliasTable.end()) {
            if(instanceof<NodeArray>(AST::aliasTable[this->mods[i].name])) {
                NodeArray* array = (NodeArray*)AST::aliasTable[this->mods[i].name];
                this->mods[i].name = ((NodeString*)array->values[0])->value;
                this->mods[i].value = ((NodeString*)array->values[1])->value;
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
        else if(this->mods[i].name == "cdecl64") {callConv = -1; this->isCdecl64 = true;}
        else if(this->mods[i].name == "inline") this->isInline = true;
        else if(this->mods[i].name == "linkname") linkName = this->mods[i].value;
        else if(this->mods[i].name == "ctargs") this->isCtargs = true;
        else if(this->mods[i].name == "comdat") this->isComdat = true;
        else if(this->mods[i].name == "safe") this->isSafe = true;
        else if(this->mods[i].name == "nochecks") this->isNoChecks = true;
        else if(this->mods[i].name == "noOptimize") this->isNoOpt = true;
        else if(this->mods[i].name.size() > 0 && this->mods[i].name[0] == '@') builtins[this->mods[i].name.substr(1)] = ((NodeBuiltin*)this->mods[i].genValue);
    }
    if(!this->isTemplate && this->isCtargs) return nullptr;

    std::map<std::string, Type*> oldReplace = std::map<std::string, Type*>(generator->toReplace);
    if(this->isTemplate) {
        generator->toReplace.clear();
        for(int i=0; i<this->templateNames.size(); i++) generator->toReplace[this->templateNames[i]] = this->templateTypes[i];
    }

    for(const auto &data : builtins) {
        if(data.second->generate() != LLVMConstInt(LLVMInt1TypeInContext(generator->context), 1, false)) {
            generator->error("The '"+data.first+"' builtin failed when generating the function '"+this->name+"'!", this->loc);
            return nullptr;
        }
    }

    this->getParameters(callConv);
    LLVMValueRef get = LLVMGetNamedFunction(generator->lModule, linkName.c_str());
    if(get != nullptr) return nullptr;
    if(callConv == -1) callConv = LLVMCCallConv;
    generator->functions[this->name] = LLVMAddFunction(
        generator->lModule, linkName.c_str(),
        LLVMFunctionType(generator->genType(this->type, loc), this->genTypes.data(), this->args.size(), this->isVararg)
    );
    LLVMSetFunctionCallConv(generator->functions[this->name], callConv);

    if(this->isInline) generator->addAttr("alwaysinline", LLVMAttributeFunctionIndex, generator->functions[this->name], this->loc);
    else if(this->isNoOpt) {
        generator->addAttr("noinline", LLVMAttributeFunctionIndex, generator->functions[this->name], this->loc);
        generator->addAttr("optnone", LLVMAttributeFunctionIndex, generator->functions[this->name], this->loc);
    }
    if(this->isTemplatePart || this->isTemplate || this->isCtargsPart || this->isComdat) {
        LLVMComdatRef comdat = LLVMGetOrInsertComdat(generator->lModule, linkName.c_str());
        LLVMSetComdatSelectionKind(comdat, LLVMAnyComdatSelectionKind);
        LLVMSetComdat(generator->functions[this->name], comdat);
        LLVMSetLinkage(generator->functions[this->name], LLVMLinkOnceODRLinkage);
    }

    //std::cout << "File = " << generator->file << ", name = " << name << ", isExtern = " << (isExtern ? "true" : "false") << std::endl;

    if(!this->isExtern) {
        if(this->isCtargsPart || this->isCtargs) generator->currentBuiltinArg = 0;
        LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(generator->context, generator->functions[this->name], "entry");
        this->exitBlock = LLVMAppendBasicBlockInContext(generator->context, generator->functions[this->name], "exit");
        this->builder = LLVMCreateBuilderInContext(generator->context);
        generator->builder = this->builder;
        LLVMPositionBuilderAtEnd(generator->builder, entry);

        std::map<std::string, int> indexes;
        std::map<std::string, NodeVar*> vars;
        for(int i=0; i<this->args.size(); i++) {
            indexes.insert({this->args[i].name, i});
            vars.insert({this->args[i].name, new NodeVar(this->args[i].name, nullptr, false, true, false, {}, this->loc, this->args[i].type)});
        }

        currScope = new Scope(this->name, indexes, vars);
        generator->currBB = entry;
        currScope->fnEnd = this->exitBlock;

        if(!instanceof<TypeVoid>(this->type)) this->block->nodes.insert(this->block->nodes.begin(), new NodeVar("return", new NodeNull(this->type, this->loc), false, false, false, {}, this->loc, this->type, false));
        this->block->generate();

        currScope->fnEnd = this->exitBlock;

        if(!currScope->funcHasRet) LLVMBuildBr(generator->builder, this->exitBlock);

        LLVMMoveBasicBlockAfter(this->exitBlock, LLVMGetLastBasicBlock(generator->functions[this->name]));

        uint32_t bbLength = LLVMCountBasicBlocks(generator->functions[currScope->funcName]);
        LLVMBasicBlockRef* basicBlocks = (LLVMBasicBlockRef*)malloc(sizeof(LLVMBasicBlockRef)*bbLength);
        LLVMGetBasicBlocks(generator->functions[currScope->funcName], basicBlocks);
            for(int i=0; i<bbLength; i++) {
                if(basicBlocks[i] != nullptr && std::string(LLVMGetBasicBlockName(basicBlocks[i])) != "exit" && LLVMGetBasicBlockTerminator(basicBlocks[i]) == nullptr) {
                    LLVMPositionBuilderAtEnd(generator->builder, basicBlocks[i]);
                    LLVMBuildBr(generator->builder, this->exitBlock);
                }
            }
        free(basicBlocks);

        LLVMPositionBuilderAtEnd(generator->builder, this->exitBlock);

        if(!instanceof<TypeVoid>(this->type)) {
            LLVMBuildRet(generator->builder, currScope->get("return", this->loc));
        }
        else {
            LLVMBuildRetVoid(generator->builder);
        }

        generator->builder = nullptr;
        currScope = nullptr;
    }
    if(this->isTemplate) generator->toReplace = std::map<std::string, Type*>(oldReplace);

    if(LLVMVerifyFunction(generator->functions[this->name], LLVMPrintMessageAction)) {
        generator->error("LLVM errors into the function '"+this->name+"'! Content:\n"+LLVMPrintValueToString(generator->functions[this->name]), this->loc);
    }
    return generator->functions[this->name];
}

std::string NodeFunc::generateWithCtargs(std::vector<LLVMTypeRef> args) {
    std::vector<FuncArgSet> newArgs;
    for(int i=0; i<args.size(); i++) newArgs.push_back(FuncArgSet{.name = "_RaveArg"+std::to_string(i), .type = lTypeToType(args[i])});

    auto activeLoops = std::map<int32_t, Loop>(generator->activeLoops);
    auto builder = generator->builder;
    auto currBB = generator->currBB;
    auto _scope = currScope;

    std::vector<DeclarMod> _mods;
    for(int i=0; i<mods.size(); i++) if(mods[i].name != "ctargs") _mods.push_back(mods[i]);

    NodeFunc* _f = new NodeFunc(name+typesToString(newArgs), newArgs, (NodeBlock*)this->block->copy(), false, _mods, this->loc, this->type->copy(), {});
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

LLVMValueRef NodeFunc::generateWithTemplate(std::vector<Type*> types, std::string all) {
    auto activeLoops = std::map<int32_t, Loop>(generator->activeLoops);
    auto builder = generator->builder;
    auto currBB = generator->currBB;
    auto _scope = currScope;

    NodeFunc* _f = new NodeFunc(all, this->args, (NodeBlock*)this->block->copy(), false, this->mods, this->loc, this->type->copy(), this->templateNames);
    _f->isTemplate = true;
    _f->check();
    _f->templateTypes = std::vector<Type*>(types);
    LLVMValueRef v = _f->generate();

    generator->activeLoops = activeLoops;
    generator->builder = builder;
    generator->currBB = currBB;
    currScope = _scope;

    return v;
}

Node* NodeFunc::comptime() {return this;}
Type* NodeFunc::getType() {return this->type;}

Node* NodeFunc::copy() {return new NodeFunc(this->name, this->args, (NodeBlock*)this->block->copy(), this->isExtern, this->mods, this->loc, this->type->copy(), this->templateNames);}

bool NodeFunc::isReleased(int n) {
    for(int i=0; i<this->block->nodes.size(); i++) {
        if(instanceof<NodeUnary>(this->block->nodes[i])) {
            NodeUnary* nunary = (NodeUnary*)this->block->nodes[i];
            if(nunary->type == TokType::Destructor && instanceof<NodeIden>(nunary->base)) {
                if(((NodeIden*)nunary->base)->name == this->origArgs[n].name) return true;
            }
        }
        else if(instanceof<NodeWhile>(this->block->nodes[i])) {
            if(((NodeWhile*)this->block->nodes[i])->isReleased(this->origArgs[n].name)) return true;
        }
        else if(instanceof<NodeFor>(this->block->nodes[i])) {
            if(((NodeFor*)this->block->nodes[i])->isReleased(this->origArgs[n].name)) return true;
        }
    }
    return false;
}