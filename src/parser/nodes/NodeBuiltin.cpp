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
#include "../../include/parser/nodes/NodeNamespace.hpp"
#include "../../include/parser/nodes/NodeFunc.hpp"
#include "../../include/parser/nodes/NodeCall.hpp"
#include "../../include/parser/nodes/NodeVar.hpp"
#include "../../include/parser/nodes/NodeStruct.hpp"
#include "../../include/parser/nodes/NodeIf.hpp"
#include "../../include/parser/Types.hpp"

NodeBuiltin::NodeBuiltin(std::string name, std::vector<Node*> args, long loc, NodeBlock* block) {
    this->name = name;
    this->args = std::vector<Node*>(args);
    this->loc = loc;
    this->block = block;
}
NodeBuiltin::NodeBuiltin(std::string name, std::vector<Node*> args, long loc, NodeBlock* block, Type* type, bool isImport, bool isTopLevel, int CTId) {
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
    if(this->name == "trunc" || this->name == "fmodf") return this->args[0]->getType();
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

LLVMValueRef NodeBuiltin::generate() {
    this->name = trim(this->name);
    if(this->name[0] == '@') this->name = this->name.substr(1);
    if(this->name == "baseType") {
        Type* ty = this->asType(0)->type;
        if(instanceof<TypeArray>(ty)) this->type = ((TypeArray*)ty)->element;
        else if(instanceof<TypePointer>(ty)) this->type = ((TypePointer*)ty)->instance;
        else this->type = ty;
        return nullptr;
    }
    if(this->name == "trunc") {
        LLVMValueRef value = this->args[0]->generate();
        if(LLVMGetTypeKind(LLVMTypeOf(value)) != LLVMFloatTypeKind && LLVMGetTypeKind(LLVMTypeOf(value)) != LLVMDoubleTypeKind) return value;
        return LLVMBuildSIToFP(generator->builder,
            LLVMBuildFPToSI(generator->builder, value, LLVMInt64TypeInContext(generator->context), "NodeBuiltin_trunc_1"), LLVMTypeOf(value), "NodeBuiltin_trunc_2"
        );
    }
    if(this->name == "fmodf") {
        LLVMValueRef one = this->args[0]->generate(), two = this->args[1]->generate();
        LLVMTypeRef ty = LLVMTypeOf(one);
        LLVMValueRef result = nullptr;
        if(LLVMGetTypeKind(ty) == LLVMFloatTypeKind || LLVMGetTypeKind(ty) == LLVMDoubleTypeKind) {
            if(LLVMGetTypeKind(ty) != LLVMGetTypeKind(LLVMTypeOf(two))) {
                if(LLVMGetTypeKind(ty) == LLVMDoubleTypeKind) two = LLVMBuildFPCast(generator->builder, two, ty, "NodeBuiltin_fmodf_ftod");
                else one = LLVMBuildFPCast(generator->builder, one, LLVMTypeOf(two), "NodeBuiltin_fmodf_ftod");
            }
            result = LLVMBuildSIToFP(generator->builder,
                LLVMBuildFPToSI(
                    generator->builder,
                    LLVMBuildFDiv(generator->builder, one, two, "NodeBinary_fmodf_fdiv"),
                    LLVMInt64TypeInContext(generator->context),
                    "NodeBuiltin_fmodf_trunc1_"
                ),
                LLVMTypeOf(one),
                "fmodf_trunc2_"
            );
            result = LLVMBuildFMul(generator->builder, LLVMBuildFSub(generator->builder, one, result, "NodeBuiltin_fmodf"), two, "NodeBuiltin_fmodf_fmul");
        }
        else {
            result = LLVMBuildSub(generator->builder, one, LLVMBuildMul(generator->builder, LLVMBuildSDiv(generator->builder, one, two, "NodeBuiltin_fmodf_sdiv"), two, "NodeBuiltin_fmodf_mul"), "NodeBuiltin_fmodf");
        }
        return result;
    }
    if(this->name == "if") {
        Node* cond = this->args[0];
        if(instanceof<NodeBinary>(cond)) ((NodeBinary*)cond)->isStatic = true;
        AST::condStack[CTId] = cond;
        if(this->isImport) for(int i=0; i<this->block->nodes.size(); i++) {
            if(instanceof<NodeBuiltin>(this->block->nodes[i])) ((NodeBuiltin*)this->block->nodes[i])->isImport = true;
            else if(instanceof<NodeNamespace>(this->block->nodes[i])) ((NodeNamespace*)this->block->nodes[i])->isImported = true;
            else if(instanceof<NodeFunc>(this->block->nodes[i])) ((NodeFunc*)this->block->nodes[i])->isExtern = true;
            else if(instanceof<NodeVar>(this->block->nodes[i])) ((NodeVar*)this->block->nodes[i])->isExtern = true;
            else if(instanceof<NodeStruct>(this->block->nodes[i])) ((NodeStruct*)this->block->nodes[i])->isImported = true;
        }
        NodeIf* _if = new NodeIf(cond, block, nullptr, this->loc, (currScope == nullptr ? "" : currScope->funcName), true);
        _if->comptime();
        return nullptr;
    }
    if(this->name == "aliasExists") {
        if(AST::aliasTable.find(this->getAliasName(0)) != AST::aliasTable.end()) return LLVMConstInt(LLVMInt1TypeInContext(generator->context), 1, false);
        return LLVMConstInt(LLVMInt1TypeInContext(generator->context), 0, false);
    }
    if(this->name == "foreachArgs") {
        for(int i=generator->currentBuiltinArg; i<AST::funcTable[currScope->funcName]->args.size(); i++) {
            this->block->generate();
            generator->currentBuiltinArg += 1;
        }
        return nullptr;
    }
    if(this->name == "sizeOf") return LLVMConstInt(LLVMInt32TypeInContext(generator->context), (asType(0)->type)->getSize() / 8, false);
    if(this->name == "callWithArgs") {
        std::vector<Node*> nodes;
        for(int i=generator->currentBuiltinArg; i<AST::funcTable[currScope->funcName]->args.size(); i++) {
            nodes.push_back(new NodeIden("_RaveArg"+std::to_string(i), this->loc));
        }
        for(int i=1; i<args.size(); i++) nodes.push_back(args[i]);
        return (new NodeCall(this->loc, args[0], nodes))->generate();
    }
    generator->error("builtin with the name '"+this->name+"' does not exist!", this->loc);
    return nullptr;
}

Node* NodeBuiltin::comptime() {
    this->name = trim(this->name);
    if(this->name[0] == '@') this->name = this->name.substr(1);
    if(this->name == "aliasExists") return new NodeBool(AST::aliasTable.find(this->getAliasName(0)) != AST::aliasTable.end());
    if(this->name == "sizeOf") return new NodeInt((asType(0)->type->getSize()) / 8);
    AST::checkError("builtin with name '"+this->name+"' does not exist!", this->loc);
    return nullptr;
}