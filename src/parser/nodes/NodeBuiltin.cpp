/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/


#include "../../include/parser/nodes/NodeBuiltin.hpp"
#include "../../include/utils.hpp"
#include "../../include/parser/ast.hpp"
#include "../../include/parser/nodes/NodeType.hpp"
#include "../../include/parser/nodes/NodeFloat.hpp"
#include "../../include/parser/nodes/NodeInt.hpp"
#include "../../include/parser/nodes/NodeString.hpp"
#include "../../include/parser/nodes/NodeIden.hpp"
#include "../../include/parser/nodes/NodeBool.hpp"
#include "../../include/parser/nodes/NodeBinary.hpp"
#include "../../include/parser/nodes/NodeCast.hpp"
#include "../../include/parser/nodes/NodeNamespace.hpp"
#include "../../include/parser/nodes/NodeFunc.hpp"
#include "../../include/parser/nodes/NodeCall.hpp"
#include "../../include/parser/nodes/NodeVar.hpp"
#include "../../include/parser/nodes/NodeStruct.hpp"
#include "../../include/parser/nodes/NodeIf.hpp"
#include "../../include/parser/nodes/NodeWhile.hpp"
#include "../../include/parser/nodes/NodeUnary.hpp"
#include "../../include/parser/nodes/NodeDone.hpp"
#include "../../include/parser/nodes/NodeRet.hpp"
#include "../../include/parser/nodes/NodeIndex.hpp"
#include "../../include/parser/nodes/NodeArray.hpp"
#include "../../include/llvm.hpp"
#include "../../include/parser/Types.hpp"
#include "../../include/compiler.hpp"

NodeBuiltin::NodeBuiltin(std::string name, std::vector<Node*> args, int loc, NodeBlock* block) {
    this->name = name;
    this->args = std::vector<Node*>(args);
    this->loc = loc;
    this->block = block;
}

NodeBuiltin::NodeBuiltin(std::string name, std::vector<Node*> args, int loc, NodeBlock* block, Type* type, bool isImport, bool isTopLevel, int CTId) {
    this->name = name;
    this->args = std::vector<Node*>(args);
    this->loc = loc;
    this->block = block;
    this->type = type;
    this->isImport = isImport;
    this->isTopLevel = isTopLevel;
    this->CTId = CTId;
}

Node* NodeBuiltin::copy() {
    return new NodeBuiltin(
        this->name, this->args, this->loc,
        (NodeBlock*)this->block->copy(), (this->type == nullptr ? nullptr : this->type->copy()), this->isImport,
        this->isTopLevel, this->CTId
    );
}

Type* NodeBuiltin::getType() {
    if(this->name[0] == '@') this->name = this->name.substr(1);
    if(this->name == "fmodf") return this->args[0]->getType();
    if(this->name == "getCurrArg") return asType(0)->type;
    if(this->name == "isNumeric" || this->name == "isVector" || this->name == "isPointer"
    || this->name == "isArray" || this->name == "aliasExists" || this->name == "tEquals"
    || this->name == "tNequals" || this->name == "isStructure" || this->name == "hasMethod"
    || this->name == "hasDestructor" || this->name == "contains") return new TypeBasic(BasicType::Bool);
    if(this->name == "sizeOf" || this->name == "argsLength" || this->name == "getCurrArgNumber") return new TypeBasic(BasicType::Int);
    if(this->name == "vAdd" || this->name == "vSub" || this->name == "vMul" || this->name == "vDiv"
    || this->name == "vShuffle" || this->name == "vHAdd32x4" || this->name == "vHAdd16x8"
    || this->name == "vSumAll") return args[0]->getType();
    if(this->name == "typeToString") return new TypePointer(new TypeBasic(BasicType::Char));
    if(this->name == "minOf" || this->name == "maxOf") return new TypeBasic(BasicType::Ulong);
    return new TypeVoid();
}

void NodeBuiltin::check() {this->isChecked = true;}

std::string NodeBuiltin::getAliasName(int n) {
    if(instanceof<NodeIden>(this->args[n])) return ((NodeIden*)this->args[n])->name;
    else if(instanceof<NodeString>(this->args[n])) return ((NodeString*)this->args[n])->value;
    return "";
}

NodeType* NodeBuiltin::asType(int n, bool isCompTime) {
    if(instanceof<NodeType>(this->args[n])) return (NodeType*)this->args[n];
    if(instanceof<NodeIden>(this->args[n])) {
        std::string name = ((NodeIden*)this->args[n])->name;
        if(generator->toReplace.find(name) != generator->toReplace.end()) {
            Type* ty = generator->toReplace[name];
            while(generator->toReplace.find(ty->toString()) != generator->toReplace.end()) ty = generator->toReplace[ty->toString()];
            return new NodeType(ty, this->loc);
        }
        if(name == "void") return new NodeType(new TypeVoid(), this->loc);
        if(name == "bool") return new NodeType(new TypeBasic(BasicType::Bool), this->loc);
        if(name == "char") return new NodeType(new TypeBasic(BasicType::Char), this->loc);
        if(name == "uchar") return new NodeType(new TypeBasic(BasicType::Uchar), this->loc);
        if(name == "short") return new NodeType(new TypeBasic(BasicType::Short), this->loc);
        if(name == "ushort") return new NodeType(new TypeBasic(BasicType::Ushort), this->loc);
        if(name == "int") return new NodeType(new TypeBasic(BasicType::Int), this->loc);
        if(name == "uint") return new NodeType(new TypeBasic(BasicType::Uint), this->loc);
        if(name == "long") return new NodeType(new TypeBasic(BasicType::Long), this->loc);
        if(name == "ulong") return new NodeType(new TypeBasic(BasicType::Ulong), this->loc);
        if(name == "cent") return new NodeType(new TypeBasic(BasicType::Cent), this->loc);
        if(name == "ucent") return new NodeType(new TypeBasic(BasicType::Ucent), this->loc);
        if(name == "float") return new NodeType(new TypeBasic(BasicType::Float), this->loc);
        if(name == "double") return new NodeType(new TypeBasic(BasicType::Double), this->loc);
        if(name == "float4") return new NodeType(new TypeVector(new TypeBasic(BasicType::Float), 8), this->loc);
        if(name == "float8") return new NodeType(new TypeVector(new TypeBasic(BasicType::Float), 8), this->loc);
        if(name == "int4") return new NodeType(new TypeVector(new TypeBasic(BasicType::Int), 4), this->loc);
        if(name == "int8") return new NodeType(new TypeVector(new TypeBasic(BasicType::Int), 8), this->loc);
        if(name == "short8") return new NodeType(new TypeVector(new TypeBasic(BasicType::Short), 8), this->loc);
        return new NodeType(new TypeStruct(name), this->loc);
    }
    if(instanceof<NodeBuiltin>(this->args[n])) {
        NodeBuiltin* nb = (NodeBuiltin*)this->args[n];
        Type* ty = nullptr;
        if(!isCompTime) {
            nb->generate();
            ty = nb->type->copy();
        }
        else ty = ((NodeType*)nb->comptime())->type->copy();
        return new NodeType(ty, this->loc);
    }
    return new NodeType(this->args[n]->getType(), this->loc);
}

std::string NodeBuiltin::asStringIden(int n) {
    if(instanceof<NodeIden>(args[n])) {
        NodeIden* id = (NodeIden*)args[n];
        std::string nam = id->name;
        while(AST::aliasTable.find(nam) != AST::aliasTable.end()) {
            Node* node = AST::aliasTable[nam];
            if(instanceof<NodeIden>(node)) nam = ((NodeIden*)node)->name;
            else if(instanceof<NodeString>(node)) nam = ((NodeString*)node)->value;
            else if(instanceof<NodeInt>(node)) return ((NodeInt*)node)->value.to_string();
            else {
                generator->error("NodeBuiltin::asStringIden assert!", this->loc);
                return "";
            }
        }
        return nam;
    }
    else if(instanceof<NodeString>(args[n])) return ((NodeString*)args[n])->value;
    else if(instanceof<NodeBuiltin>(args[n])) {
        Node* nn = ((NodeBuiltin*)args[n])->comptime();
        if(instanceof<NodeString>(nn)) return ((NodeString*)nn)->value;
        if(instanceof<NodeIden>(nn)) return ((NodeIden*)nn)->name;
        generator->error("NodeBuiltin::asStringIden assert!", this->loc);
        return "";
    }
    generator->error("NodeBuiltin::asStringIden assert!", this->loc);
    return "";
}

BigInt NodeBuiltin::asNumber(int n) {
    if(instanceof<NodeIden>(args[n])) {
        NodeIden* id = (NodeIden*)args[n];
        while(AST::aliasTable.find(id->name) != AST::aliasTable.end()) {
            Node* node = AST::aliasTable[id->name];
            if(instanceof<NodeIden>(node)) id = ((NodeIden*)node);
            else if(instanceof<NodeInt>(node)) return ((NodeInt*)node)->value;
            else {
                generator->error("NodeBuiltin::asNumber assert!", this->loc);
                return BigInt(0);
            }
        }
        return BigInt(0);
    }
    else if(instanceof<NodeBuiltin>(args[n])) {
        Node* nn = ((NodeBuiltin*)args[n])->comptime();
        if(instanceof<NodeInt>(nn)) return ((NodeInt*)nn)->value;
        generator->error("NodeBuiltin::asBool assert!", this->loc);
        return BigInt(0);
    }
    else if(instanceof<NodeInt>(args[n])) return ((NodeInt*)args[n])->value;
    generator->error("NodeBuiltin::asNumber assert!", this->loc);
    return BigInt(0);
}

NodeBool* NodeBuiltin::asBool(int n) {
    if(instanceof<NodeIden>(args[n])) {
        NodeIden* id = (NodeIden*)args[n];
        while(AST::aliasTable.find(id->name) != AST::aliasTable.end()) {
            Node* node = AST::aliasTable[id->name];
            if(instanceof<NodeIden>(node)) id = ((NodeIden*)node);
            else if(instanceof<NodeBool>(node)) return (NodeBool*)node;
            else {
                generator->error("NodeBuiltin::asBool assert!", this->loc);
                return new NodeBool(false);
            }
        }
        return new NodeBool(false);
    }
    else if(instanceof<NodeBool>(args[n])) return (NodeBool*)args[n];
    else if(instanceof<NodeBuiltin>(args[n])) {
        Node* nn = ((NodeBuiltin*)args[n])->comptime();
        if(instanceof<NodeBool>(nn)) return (NodeBool*)nn;
        generator->error("NodeBuiltin::asBool assert!", this->loc);
        return new NodeBool(false);
    }
    generator->error("NodeBuiltin::asBool assert!", this->loc);
    return new NodeBool(false);
}

std::string getDirectory3(std::string file) {
    return file.substr(0, file.find_last_of("/\\"));
}

RaveValue NodeBuiltin::generate() {
    this->name = trim(this->name);
    if(this->name[0] == '@') this->name = this->name.substr(1);
    if(this->name == "baseType") {
        Type* ty = this->asType(0)->type;
        if(instanceof<TypeArray>(ty)) this->type = ((TypeArray*)ty)->element;
        else if(instanceof<TypePointer>(ty)) this->type = ((TypePointer*)ty)->instance;
        else this->type = ty;
        return {nullptr, nullptr};
    }
    if(this->name == "typeToString") return (new NodeString(this->asType(0)->type->toString(), false))->generate();
    if(this->name == "fmodf") {
        // one = step, two = 0
        RaveValue one = this->args[0]->generate(), two = this->args[1]->generate();
        if(instanceof<TypePointer>(one.type)) one = LLVM::load(one, "fmodf_load", loc);
        if(instanceof<TypePointer>(two.type)) two = LLVM::load(two, "fmodf_load", loc);

        if(!instanceof<TypeBasic>(one.type)) generator->error("the argument must have a numeric type!", loc);

        TypeBasic* ty = (TypeBasic*)one.type;
        RaveValue result = {nullptr, nullptr};

        if(ty->isFloat()) {
            if(ty->toString() != two.type->toString()) {
                if(ty->type == BasicType::Double) {
                    two.value = LLVMBuildFPCast(generator->builder, two.value, generator->genType(ty, loc), "NodeBuiltin_fmodf_ftod");
                    two.type = ty;
                }
                else {
                    one.value = LLVMBuildFPCast(generator->builder, one.value, generator->genType(two.type, loc), "NodeBuiltin_fmodf_ftod");
                    one.type = two.type;
                }

                ty = (TypeBasic*)one.type;
            }

            result = {LLVMBuildSIToFP(generator->builder,
                LLVMBuildFPToSI(
                    generator->builder,
                    LLVMBuildFDiv(generator->builder, one.value, two.value, "NodeBinary_fmodf_fdiv"),
                    LLVMInt64TypeInContext(generator->context),
                    "NodeBuiltin_fmodf_"
                ),
                generator->genType(ty, loc),
                "NodeBuiltin_fmodf2_"
            ), ty};

            result = {LLVMBuildFMul(generator->builder, LLVMBuildFSub(generator->builder, one.value, result.value, "NodeBuiltin_fmodf"), two.value, "NodeBuiltin_fmodf_fmul"), ty};
        }
        else {
            if(one.type->getSize() > two.type->getSize()) {
                two.value = LLVMBuildIntCast2(generator->builder, two.value, generator->genType(one.type, loc), false, "NodeBuiltin_fmodf_itoi");
                two.type = one.type;
            }
            else {
                one.value = LLVMBuildIntCast2(generator->builder, one.value, generator->genType(two.type, loc), false, "NodeBuiltin_fmodf_itoi");
                one.type = two.type;
            }

            result = {LLVMBuildSRem(generator->builder, one.value, two.value, "NodeBuiltin_fmodf_srem"), ty};
        }

        return result;
    }
    if(this->name == "if") {
        if(this->args.size() < 1) generator->error("at least one argument is required!", this->loc);

        Node* cond = this->args[0];
        if(instanceof<NodeBinary>(cond)) ((NodeBinary*)cond)->isStatic = true;
        AST::condStack[CTId] = cond;
        CTId += 1;

        if(this->isImport) for(int i=0; i<this->block->nodes.size(); i++) {
            if(instanceof<NodeBuiltin>(this->block->nodes[i])) ((NodeBuiltin*)this->block->nodes[i])->isImport = true;
            else if(instanceof<NodeNamespace>(this->block->nodes[i])) ((NodeNamespace*)this->block->nodes[i])->isImported = true;
            else if(instanceof<NodeFunc>(this->block->nodes[i])) ((NodeFunc*)this->block->nodes[i])->isExtern = true;
            else if(instanceof<NodeVar>(this->block->nodes[i])) ((NodeVar*)this->block->nodes[i])->isExtern = true;
            else if(instanceof<NodeStruct>(this->block->nodes[i])) ((NodeStruct*)this->block->nodes[i])->isImported = true;
        }
        NodeIf* _if = new NodeIf(cond, block, nullptr, this->loc, (currScope == nullptr ? "" : currScope->funcName), true);
        _if->comptime();
        CTId -= 1;

        return {};
    }
    if(this->name == "else") {
        if(AST::condStack.find(CTId) != AST::condStack.end()) {
            if(this->isImport) for(int i=0; i<this->block->nodes.size(); i++) {
                if(instanceof<NodeBuiltin>(this->block->nodes[i])) ((NodeBuiltin*)this->block->nodes[i])->isImport = true;
                else if(instanceof<NodeNamespace>(this->block->nodes[i])) ((NodeNamespace*)this->block->nodes[i])->isImported = true;
                else if(instanceof<NodeFunc>(this->block->nodes[i])) ((NodeFunc*)this->block->nodes[i])->isExtern = true;
                else if(instanceof<NodeVar>(this->block->nodes[i])) ((NodeVar*)this->block->nodes[i])->isExtern = true;
                else if(instanceof<NodeStruct>(this->block->nodes[i])) ((NodeStruct*)this->block->nodes[i])->isImported = true;
            }
            NodeIf* _else = new NodeIf(new NodeUnary(loc, TokType::Ne, AST::condStack[CTId]), block, nullptr, loc, (currScope == nullptr ? "" : currScope->funcName), true);
            _else->comptime();
        }
        else generator->error("using '@else' statement without '@if'!", loc);
        return {};
    }
    if(this->name == "aliasExists") {
        if(AST::aliasTable.find(this->getAliasName(0)) != AST::aliasTable.end()) return {LLVM::makeInt(1, 1, false), new TypeBasic(BasicType::Bool)};
        return {LLVM::makeInt(1, 0, false), new TypeBasic(BasicType::Bool)};
    }
    if(this->name == "foreachArgs") {
        for(int i=generator->currentBuiltinArg; i<AST::funcTable[currScope->funcName]->args.size(); i++) {
            this->block->generate();
            generator->currentBuiltinArg += 1;
        }
        return {};
    }
    if(this->name == "sizeOf") return {LLVM::makeInt(32, (asType(0)->type)->getSize() / 8, false), new TypeBasic(BasicType::Bool)};
    if(this->name == "argsLength") return (new NodeInt(AST::funcTable[currScope->funcName]->args.size()))->generate();
    if(this->name == "getCurrArgNumber") return (new NodeInt(generator->currentBuiltinArg))->generate();
    if(this->name == "callWithArgs") {
        std::vector<Node*> nodes;
        for(int i=generator->currentBuiltinArg; i<AST::funcTable[currScope->funcName]->args.size(); i++) {
            nodes.push_back(new NodeIden("_RaveArg" + std::to_string(i), this->loc));
        }
        for(int i=1; i<args.size(); i++) nodes.push_back(args[i]);
        return (new NodeCall(this->loc, args[0], nodes))->generate();
    }
    if(this->name == "callWithBeforeArgs") {
        std::vector<Node*> nodes;
        for(int i=1; i<args.size(); i++) nodes.push_back(args[i]);
        for(int i=generator->currentBuiltinArg; i<AST::funcTable[currScope->funcName]->args.size(); i++) {
            nodes.push_back(new NodeIden("_RaveArg" + std::to_string(i), this->loc));
        }
        return (new NodeCall(this->loc, args[0], nodes))->generate();
    }
    if(this->name == "tEquals") {
        if(this->asType(0)->type->toString() == this->asType(1)->type->toString()) return {LLVM::makeInt(1, 1, false), new TypeBasic(BasicType::Bool)};
        return {LLVM::makeInt(1, 1, false), new TypeBasic(BasicType::Bool)};
    }
    if(this->name == "tNequals") {
        // TODO: delete it (deprecated)
        if(this->asType(0)->type->toString() != this->asType(1)->type->toString()) return {LLVM::makeInt(1, 1, false), new TypeBasic(BasicType::Bool)};
        return {LLVM::makeInt(1, 0, false), new TypeBasic(BasicType::Bool)};
    }
    if(this->name == "isArray") {
        Type* ty = this->asType(0)->type;
        while(instanceof<TypeConst>(ty)) ty = ((TypeConst*)ty)->instance;
        return {LLVM::makeInt(1, instanceof<TypeArray>(ty), false), new TypeBasic(BasicType::Bool)};
    }
    if(this->name == "isVector") {
        Type* ty = this->asType(0)->type;
        while(instanceof<TypeConst>(ty)) ty = ((TypeConst*)ty)->instance;
        return {LLVM::makeInt(1, instanceof<TypeVector>(ty), false), new TypeBasic(BasicType::Bool)};
    }
    if(this->name == "isPointer") {
        Type* ty = this->asType(0)->type;
        while(instanceof<TypeConst>(ty)) ty = ((TypeConst*)ty)->instance;
        return {LLVM::makeInt(1, instanceof<TypePointer>(ty), false), new TypeBasic(BasicType::Bool)};
    }
    if(this->name == "getCurrArg") return (new NodeCast(asType(0)->type, new NodeIden("_RaveArg" + std::to_string(generator->currentBuiltinArg), this->loc), this->loc))->generate();
    if(this->name == "getArg") return (new NodeCast(asType(0)->type, new NodeIden("_RaveArg" + asNumber(1).to_string(), this->loc), this->loc))->generate();
    if(this->name == "getArgType") {
        this->type = currScope->getVar("_RaveArg"+asNumber(1).to_string())->type;
        return {};
    }
    if(this->name == "skipArg") {
        generator->currentBuiltinArg += 1;
        return {};
    }
    if(this->name == "getCurrArgType") {
        this->type = currScope->getVar("_RaveArg" + std::to_string(generator->currentBuiltinArg), this->loc)->type;
        return {};
    }
    if(this->name == "compileAndLink") {
        std::string iden = this->asStringIden(0);
        if(iden[0] == '<') AST::addToImport.push_back(exePath + iden.substr(1, iden.size()-1) + ".rave");
        else AST::addToImport.push_back(getDirectory3(AST::mainFile) + "/" + iden.substr(1, iden.size()-1) + ".rave");
        return {};
    }
    if(this->name == "isStructure") {
        if(instanceof<TypeStruct>(asType(0)->type)) return {LLVM::makeInt(1, 1, false), new TypeBasic(BasicType::Bool)};
        return {LLVM::makeInt(1, 0, false), new TypeBasic(BasicType::Bool)};
    }
    if(this->name == "isNumeric") {
        if(instanceof<TypeBasic>(asType(0)->type)) return {LLVM::makeInt(1, 1, false), new TypeBasic(BasicType::Bool)};
        return {LLVM::makeInt(1, 0, false), new TypeBasic(BasicType::Bool)};
    }
    if(this->name == "echo") {
        std::string buffer = "";
        for(int i=0; i<this->args.size(); i++) buffer += this->asStringIden(i);
        std::cout << buffer << std::endl;
        return {};
    }
    if(this->name == "warning") {
        if(generator->settings.disableWarnings) return {};

        std::string buffer = "";
        for(int i=0; i<this->args.size(); i++) buffer += this->asStringIden(i);
        generator->warning(buffer, this->loc);
        return {};
    }
    if(this->name == "error") {
        std::string buffer = "";
        for(int i=0; i<this->args.size(); i++) buffer += this->asStringIden(i);
        generator->error(buffer, this->loc);
        return {};
    }
    if(this->name == "addLibrary") {
        Compiler::linkString += " -l"+asStringIden(0)+" ";
        return {};
    }
    else if(this->name == "return") {
        if(currScope != nullptr) {
            if(currScope->fnEnd != nullptr) {
                if(args.size() == 1) (new NodeBinary(TokType::Equ, new NodeIden("return", this->loc), args[0], this->loc))->generate();
                if(currScope->fnEnd != nullptr) LLVMBuildBr(generator->builder, currScope->fnEnd);
                else LLVMBuildBr(generator->builder, AST::funcTable[currScope->funcName]->exitBlock);
                currScope->funcHasRet = true;
            }
        }
        return {};
    }
    else if(this->name == "hasMethod") {
        Type* ty = asType(0)->type;
        if(!instanceof<TypeStruct>(ty)) return {LLVM::makeInt(1, 0, false), new TypeBasic(BasicType::Bool)};
        TypeStruct* tstruct = (TypeStruct*)ty;
        
        std::string methodName = asStringIden(1);
        TypeFunc* fnType = nullptr;

        if(args.size() == 3) {
            if(instanceof<NodeCall>(args[2])) fnType = callToTFunc(((NodeCall*)args[2]));
            else if(instanceof<NodeType>(args[2])) fnType = (TypeFunc*)((NodeType*)args[2])->type;
            else {
                generator->error("undefined type of method '" + methodName + "'!", this->loc);
                return {};
            }
        }

        for(auto &&x : AST::methodTable) {
            if(x.first.first == tstruct->name && x.second->origName == methodName) {
                if(fnType == nullptr) return {LLVM::makeInt(1, 1, false), new TypeBasic(BasicType::Bool)};
            }
        }

        return {LLVM::makeInt(1, 0, false), new TypeBasic(BasicType::Bool)};
    }
    else if(this->name == "hasDestructor") {
        Type* ty = asType(0)->type;
        if(!instanceof<TypeStruct>(ty)) return {LLVM::makeInt(1, 0, false), new TypeBasic(BasicType::Bool)};
        TypeStruct* tstruct = (TypeStruct*)ty;

        if(AST::structTable.find(tstruct->name) == AST::structTable.end()) return {LLVM::makeInt(1, 0, false), new TypeBasic(BasicType::Bool)};
        if(AST::structTable[tstruct->name]->destructor == nullptr) return {LLVM::makeInt(1, 0, false), new TypeBasic(BasicType::Bool)};
        return {LLVM::makeInt(1, 1, false), new TypeBasic(BasicType::Bool)};
    }
    else if(this->name == "atomicTAS") {
        LLVMValueRef result = LLVMBuildAtomicRMW(generator->builder, LLVMAtomicRMWBinOpXchg, this->args[0]->generate().value,
            this->args[1]->generate().value, LLVMAtomicOrderingSequentiallyConsistent, false
        );
        LLVMSetVolatile(result, true);
        return {result, new TypeBasic(BasicType::Int)};
    }
    else if(this->name == "atomicClear") {
        RaveValue ptr = this->args[0]->generate();
        LLVMValueRef store = LLVMBuildStore(generator->builder, LLVMConstNull(generator->genType(ptr.type->getElType(), loc)), ptr.value);
        LLVMSetOrdering(store, LLVMAtomicOrderingSequentiallyConsistent);
        LLVMSetVolatile(store, true);
        return {store, new TypeVoid()};
    }
    else if(this->name == "getLinkName") {
        if(instanceof<NodeIden>(this->args[0])) {
            NodeIden* niden = (NodeIden*)this->args[0];

            if(currScope->has(niden->name)) return (new NodeString(currScope->getVar(niden->name, this->loc)->linkName, false))->generate();
            if(AST::funcTable.find(niden->name) != AST::funcTable.end()) return (new NodeString(AST::funcTable[niden->name]->linkName, false))->generate();
        }
        generator->error("cannot get the link name of this value!", this->loc);
        return {};
    }
    else if(this->name == "vLoad") {
        RaveValue value = {nullptr, nullptr};
        Type* resultVectorType;
        if(this->args.size() < 2) generator->error("at least two arguments are required!", this->loc);

        resultVectorType = asType(0)->type;
        if(!instanceof<TypeVector>(resultVectorType)) generator->error("the type must be a vector!", this->loc);

        value = this->args[1]->generate();
        if(!instanceof<TypePointer>(value.type)) generator->error("the second argument is not a pointer to the data!", this->loc);

        bool alignment = true;
        if(this->args.size() == 3) alignment = asBool(2)->value;

        std::string sName = "__rave_vLoad_" + resultVectorType->toString();
        if(AST::structTable.find(sName) == AST::structTable.end()) {
            AST::structTable[sName] = new NodeStruct(sName, {new NodeVar("v1", nullptr, false, false, false, {}, loc, resultVectorType)}, loc, "", {}, {});
            
            if(generator->structures.find(sName) == generator->structures.end()) AST::structTable[sName]->generate();
        }

        // Uses clang method: creating anonymous structure with vector type and bitcast it
        RaveValue v = LLVM::load(LLVM::structGep({LLVMBuildBitCast(
            generator->builder, value.value,
            LLVMPointerType(LLVMStructTypeInContext(generator->context, std::vector<LLVMTypeRef>({generator->genType(resultVectorType, this->loc)}).data(), 1, false), 0),
            "vLoad_bitc"
        ), new TypePointer(new TypeStruct(sName))}, 0, "vLoad_sgep"), "vLoad", loc);

        if(!alignment) LLVMSetAlignment(v.value, 1);
        return v;
    }
    else if(this->name == "vStore") {
        if(this->args.size() < 2) generator->error("at least two arguments are required!", this->loc);

        RaveValue vector = this->args[0]->generate();
        if(!instanceof<TypeVector>(vector.type)) generator->error("the first argument must have a vector type!", this->loc);
 
        RaveValue dataPtr = this->args[1]->generate();
        if(!instanceof<TypePointer>(dataPtr.type)) generator->error("the second argument is not a pointer to the data!", this->loc);

        bool alignment = true;
        if(this->args.size() == 3) alignment = asBool(2)->value;

        std::string sName = "__rave_vStore_" + vector.type->toString();
        if(AST::structTable.find(sName) == AST::structTable.end()) {
            AST::structTable[sName] = new NodeStruct(sName, {new NodeVar("v1", nullptr, false, false, false, {}, loc, vector.type)}, loc, "", {}, {});
            
            if(generator->structures.find(sName) == generator->structures.end()) AST::structTable[sName]->generate();
        }

        // Uses clang method: creating anonymous structure with vector type and bitcast it
        LLVMValueRef vPtr = LLVMBuildStore(generator->builder, vector.value, LLVM::structGep({LLVMBuildBitCast(
            generator->builder, dataPtr.value,
            LLVMPointerType(LLVMStructTypeInContext(generator->context, std::vector<LLVMTypeRef>({LLVMTypeOf(vector.value)}).data(), 1, false), 0),
            "vStore_bitc"
        ), new TypePointer(new TypeStruct(sName))}, 0, "vStore_sgep").value);

        if(!alignment) LLVMSetAlignment(vPtr, 1);
        return {};
    }
    else if(this->name == "vFrom") {
        RaveValue value = {nullptr, nullptr};
        Type* resultVectorType;
        if(this->args.size() < 2) generator->error("at least two arguments are required!", this->loc);

        resultVectorType = asType(0)->type;
        if(!instanceof<TypeVector>(resultVectorType)) generator->error("the type must be a vector!", this->loc);

        value = this->args[1]->generate();

        RaveValue vector = LLVM::alloc(resultVectorType, "vFrom_buffer");
        RaveValue tempVector = LLVM::load(vector, "vFrom_loadedBuffer", loc);

        for(int i=0; i<((TypeVector*)resultVectorType)->count; i++)
            tempVector.value = LLVMBuildInsertElement(generator->builder, tempVector.value, value.value, LLVMConstInt(LLVMInt32TypeInContext(generator->context), i, false), "vFrom_ie");

        return tempVector;
    }
    else if(this->name == "vGet") {
        if(this->args.size() < 2) generator->error("at least two arguments are required!", this->loc);
        RaveValue vector = this->args[0]->generate();

        if(instanceof<TypePointer>(vector.type)) vector = LLVM::load(vector, "vGet_load", loc);
        if(!instanceof<TypeVector>(vector.type)) generator->error("the value must be a vector!", this->loc);

        return {LLVMBuildExtractElement(generator->builder, vector.value, this->args[1]->generate().value, "vGet_ee"), vector.type->getElType()};
    }
    else if(this->name == "vSet") {
        if(this->args.size() < 3) generator->error("at least three arguments are required!", this->loc);
        RaveValue vector = this->args[0]->generate();
        if(!instanceof<TypePointer>(vector.type)) generator->error("the value must be a pointer to the vector!", this->loc);

        RaveValue index = this->args[1]->generate();
        RaveValue value = this->args[2]->generate();
        RaveValue buffer = LLVM::load(vector, "vSet_buffer", loc);

        buffer.value = LLVMBuildInsertElement(generator->builder, buffer.value, value.value, index.value, "vSet_ie");
        LLVMBuildStore(generator->builder, buffer.value, vector.value);

        return {};
    }
    else if(this->name == "vShuffle") {
        if(this->args.size() < 3) generator->error("at least three arguments are required!", this->loc);

        RaveValue vector1 = this->args[0]->generate();
        RaveValue vector2 = this->args[1]->generate();

        if(instanceof<TypePointer>(vector1.type)) vector1 = LLVM::load(vector1, "vShuffle_load1_", loc);
        if(instanceof<TypePointer>(vector2.type)) vector2 = LLVM::load(vector2, "vShuffle_load2_", loc);

        if(!instanceof<TypeVector>(vector1.type) || !instanceof<TypeVector>(vector2.type)) generator->error("the values must have the vector type!", this->loc);

        if(!instanceof<NodeArray>(this->args[2])) generator->error("The third argument must be a constant array!", this->loc);
        if(!instanceof<TypeArray>(this->args[2]->getType())) generator->error("the third argument must be a constant array!", this->loc);

        TypeArray* tarray = (TypeArray*)this->args[2]->getType();
        if(!instanceof<TypeBasic>(tarray->element) || ((TypeBasic*)tarray->element)->type != BasicType::Int)
            generator->error("the third argument must be a constant array of integers!", this->loc);
        if(tarray->count < 1) generator->error("the constant array cannot be empty!", this->loc);

        std::vector<LLVMValueRef> values;
        for(int i=0; i<((NodeArray*)this->args[2])->values.size(); i++) values.push_back(((NodeArray*)this->args[2])->values[i]->generate().value);

        return {LLVMBuildShuffleVector(generator->builder, vector1.value, vector2.value, LLVMConstVector(values.data(), values.size()), "vShuffle"), vector1.type};
    }
    else if(this->name == "vHAdd32x4") {
        if(Compiler::features.find("+sse3") == std::string::npos || Compiler::features.find("+ssse3") == std::string::npos) {
            generator->error("your target does not supports SSE3/SSSE3!", this->loc);
            return {};
        }

        if(this->args.size() < 2) generator->error("at least two arguments are required!", this->loc);

        RaveValue vector1 = this->args[0]->generate();
        RaveValue vector2 = this->args[1]->generate();

        if(instanceof<TypePointer>(vector1.type)) vector1 = LLVM::load(vector1, "VHAdd32x4_load1_", loc);
        if(instanceof<TypePointer>(vector2.type)) vector2 = LLVM::load(vector2, "VHAdd32x4_load2_", loc);

        if(!instanceof<TypeVector>(vector1.type) || !instanceof<TypeVector>(vector2.type)) generator->error("the values must have the vector type!", this->loc);
        if(vector1.type->getElType()->toString() != vector2.type->getElType()->toString()) generator->error("the values must have the same type!", this->loc);

        if(!((TypeBasic*)vector1.type->getElType())->isFloat()) return LLVM::call(generator->functions["llvm.x86.ssse3.phadd.d.128"], std::vector<LLVMValueRef>({vector1.value, vector2.value}).data(), 2, "vHAdd32x4");
        return LLVM::call(generator->functions["llvm.x86.sse3.hadd.ps"], std::vector<LLVMValueRef>({vector1.value, vector2.value}).data(), 2, "vHAdd32x4");
    }
    else if(this->name == "vHAdd16x8") {
        if(Compiler::features.find("+ssse3") == std::string::npos) {
            generator->error("your target does not supports SSSE3!", this->loc);
            return {};
        }

        if(this->args.size() < 2) generator->error("at least two arguments are required!", this->loc);

        RaveValue vector1 = this->args[0]->generate();
        RaveValue vector2 = this->args[1]->generate();

        if(instanceof<TypePointer>(vector1.type)) vector1 = LLVM::load(vector1, "VHAdd16x8_load1_", loc);
        if(instanceof<TypePointer>(vector2.type)) vector2 = LLVM::load(vector2, "VHAdd16x8_load2_", loc);

        if(!instanceof<TypeVector>(vector1.type) || !instanceof<TypeVector>(vector2.type)) generator->error("the values must have the vector type!", this->loc);
        if(vector1.type->getElType()->toString() != vector2.type->getElType()->toString()) generator->error("the values must have the same type!", this->loc);

        return LLVM::call(generator->functions["llvm.x86.ssse3.phadd.sw.128"], std::vector<LLVMValueRef>({vector1.value, vector2.value}).data(), 2, "vHAdd16x8");
    }
    else if(this->name == "vSumAll") {
        if(Compiler::features.find("+sse3") == std::string::npos || Compiler::features.find("+ssse3") == std::string::npos) {
            generator->error("your target does not supports SSE3/SSSE3!", this->loc);
            return {};
        }

        if(this->args.size() < 1) generator->error("at least one argument is required!", this->loc);

        RaveValue value = this->args[0]->generate();
        if(!instanceof<TypeVector>(value.type)) generator->error("the value must have the vector type!", this->loc);

        unsigned int vectorSize = LLVMGetVectorSize(LLVMTypeOf(value.value));
        unsigned long long abiSize = LLVMABISizeOfType(generator->targetData, LLVMTypeOf(value.value));

        if(vectorSize == 4 && abiSize == 16) {
            // 32 x 4
            RaveValue result = (new NodeBuiltin("vHAdd32x4", {new NodeDone(value), new NodeDone(value)}, this->loc, this->block))->generate();
            return (new NodeBuiltin("vHAdd32x4", {new NodeDone(result), new NodeDone(result)}, this->loc, this->block))->generate();
        }
        else if(vectorSize == 8 && abiSize == 16) {
            // 16 x 8
            RaveValue result = (new NodeBuiltin("vHAdd16x8", {new NodeDone(value), new NodeDone(value)}, this->loc, this->block))->generate();
            result = (new NodeBuiltin("vHAdd16x8", {new NodeDone(result), new NodeDone(result)}, this->loc, this->block))->generate();
            return (new NodeBuiltin("vHAdd16x8", {new NodeDone(result), new NodeDone(result)}, this->loc, this->block))->generate();
        }

        generator->error("unsupported vector!", this->loc);
        return {};
    }
    else if(this->name == "alloca") {
        if(this->args.size() < 1) generator->error("at least one argument is required!", this->loc);
        RaveValue size = this->args[0]->generate();

        return (
            new NodeCast(
                new TypePointer(new TypeBasic(BasicType::Char)),
                new NodeDone(LLVM::alloc(size, "NodeBuiltin_alloca")),
                this->loc
            )
        )->generate();
    }
    else if(this->name == "minOf") {
        if(this->args.size() < 1) generator->error("at least one argument is required!", this->loc);
        Type* type = asType(0)->type;

        if(!instanceof<TypeVoid>(type) && !instanceof<TypeBasic>(type)) generator->error("the type must be a basic!", this->loc);
        if(instanceof<TypeVoid>(type)) return {LLVM::makeInt(64, 0, false), new TypeBasic(BasicType::Long)};
        else {
            TypeBasic* btype = (TypeBasic*)type;
            switch(btype->type) {
                case BasicType::Bool: return {LLVM::makeInt(64, 0, false), new TypeBasic(BasicType::Long)};
                case BasicType::Char: return {LLVM::makeInt(64, -128, false), new TypeBasic(BasicType::Long)};
                case BasicType::Short: return {LLVM::makeInt(64, -32768, false), new TypeBasic(BasicType::Long)};
                case BasicType::Int: return {LLVM::makeInt(64, -2147483647, false), new TypeBasic(BasicType::Long)};
                case BasicType::Long: return {LLVM::makeInt(64, -9223372036854775807, false), new TypeBasic(BasicType::Long)};
                // TODO: Add BasicType::Cent
                default: return {LLVM::makeInt(64, 0, false), new TypeBasic(BasicType::Long)};
            }
        }
    }
    else if(this->name == "maxOf") {
        if(this->args.size() < 1) generator->error("at least one argument is required!", this->loc);
        Type* type = asType(0)->type;

        if(!instanceof<TypeVoid>(type) && !instanceof<TypeBasic>(type)) generator->error("the type must be a basic!", this->loc);
        if(instanceof<TypeVoid>(type)) return {LLVM::makeInt(64, 0, false), new TypeBasic(BasicType::Long)};
        else {
            TypeBasic* btype = (TypeBasic*)type;
            switch(btype->type) {
                case BasicType::Bool: return {LLVM::makeInt(64, 1, false), new TypeBasic(BasicType::Long)};
                case BasicType::Char: return {LLVM::makeInt(64, 127, false), new TypeBasic(BasicType::Long)};
                case BasicType::Uchar: return {LLVM::makeInt(64, 255, false), new TypeBasic(BasicType::Long)};
                case BasicType::Short: return {LLVM::makeInt(64, 32767, false), new TypeBasic(BasicType::Long)};
                case BasicType::Ushort: return {LLVM::makeInt(64, 65535, false), new TypeBasic(BasicType::Long)};
                case BasicType::Int: return {LLVM::makeInt(64, 2147483647, false), new TypeBasic(BasicType::Long)};
                case BasicType::Uint: return {LLVM::makeInt(64, 4294967295, false), new TypeBasic(BasicType::Long)};
                case BasicType::Long: return {LLVM::makeInt(64, 9223372036854775807, false), new TypeBasic(BasicType::Long)};
                case BasicType::Ulong: return {LLVM::makeInt(64, 18446744073709551615ull, false), new TypeBasic(BasicType::Ulong)};
                // TODO: Add BasicType::Cent and BasicType::Ucent
                default: return {LLVM::makeInt(64, 0, false), new TypeBasic(BasicType::Long)};
            }
        }
    }
    generator->error("builtin with the name '" + this->name + "' does not exist!", this->loc);
    return {};
}

Node* NodeBuiltin::comptime() {
    this->name = trim(this->name);
    if(this->name[0] == '@') this->name = this->name.substr(1);
    if(this->name == "aliasExists") return new NodeBool(AST::aliasTable.find(this->getAliasName(0)) != AST::aliasTable.end());
    if(this->name == "getLinkName") {
        if(instanceof<NodeIden>(this->args[0])) {
            NodeIden* niden = (NodeIden*)this->args[0];

            if(currScope->has(niden->name)) return new NodeString(currScope->getVar(niden->name, this->loc)->linkName, false);
            if(AST::funcTable.find(niden->name) != AST::funcTable.end()) return new NodeString(AST::funcTable[niden->name]->linkName, false);
        }
        generator->error("cannot get the link name of this value!", this->loc);
        return nullptr;
    }
    if(this->name == "sizeOf") return new NodeInt((asType(0)->type->getSize()) / 8);
    if(this->name == "tEquals") return new NodeBool(asType(0)->type->toString() == asType(1)->type->toString());
    if(this->name == "tNequals") return new NodeBool(asType(0)->type->toString() != asType(1)->type->toString());
    if(this->name == "isStructure") return new NodeBool(instanceof<TypeStruct>(asType(0)->type));
    if(this->name == "isNumeric") return new NodeBool(instanceof<TypeBasic>(asType(0)->type));
    if(this->name == "argsLength") return new NodeInt(AST::funcTable[currScope->funcName]->args.size());
    if(this->name == "typeToString") return new NodeString(this->asType(0)->type->toString(), false);
    if(this->name == "baseType") {
        Type* ty = this->asType(0)->type;
        if(instanceof<TypeArray>(ty)) this->type = ((TypeArray*)ty)->element;
        else if(instanceof<TypePointer>(ty)) this->type = ((TypePointer*)ty)->instance;
        else if(instanceof<TypeVector>(ty)) this->type = ((TypeVector*)ty)->mainType;
        else this->type = ty;
        return new NodeType(ty, this->loc);
    }
    if(this->name == "isArray") {
        Type* ty = this->asType(0)->type;
        while(instanceof<TypeConst>(ty)) ty = ((TypeConst*)ty)->instance;
        return new NodeBool(instanceof<TypeArray>(ty));
    }
    if(this->name == "isVector") {
        Type* ty = this->asType(0)->type;
        while(instanceof<TypeConst>(ty)) ty = ((TypeConst*)ty)->instance;
        return new NodeBool(instanceof<TypeVector>(ty));
    }
    if(this->name == "isPointer") {
        Type* ty = this->asType(0)->type;
        while(instanceof<TypeConst>(ty)) ty = ((TypeConst*)ty)->instance;
        return new NodeBool(instanceof<TypePointer>(ty));
    }
    if(this->name == "contains") return new NodeBool(asStringIden(0).find(asStringIden(1)) != std::string::npos);
    if(this->name == "hasMethod") {
        Type* ty = asType(0)->type;
        if(!instanceof<TypeStruct>(ty)) return new NodeBool(false);
        TypeStruct* tstruct = (TypeStruct*)ty;
        
        std::string methodName = asStringIden(1);
        TypeFunc* fnType = nullptr;

        if(args.size() == 3) {
            if(instanceof<NodeCall>(args[2])) fnType = callToTFunc(((NodeCall*)args[2]));
            else if(instanceof<NodeType>(args[2])) fnType = (TypeFunc*)((NodeType*)args[2])->type;
            else {
                generator->error("undefined type of method '"+methodName+"'!", this->loc);
                return nullptr;
            }
        }

        for(auto &&x : AST::methodTable) {
            if(x.first.first == tstruct->name && x.second->origName == methodName) {
                if(fnType == nullptr) return new NodeBool(true);
            }
        }
        return new NodeBool(false);
    }
    if(this->name == "hasDestructor") {
        Type* ty = asType(0)->type;
        if(!instanceof<TypeStruct>(ty)) return new NodeBool(false);
        TypeStruct* tstruct = (TypeStruct*)ty;

        if(AST::structTable.find(tstruct->name) == AST::structTable.end()) return new NodeBool(false);
        return new NodeBool(AST::structTable[tstruct->name]->destructor == nullptr);
    }
    else if(this->name == "minOf") {
        if(this->args.size() < 1) generator->error("at least one argument is required!", this->loc);
        Type* type = asType(0)->type;

        if(!instanceof<TypeVoid>(type) && !instanceof<TypeBasic>(type)) generator->error("the type must be a basic!", this->loc);
        if(instanceof<TypeVoid>(type)) return new NodeInt(0);
        else {
            TypeBasic* btype = (TypeBasic*)type;
            switch(btype->type) {
                case BasicType::Bool: return new NodeInt(0);
                case BasicType::Char: return new NodeInt(-128);
                case BasicType::Short: return new NodeInt(-32768);
                case BasicType::Int: return new NodeInt(-2147483647);
                case BasicType::Long: return new NodeInt(BigInt("-9223372036854775807"));
                case BasicType::Cent: return new NodeInt(BigInt("170141183460469231731687303715884105728"));
                default: return new NodeInt(0);
            }
        }
    }
    else if(this->name == "maxOf") {
        if(this->args.size() < 1) generator->error("at least one argument is required!", this->loc);
        Type* type = asType(0)->type;

        if(!instanceof<TypeVoid>(type) && !instanceof<TypeBasic>(type)) generator->error("the type must be a basic!", this->loc);
        if(instanceof<TypeVoid>(type)) return new NodeInt(0);
        else {
            TypeBasic* btype = (TypeBasic*)type;
            switch(btype->type) {
                case BasicType::Bool: return new NodeInt(1);
                case BasicType::Char: return new NodeInt(127);
                case BasicType::Uchar: return new NodeInt(255);
                case BasicType::Short: return new NodeInt(32767);
                case BasicType::Ushort: return new NodeInt(65535);
                case BasicType::Int: return new NodeInt(2147483647);
                case BasicType::Uint: return new NodeInt(4294967295);
                case BasicType::Long: return new NodeInt(9223372036854775807);
                case BasicType::Ulong: return new NodeInt(BigInt("18446744073709551615"));
                case BasicType::Cent: return new NodeInt(BigInt("170141183460469231731687303715884105727"));
                case BasicType::Ucent: return new NodeInt(BigInt("340282366920938463463374607431768211455"));
                default: return new NodeInt(0);
            }
        }
    }
    AST::checkError("builtin with name '" + this->name + "' does not exist!", this->loc);
    return nullptr;
}