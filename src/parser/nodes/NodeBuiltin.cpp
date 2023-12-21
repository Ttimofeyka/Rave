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
    if(this->name[0] == '@') this->name = this->name.substr(1);
    if(this->name == "trunc" || this->name == "fmodf") return this->args[0]->getType();
    if(this->name == "getCurrArg") return asType(0)->type;
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

std::string NodeBuiltin::asStringIden(int n) {
    if(instanceof<NodeIden>(args[n])) {
        NodeIden* id = (NodeIden*)args[n];
        std::string nam = id->name;
        while(AST::aliasTable.find(nam) != AST::aliasTable.end()) {
            Node* node = AST::aliasTable[nam];
            if(instanceof<NodeIden>(node)) nam = ((NodeIden*)node)->name;
            else if(instanceof<NodeString>(node)) nam = ((NodeString*)node)->value;
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
        else result = LLVMBuildSub(generator->builder, one, LLVMBuildMul(generator->builder, LLVMBuildSDiv(generator->builder, one, two, "NodeBuiltin_fmodf_sdiv"), two, "NodeBuiltin_fmodf_mul"), "NodeBuiltin_fmodf");
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
    if(this->name == "argsLength") return (new NodeInt(AST::funcTable[currScope->funcName]->args.size()))->generate();
    if(this->name == "callWithArgs") {
        std::vector<Node*> nodes;
        for(int i=generator->currentBuiltinArg; i<AST::funcTable[currScope->funcName]->args.size(); i++) {
            nodes.push_back(new NodeIden("_RaveArg"+std::to_string(i), this->loc));
        }
        for(int i=1; i<args.size(); i++) nodes.push_back(args[i]);
        return (new NodeCall(this->loc, args[0], nodes))->generate();
    }
    if(this->name == "tEquals") {
        if(this->asType(0)->type->toString() == this->asType(1)->type->toString()) return LLVMConstInt(LLVMInt1TypeInContext(generator->context), 1, false);
        return LLVMConstInt(LLVMInt1TypeInContext(generator->context), 0, false);
    }
    if(this->name == "tNequals") {
        if(this->asType(0)->type->toString() != this->asType(1)->type->toString()) return LLVMConstInt(LLVMInt1TypeInContext(generator->context), 1, false);
        return LLVMConstInt(LLVMInt1TypeInContext(generator->context), 0, false);
    }
    if(this->name == "tIsArray") {
        Type* ty = this->asType(0)->type;
        while(instanceof<TypeConst>(ty)) ty = ((TypeConst*)ty)->instance;
        return LLVMConstInt(LLVMInt1TypeInContext(generator->context), instanceof<TypeArray>(ty), false);
    }
    if(this->name == "tIsPointer") {
        Type* ty = this->asType(0)->type;
        while(instanceof<TypeConst>(ty)) ty = ((TypeConst*)ty)->instance;
        return LLVMConstInt(LLVMInt1TypeInContext(generator->context), instanceof<TypePointer>(ty), false);
    }
    if(this->name == "getCurrArg") return (new NodeCast(asType(0)->type, new NodeIden("_RaveArg"+std::to_string(generator->currentBuiltinArg), this->loc), this->loc))->generate();
    if(this->name == "getArg") return (new NodeCast(asType(0)->type, new NodeIden("_RaveArg"+asNumber(1).to_string(), this->loc), this->loc))->generate();
    if(this->name == "getArgType") {
        this->type = currScope->getVar("_RaveArg"+asNumber(1).to_string())->type;
        return nullptr;
    }
    if(this->name == "skipArg") {
        generator->currentBuiltinArg += 1;
        return nullptr;
    }
    if(this->name == "getCurrArgType") {
        this->type = currScope->getVar("_RaveArg"+std::to_string(generator->currentBuiltinArg), this->loc)->type;
        return nullptr;
    }
    if(this->name == "compileAndLink") {
        std::string iden = this->asStringIden(0);
        if(iden[0] == '<') AST::addToImport.push_back(exePath+iden.substr(1, iden.size()-1)+".rave");
        else AST::addToImport.push_back(getDirectory3(AST::mainFile)+"/"+iden.substr(1, iden.size()-1)+".rave");
        return nullptr;
    }
    if(this->name == "isStructure") {
        if(instanceof<TypeStruct>(asType(0)->type)) return LLVMConstInt(LLVMInt1TypeInContext(generator->context), 1, false);
        return LLVMConstInt(LLVMInt1TypeInContext(generator->context), 0, false);
    }
    if(this->name == "isNumeric") {
        if(instanceof<TypeBasic>(asType(0)->type)) return LLVMConstInt(LLVMInt1TypeInContext(generator->context), 1, false);
        return LLVMConstInt(LLVMInt1TypeInContext(generator->context), 0, false);
    }
    if(this->name == "detectMemoryLeaks") {
        if(currScope != nullptr) currScope->detectMemoryLeaks = asBool(0)->value;
        return nullptr;
    }
    if(this->name == "echo") {
        std::string buffer = "";
        for(int i=0; i<this->args.size(); i++) buffer += this->asStringIden(i);
        std::cout << buffer << std::endl;
        return nullptr;
    }
    if(this->name == "warning") {
        if(generator->settings.disableWarnings) return nullptr;
        std::string buffer = "";
        for(int i=0; i<this->args.size(); i++) buffer += this->asStringIden(i);
        generator->warning(buffer, this->loc);
        return nullptr;
    }
    if(this->name == "error") {
        std::string buffer = "";
        for(int i=0; i<this->args.size(); i++) buffer += this->asStringIden(i);
        generator->error(buffer, this->loc);
        return nullptr;
    }
    if(this->name == "constArrToVal") {
        Type* inType = asType(0)->type;
        Type* outType = asType(1)->type;
        int inSize = inType->getSize();
        int outSize = outType->getSize();
        if(inSize == 8) {
            std::vector<int8_t> ch;
            for(int i=2; i<this->args.size(); i++) {if(instanceof<NodeInt>(this->args[i])) ch.push_back((int8_t)(((NodeInt*)this->args[i])->value.to_int()));}
            switch(outSize) {
                case 16: return LLVMConstInt(LLVMInt16TypeInContext(generator->context), ((int16_t*)ch.data())[0], false);
                case 32: return LLVMConstInt(LLVMInt32TypeInContext(generator->context), ((int32_t*)ch.data())[0], false);
                case 64: return LLVMConstInt(LLVMInt64TypeInContext(generator->context), ((int64_t*)ch.data())[0], false);
                default: generator->error("constArrToVal assert!", this->loc); return nullptr;
            }
        }
        else if(inSize == 16) {
            std::vector<int16_t> ch;
            for(int i=2; i<this->args.size(); i++) {if(instanceof<NodeInt>(this->args[i])) ch.push_back((int16_t)(((NodeInt*)this->args[i])->value.to_int()));}
            switch(outSize) {
                case 32: return LLVMConstInt(LLVMInt32TypeInContext(generator->context), ((int32_t*)ch.data())[0], false);
                case 64: return LLVMConstInt(LLVMInt64TypeInContext(generator->context), ((int64_t*)ch.data())[0], false);
                default: generator->error("constArrToVal assert!", this->loc); return nullptr;
            }
        }
        else if(inSize == 32) {
            std::vector<int32_t> ch;
            for(int i=2; i<this->args.size(); i++) {if(instanceof<NodeInt>(this->args[i])) ch.push_back(((NodeInt*)this->args[i])->value.to_int());}
            if(outSize == 64) return LLVMConstInt(LLVMInt64TypeInContext(generator->context), ((short*)ch.data())[0], false);
            generator->error("constArrToVal assert!", this->loc);
            return nullptr;
        }
        generator->error("constArrToVal assert!", this->loc);
        return nullptr;
    }
    else if(this->name == "local") {
        if(currScope != nullptr) {
            LLVMValueRef value;
            if(instanceof<NodeIden>(this->args[0])) ((NodeIden*)this->args[0])->isMustBePtr = true;
            value = this->args[0]->generate();
            if(generator->activeLoops.size() > 0) {
                // Inside NodeIf/NodeWhile
                Loop lastLoop = generator->activeLoops[generator->activeLoops.size()-1];
                if(instanceof<NodeWhile>(lastLoop.owner)) {
                    NodeWhile* nwhile = (NodeWhile*)lastLoop.owner;
                    if(instanceof<NodeBlock>(nwhile)) ((NodeBlock*)nwhile->body)->nodes.push_back(new NodeUnary(this->loc, TokType::Destructor, new NodeDone(value)));
                    else nwhile->body = new NodeBlock({nwhile->body, new NodeUnary(this->loc, TokType::Destructor, new NodeDone(value))});
                }
            }
            else {
                // Just inside function
                if(AST::funcTable.find(currScope->funcName) != AST::funcTable.end()) {
                    AST::funcTable[currScope->funcName]->localBuiltinBlock->nodes.push_back(new NodeUnary(this->loc, TokType::Destructor, new NodeDone(value)));
                }
            }
            return value;
        }
    }
    else if(this->name == "return") {
        if(currScope != nullptr) {
            if(currScope->fnEnd != nullptr) {
                if(args.size() == 1) (new NodeBinary(TokType::Equ, new NodeIden("return", this->loc), args[0], this->loc))->generate();
                LLVMBuildBr(generator->builder, currScope->fnEnd);
                currScope->funcHasRet = true;
            }
        }
        return nullptr;
    }
    else if(this->name == "hasMethod") {
        Type* ty = asType(0)->type;
        if(!instanceof<TypeStruct>(ty)) return LLVMConstInt(LLVMInt1TypeInContext(generator->context), 0, false);
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
                if(fnType == nullptr) return LLVMConstInt(LLVMInt1TypeInContext(generator->context), 1, false);
            }
        }
        return LLVMConstInt(LLVMInt1TypeInContext(generator->context), 0, false);
    }
    generator->error("builtin with the name '"+this->name+"' does not exist!", this->loc);
    return nullptr;
}

Node* NodeBuiltin::comptime() {
    this->name = trim(this->name);
    if(this->name[0] == '@') this->name = this->name.substr(1);
    if(this->name == "aliasExists") return new NodeBool(AST::aliasTable.find(this->getAliasName(0)) != AST::aliasTable.end());
    if(this->name == "sizeOf") return new NodeInt((asType(0)->type->getSize()) / 8);
    if(this->name == "tEquals") return new NodeBool(asType(0)->type->toString() == asType(1)->type->toString());
    if(this->name == "tNequals") return new NodeBool(asType(0)->type->toString() != asType(1)->type->toString());
    if(this->name == "isStructure") return new NodeBool(instanceof<TypeStruct>(asType(0)->type));
    if(this->name == "isNumeric") return new NodeBool(instanceof<TypeBasic>(asType(0)->type));
    if(this->name == "argsLength") return new NodeInt(AST::funcTable[currScope->funcName]->args.size());
    if(this->name == "tIsArray") {
        Type* ty = this->asType(0)->type;
        while(instanceof<TypeConst>(ty)) ty = ((TypeConst*)ty)->instance;
        return new NodeBool(instanceof<TypeArray>(ty));
    }
    if(this->name == "tIsPointer") {
        Type* ty = this->asType(0)->type;
        while(instanceof<TypeConst>(ty)) ty = ((TypeConst*)ty)->instance;
        return new NodeBool(instanceof<TypePointer>(ty));
    }
    if(this->name == "detectMemoryLeaks") {
        if(currScope != nullptr) currScope->detectMemoryLeaks = asBool(0)->value;
        return nullptr;
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
    AST::checkError("builtin with name '"+this->name+"' does not exist!", this->loc);
    return nullptr;
}