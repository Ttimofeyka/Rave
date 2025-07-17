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

NodeBuiltin::~NodeBuiltin() {
    for(size_t i=0; i<args.size(); i++) if(args[i] != nullptr) delete args[i];
    if(block != nullptr) delete block;
    if(type != nullptr) delete type;
}

Node* NodeBuiltin::copy() {
    return new NodeBuiltin(
        name, this->args, this->loc,
        (NodeBlock*)this->block->copy(), (this->type == nullptr ? nullptr : this->type->copy()), this->isImport,
        this->isTopLevel, this->CTId
    );
}

Type* NodeBuiltin::getType() {
    if(name[0] == '@') name = name.substr(1);
    if(name == "fmodf") return asType(0)->type;
    if(name == "getCurrArg") return asType(0)->type;
    if(name == "isNumeric" || name == "isVector" || name == "isPointer"
    || name == "isArray" || name == "aliasExists" || name == "isStructure"
    || name == "isFloat" || name == "hasMethod" || name == "hasDestructor"
    || name == "contains" || name == "isUnsigned") return basicTypes[BasicType::Bool];
    if(name == "sizeOf" || name == "argsLength" || name == "getCurrArgNumber"
    || name == "vMoveMask128" || name == "vMoveMask256" || name == "cttz" || name == "ctlz") return basicTypes[BasicType::Int];
    if(name == "vShuffle" || name == "vHAdd32x4" || name == "vHAdd16x8"
    || name == "vSqrt" || name == "vAbs" || name == "vLoad") return asType(0)->type;
    if(name == "typeToString") return new TypePointer(basicTypes[BasicType::Char]);
    if(name == "minOf" || name == "maxOf") return basicTypes[BasicType::Ulong];
    return typeVoid;
}

void NodeBuiltin::check() {isChecked = true;}

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

        return new NodeType(getTypeByName(name), loc);
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
    name = trim(name);
    if(name[0] == '@') name = name.substr(1);

    if(name == "baseType") {
        if(this->args.size() < 1) generator->error("at least one argument is required!", this->loc);

        Type* ty = this->asType(0)->type;
        if(instanceof<TypeArray>(ty)) this->type = ((TypeArray*)ty)->element;
        else if(instanceof<TypePointer>(ty)) this->type = ((TypePointer*)ty)->instance;
        else this->type = ty;
        return {nullptr, nullptr};
    }
    else if(name == "typeToString") return (new NodeString(this->asType(0)->type->toString(), false))->generate();
    else if(name == "fmodf") {
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
    else if(name == "aliasExists") {
        if(this->args.size() < 1) generator->error("at least one argument is required!", this->loc);

        if(AST::aliasTable.find(this->getAliasName(0)) != AST::aliasTable.end()) return {LLVM::makeInt(1, 1, false), basicTypes[BasicType::Bool]};
        return {LLVM::makeInt(1, 0, false), basicTypes[BasicType::Bool]};
    }
    else if(name == "foreachArgs") {
        for(int i=generator->currentBuiltinArg; i<AST::funcTable[currScope->funcName]->args.size(); i++) {
            this->block->generate();
            generator->currentBuiltinArg += 1;
        }
        return {};
    }
    else if(name == "sizeOf") {
        if(this->args.size() < 1) generator->error("at least one argument is required!", this->loc);

        return {LLVM::makeInt(32, (asType(0)->type)->getSize() / 8, false), basicTypes[BasicType::Bool]};
    }
    else if(name == "argsLength") return (new NodeInt(AST::funcTable[currScope->funcName]->args.size()))->generate();
    else if(name == "getCurrArgNumber") return (new NodeInt(generator->currentBuiltinArg))->generate();
    else if(name == "callWithArgs") {
        std::vector<Node*> nodes;
        for(int i=generator->currentBuiltinArg; i<AST::funcTable[currScope->funcName]->args.size(); i++) {
            nodes.push_back(new NodeIden("_RaveArg" + std::to_string(i), this->loc));
        }
        for(int i=1; i<args.size(); i++) nodes.push_back(args[i]);
        return (new NodeCall(this->loc, args[0], nodes))->generate();
    }
    else if(name == "callWithBeforeArgs") {
        std::vector<Node*> nodes;
        for(int i=1; i<args.size(); i++) nodes.push_back(args[i]);
        for(int i=generator->currentBuiltinArg; i<AST::funcTable[currScope->funcName]->args.size(); i++) {
            nodes.push_back(new NodeIden("_RaveArg" + std::to_string(i), this->loc));
        }
        return (new NodeCall(this->loc, args[0], nodes))->generate();
    }
    else if(name == "tEquals") {
        if(this->args.size() < 2) generator->error("at least two arguments are required!", this->loc);

        if(this->asType(0)->type->toString() == this->asType(1)->type->toString()) return {LLVM::makeInt(1, 1, false), basicTypes[BasicType::Bool]};
        return {LLVM::makeInt(1, 1, false), basicTypes[BasicType::Bool]};
    }
    else if(name == "isArray") {
        if(this->args.size() < 1) generator->error("at least one argument is required!", this->loc);

        Type* ty = this->asType(0)->type;
        while(instanceof<TypeConst>(ty)) ty = ty->getElType();
        return {LLVM::makeInt(1, instanceof<TypeArray>(ty), false), basicTypes[BasicType::Bool]};
    }
    else if(name == "isVector") {
        if(this->args.size() < 1) generator->error("at least one argument is required!", this->loc);

        Type* ty = this->asType(0)->type;
        while(instanceof<TypeConst>(ty)) ty = ty->getElType();
        return {LLVM::makeInt(1, instanceof<TypeVector>(ty), false), basicTypes[BasicType::Bool]};
    }
    else if(name == "isPointer") {
        if(this->args.size() < 1) generator->error("at least one argument is required!", this->loc);

        Type* ty = this->asType(0)->type;
        while(instanceof<TypeConst>(ty)) ty = ty->getElType();
        return {LLVM::makeInt(1, instanceof<TypePointer>(ty), false), basicTypes[BasicType::Bool]};
    }
    else if(name == "getCurrArg") {
        if(this->args.size() < 1) generator->error("at least one argument is required!", this->loc);

        return (new NodeCast(asType(0)->type, new NodeIden("_RaveArg" + std::to_string(generator->currentBuiltinArg), this->loc), this->loc))->generate();
    }
    else if(name == "getArg") {
        if(this->args.size() < 1) generator->error("at least one argument is required!", this->loc);

        return (new NodeCast(asType(0)->type, new NodeIden("_RaveArg" + asNumber(1).to_string(), this->loc), this->loc))->generate();
    }
    else if(name == "getArgType") {
        if(this->args.size() < 1) generator->error("at least one argument is required!", this->loc);

        this->type = currScope->getVar("_RaveArg" + asNumber(1).to_string())->type;
        return {};
    }
    else if(name == "skipArg") {
        generator->currentBuiltinArg += 1;
        return {};
    }
    else if(name == "getCurrArgType") {
        this->type = currScope->getVar("_RaveArg" + std::to_string(generator->currentBuiltinArg), this->loc)->type;
        return {};
    }
    else if(name == "compileAndLink") {
        if(this->args.size() < 1) generator->error("at least one argument is required!", this->loc);

        std::string iden = this->asStringIden(0);
        if(iden[0] == '<') AST::addToImport.push_back(exePath + iden.substr(1, iden.size()-1) + ".rave");
        else AST::addToImport.push_back(getDirectory3(AST::mainFile) + "/" + iden.substr(1, iden.size()-1) + ".rave");
        return {};
    }
    else if(name == "isStructure") {
        if(this->args.size() < 1) generator->error("at least one argument is required!", this->loc);

        if(instanceof<TypeStruct>(asType(0)->type)) return {LLVM::makeInt(1, 1, false), basicTypes[BasicType::Bool]};
        return {LLVM::makeInt(1, 0, false), basicTypes[BasicType::Bool]};
    }
    else if(name == "isNumeric") {
        if(this->args.size() < 1) generator->error("at least one argument is required!", this->loc);

        if(instanceof<TypeBasic>(asType(0)->type)) return {LLVM::makeInt(1, 1, false), basicTypes[BasicType::Bool]};
        return {LLVM::makeInt(1, 0, false), basicTypes[BasicType::Bool]};
    }
    else if(name == "isFloat") {
        if(args.size() < 1) generator->error("at least one argument is required!", loc);

        if(isFloatType(asType(0)->type)) return {LLVM::makeInt(1, 1, false), basicTypes[BasicType::Bool]};
        return {LLVM::makeInt(1, 0, false), basicTypes[BasicType::Bool]};
    }
    else if(name == "isUnsigned") {
        if(this->args.size() < 1) generator->error("at least one argument is required!", this->loc);

        if(instanceof<TypeBasic>(asType(0)->type)) return {LLVM::makeInt(1, ((TypeBasic*)asType(0)->type)->isUnsigned(), false), basicTypes[BasicType::Bool]};
        return {LLVM::makeInt(1, 0, false), basicTypes[BasicType::Bool]};
    }
    else if(name == "echo") {
        std::string buffer = "";
        for(size_t i=0; i<this->args.size(); i++) buffer += this->asStringIden(i);
        std::cout << buffer << std::endl;
        return {};
    }
    else if(name == "warning") {
        if(generator->settings.disableWarnings) return {};

        std::string buffer = "";
        for(size_t i=0; i<this->args.size(); i++) buffer += this->asStringIden(i);
        generator->warning(buffer, this->loc);
        return {};
    }
    else if(name == "error") {
        std::string buffer = "";
        for(size_t i=0; i<this->args.size(); i++) buffer += this->asStringIden(i);
        generator->error(buffer, this->loc);
        return {};
    }
    else if(name == "addLibrary") {
        if(this->args.size() < 1) generator->error("at least one argument is required!", this->loc);

        Compiler::linkString += " -l" + asStringIden(0) + " ";
        return {};
    }
    else if(name == "return") {
        if(currScope != nullptr) {
            if(currScope->fnEnd != nullptr) {
                if(args.size() == 1) Binary::operation(TokType::Equ, new NodeIden("return", loc), args[0], loc);
                if(currScope->fnEnd != nullptr) LLVMBuildBr(generator->builder, currScope->fnEnd);
                else LLVMBuildBr(generator->builder, AST::funcTable[currScope->funcName]->exitBlock);
                if(generator->activeLoops.size() > 0) generator->activeLoops[generator->activeLoops.size()-1].hasEnd = true;
                currScope->funcHasRet = true;
            }
        }
        return {};
    }
    else if(name == "hasMethod") {
        if(this->args.size() < 1) generator->error("at least one argument is required!", this->loc);

        Type* ty = asType(0)->type;
        if(generator->toReplace.find(ty->toString()) != generator->toReplace.end()) ty = generator->toReplace[ty->toString()];

        if(!instanceof<TypeStruct>(ty)) return {LLVM::makeInt(1, 0, false), basicTypes[BasicType::Bool]};
        TypeStruct* tstruct = (TypeStruct*)ty;
        
        std::string methodName = asStringIden(1);
        TypeFunc* fnType = nullptr;

        if(args.size() == 3) {
            if(instanceof<NodeCall>(args[2])) fnType = callToTFunc(((NodeCall*)args[2]));
            else if(instanceof<NodeType>(args[2])) fnType = (TypeFunc*)((NodeType*)args[2])->type;
            else {
                generator->error("undefined type of method \033[1m" + methodName + "\033[22m!", this->loc);
                return {};
            }
        }

        auto it = AST::methodTable.find({tstruct->name, methodName});
        if(it != AST::methodTable.end() && fnType == nullptr) return {LLVM::makeInt(1, 1, false), basicTypes[BasicType::Bool]};

        return {LLVM::makeInt(1, 0, false), basicTypes[BasicType::Bool]};
    }
    else if(name == "hasDestructor") {
        if(this->args.size() < 1) generator->error("at least one argument is required!", this->loc);

        auto it = AST::structTable.find(asType(0)->type->toString());
        return {LLVM::makeInt(1, it != AST::structTable.end() && it->second->destructor != nullptr, false), basicTypes[BasicType::Bool]};
    }
    else if(name == "atomicTAS") {
        if(this->args.size() < 1) generator->error("at least one argument is required!", this->loc);

        LLVMValueRef result = LLVMBuildAtomicRMW(generator->builder, LLVMAtomicRMWBinOpXchg, this->args[0]->generate().value,
            this->args[1]->generate().value, LLVMAtomicOrderingSequentiallyConsistent, false
        );

        LLVMSetVolatile(result, true);
        return {result, basicTypes[BasicType::Int]};
    }
    else if(name == "atomicClear") {
        if(this->args.size() < 1) generator->error("at least one argument is required!", this->loc);

        RaveValue ptr = this->args[0]->generate();
        LLVMValueRef store = LLVMBuildStore(generator->builder, LLVMConstNull(generator->genType(ptr.type->getElType(), loc)), ptr.value);
        LLVMSetOrdering(store, LLVMAtomicOrderingSequentiallyConsistent);
        LLVMSetVolatile(store, true);
        return {store, typeVoid};
    }
    else if(name == "getLinkName") {
        if(instanceof<NodeIden>(this->args[0])) {
            NodeIden* niden = (NodeIden*)this->args[0];

            if(currScope->has(niden->name)) return (new NodeString(currScope->getVar(niden->name, this->loc)->linkName, false))->generate();
            if(AST::funcTable.find(niden->name) != AST::funcTable.end()) return (new NodeString(AST::funcTable[niden->name]->linkName, false))->generate();
        }
        generator->error("cannot get the link name of this value!", this->loc);
        return {};
    }
    else if(name == "vLoad") {
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
        else LLVMSetAlignment(v.value, resultVectorType->getElType()->getSize() / 2);
        return v;
    }
    else if(name == "vStore") {
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
        else LLVMSetAlignment(vPtr, vector.type->getElType()->getSize() / 2);
        return {};
    }
    else if(name == "vFrom") {
        RaveValue value = {nullptr, nullptr};
        Type* resultVectorType;
        if(this->args.size() < 2) generator->error("at least two arguments are required!", this->loc);

        resultVectorType = asType(0)->type;
        if(!instanceof<TypeVector>(resultVectorType)) generator->error("the type must be a vector!", this->loc);

        value = this->args[1]->generate();

        RaveValue vector = LLVM::alloc(resultVectorType, "vFrom_buffer");
        RaveValue tempVector = LLVM::load(vector, "vFrom_loadedBuffer", loc);

        for(size_t i=0; i<((TypeVector*)resultVectorType)->count; i++)
            tempVector.value = LLVMBuildInsertElement(generator->builder, tempVector.value, value.value, LLVMConstInt(LLVMInt32TypeInContext(generator->context), i, false), "vFrom_ie");

        return tempVector;
    }
    else if(name == "vShuffle") {
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
        if(((NodeInt*)tarray->count->comptime())->value.to_int() < 1) generator->error("the constant array cannot be empty!", this->loc);

        std::vector<LLVMValueRef> values;
        for(size_t i=0; i<((NodeArray*)this->args[2])->values.size(); i++) values.push_back(((NodeArray*)this->args[2])->values[i]->generate().value);

        return {LLVMBuildShuffleVector(generator->builder, vector1.value, vector2.value, LLVMConstVector(values.data(), values.size()), "vShuffle"), vector1.type};
    }
    else if(name == "vHAdd32x4") {
        if(Compiler::features.find("+sse3") == std::string::npos || Compiler::features.find("+ssse3") == std::string::npos)
            generator->error("your target does not support SSE3/SSSE3!", this->loc);


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
    else if(name == "vHAdd16x8") {
        if(Compiler::features.find("+ssse3") == std::string::npos) {
            generator->error("your target does not support SSSE3!", this->loc);
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
    else if(name == "vMoveMask128") {
        if(Compiler::features.find("+sse2") == std::string::npos)  generator->error("your target does not support SSE2!", this->loc);

        if(this->args.size() < 1) generator->error("at least one argument is required!", this->loc);

        RaveValue value = this->args[0]->generate();

        if(instanceof<TypePointer>(value.type)) value = LLVM::load(value, "NodeBuiltin_vMoveMask_load_", this->loc);

        if(generator->functions.find("llvm.x86.sse2.pmovmskb.128") == generator->functions.end()) {
            generator->functions["llvm.x86.sse2.pmovmskb.128"] = {LLVMAddFunction(generator->lModule, "llvm.x86.sse2.pmovmskb.128", LLVMFunctionType(
                LLVMInt32TypeInContext(generator->context),
                std::vector<LLVMTypeRef>({LLVMVectorType(LLVMInt8TypeInContext(generator->context), 16)}).data(),
                1, false
            )), new TypeFunc(basicTypes[BasicType::Int], {new TypeFuncArg(new TypeVector(basicTypes[BasicType::Char], 16), "v")}, false)};
        }

        return LLVM::call(generator->functions["llvm.x86.sse2.pmovmskb.128"], std::vector<LLVMValueRef>({value.value}).data(), 1, "vMoveMask128");
    }
    else if(name == "vMoveMask256") {
        if(Compiler::features.find("+avx2") == std::string::npos)  generator->error("your target does not support AVX2!", this->loc);

        if(this->args.size() < 1) generator->error("at least one argument is required!", this->loc);

        RaveValue value = this->args[0]->generate();

        if(instanceof<TypePointer>(value.type)) value = LLVM::load(value, "NodeBuiltin_vMoveMask_load_", this->loc);

        if(generator->functions.find("llvm.x86.avx2.pmovmskb") == generator->functions.end()) {
            generator->functions["llvm.x86.avx2.pmovmskb"] = {LLVMAddFunction(generator->lModule, "llvm.x86.avx2.pmovmskb", LLVMFunctionType(
                LLVMInt32TypeInContext(generator->context),
                std::vector<LLVMTypeRef>({LLVMVectorType(LLVMInt8TypeInContext(generator->context), 32)}).data(),
                1, false
            )), new TypeFunc(basicTypes[BasicType::Int], {new TypeFuncArg(new TypeVector(basicTypes[BasicType::Char], 32), "v")}, false)};
        }

        return LLVM::call(generator->functions["llvm.x86.avx2.pmovmskb"], std::vector<LLVMValueRef>({value.value}).data(), 1, "vMoveMask256");
    }
    else if(name == "vSqrt") {
        if(this->args.size() < 1) generator->error("at least one argument is required!", this->loc);

        RaveValue value = this->args[0]->generate();

        if(instanceof<TypePointer>(value.type)) value = LLVM::load(value, "NodeBuiltin_vSqrt_load_", this->loc);
        if(!instanceof<TypeVector>(value.type)) generator->error("the value must have the vector type!", this->loc);

        TypeBasic* vectorType = (TypeBasic*)value.type->getElType();

        if(vectorType->type == BasicType::Float) {
            if(((TypeVector*)value.type)->count == 4) {
                if(Compiler::features.find("+sse") == std::string::npos)  generator->error("your target does not support SSE!", this->loc);
            }
            else if(Compiler::features.find("+avx") == std::string::npos)  generator->error("your target does not support AVX!", this->loc);
        }
        else if(vectorType->type == BasicType::Double) {
            if(((TypeVector*)value.type)->count == 2) {
                if(Compiler::features.find("+sse2") == std::string::npos)  generator->error("your target does not support SSE2!", this->loc);
            }
            else if(Compiler::features.find("+avx") == std::string::npos)  generator->error("your target does not support AVX!", this->loc);
        }
        else generator->error("unsupported type!", this->loc);

        std::string vecFuncName = "llvm.sqrt.v" + std::to_string(((TypeVector*)value.type)->count) + (vectorType->type == BasicType::Float ? "f32" : "f64");

        if(generator->functions.find(vecFuncName) == generator->functions.end()) {
            generator->functions[vecFuncName] = {LLVMAddFunction(generator->lModule, vecFuncName.c_str(), LLVMFunctionType(
                generator->genType(value.type, loc),
                std::vector<LLVMTypeRef>({generator->genType(value.type, loc)}).data(),
                1, false
            )), new TypeFunc(value.type, {new TypeFuncArg(value.type, "v")}, false)};
        }

        return LLVM::call(generator->functions[vecFuncName], std::vector<LLVMValueRef>({value.value}).data(), 1, "vSqrt");
    }
    else if(name == "vAbs") {
        if(this->args.size() < 1) generator->error("at least one argument is required!", this->loc);

        RaveValue value = this->args[0]->generate();

        if(instanceof<TypePointer>(value.type)) value = LLVM::load(value, "NodeBuiltin_vAbs_load_", this->loc);
        if(!instanceof<TypeVector>(value.type)) generator->error("the value must have the vector type!", this->loc);

        TypeBasic* vectorType = (TypeBasic*)value.type->getElType();

        if(vectorType->type == BasicType::Short) {
            if(((TypeVector*)value.type)->count == 8) {
                if(Compiler::features.find("+ssse3") == std::string::npos)  generator->error("your target does not support SSSE3!", this->loc);
            }
            else generator->error("unsupported type!", this->loc);
        }
        else if(vectorType->type == BasicType::Int) {
            if(((TypeVector*)value.type)->count == 4) {
                if(Compiler::features.find("+ssse3") == std::string::npos)  generator->error("your target does not support SSSE3!", this->loc);
            }
            else if(Compiler::features.find("+avx2") == std::string::npos)  generator->error("your target does not support AVX2!", this->loc);
        }
        else generator->error("unsupported type!", this->loc);

        std::string vecFuncName = "llvm.abs.v" + std::to_string(((TypeVector*)value.type)->count) + (vectorType->type == BasicType::Short ? "i16" : "i32");

        if(generator->functions.find(vecFuncName) == generator->functions.end()) {
            generator->functions[vecFuncName] = {LLVMAddFunction(generator->lModule, vecFuncName.c_str(), LLVMFunctionType(
                generator->genType(value.type, loc),
                std::vector<LLVMTypeRef>({generator->genType(value.type, loc)}).data(),
                1, false
            )), new TypeFunc(value.type, {new TypeFuncArg(value.type, "v"), new TypeFuncArg(basicTypes[BasicType::Bool], "poison")}, false)};
        }

        return LLVM::call(generator->functions[vecFuncName], std::vector<LLVMValueRef>({value.value, LLVM::makeInt(1, 0, false)}).data(), 2, "vSqrt");
    }
    else if(name == "cttz") {
        if(this->args.size() < 2) generator->error("at least two arguments are required!", this->loc);

        RaveValue value = this->args[0]->generate();
        RaveValue isZeroPoison = this->args[1]->generate();

        // TODO: Add check for types

        LLVMTypeRef valueType = LLVMTypeOf(value.value);
        std::string funcName = std::string("llvm.cttz.") + LLVMPrintTypeToString(valueType);

        if(generator->functions.find(funcName) == generator->functions.end()) {
            generator->functions[funcName] = {LLVMAddFunction(generator->lModule, funcName.c_str(), LLVMFunctionType(
                valueType,
                std::vector<LLVMTypeRef>({valueType, LLVMInt1TypeInContext(generator->context)}).data(),
                2, false
            )), new TypeFunc(value.type, {new TypeFuncArg(value.type, "value"), new TypeFuncArg(new TypeBasic(BasicType::Bool), "isZeroPoison")}, false)};
        }

        return LLVM::call(generator->functions[funcName], std::vector<LLVMValueRef>({value.value, isZeroPoison.value}).data(), 2, "cttz");
    }
    else if(name == "ctlz") {
        if(this->args.size() < 2) generator->error("at least two arguments are required!", this->loc);

        RaveValue value = this->args[0]->generate();
        RaveValue isZeroPoison = this->args[1]->generate();

        // TODO: Add check for types

        LLVMTypeRef valueType = LLVMTypeOf(value.value);
        std::string funcName = std::string("llvm.ctlz.") + LLVMPrintTypeToString(valueType);

        if(generator->functions.find(funcName) == generator->functions.end()) {
            generator->functions[funcName] = {LLVMAddFunction(generator->lModule, funcName.c_str(), LLVMFunctionType(
                valueType,
                std::vector<LLVMTypeRef>({valueType, LLVMInt1TypeInContext(generator->context)}).data(),
                2, false
            )), new TypeFunc(value.type, {new TypeFuncArg(value.type, "value"), new TypeFuncArg(new TypeBasic(BasicType::Bool), "isZeroPoison")}, false)};
        }

        return LLVM::call(generator->functions[funcName], std::vector<LLVMValueRef>({value.value, isZeroPoison.value}).data(), 2, "ctlz");
    }
    else if(name == "alloca") {
        if(this->args.size() < 1) generator->error("at least one argument is required!", this->loc);
        RaveValue size = this->args[0]->generate();

        return (
            new NodeCast(
                new TypePointer(basicTypes[BasicType::Char]),
                new NodeDone(LLVM::alloc(size, "NodeBuiltin_alloca")),
                this->loc
            )
        )->generate();
    }
    else if(name == "minOf") {
        if(this->args.size() < 1) generator->error("at least one argument is required!", this->loc);
        Type* type = asType(0)->type;

        if(!instanceof<TypeVoid>(type) && !instanceof<TypeBasic>(type)) generator->error("the type must be a basic!", this->loc);
        if(instanceof<TypeVoid>(type)) return {LLVM::makeInt(64, 0, false), basicTypes[BasicType::Long]};
        else {
            TypeBasic* btype = (TypeBasic*)type;
            switch(btype->type) {
                case BasicType::Bool: case BasicType::Uchar: case BasicType::Ushort:
                case BasicType::Uint: case BasicType::Ulong: case BasicType::Ucent: return {LLVM::makeInt(64, 0, false), basicTypes[BasicType::Long]};
                case BasicType::Char: return {LLVM::makeInt(64, -128, false), basicTypes[BasicType::Long]};
                case BasicType::Short: return {LLVM::makeInt(64, -32768, false), basicTypes[BasicType::Long]};
                case BasicType::Int: return {LLVM::makeInt(64, -2147483647, false), basicTypes[BasicType::Long]};
                case BasicType::Long: return {LLVM::makeInt(64, -9223372036854775807, false), basicTypes[BasicType::Long]};
                case BasicType::Cent: return {LLVMConstIntOfString(LLVMInt128TypeInContext(generator->context), "-170141183460469231731687303715884105728", 10), basicTypes[BasicType::Cent]};;
                default: return {LLVM::makeInt(64, 0, false), basicTypes[BasicType::Long]};
            }
        }
    }
    else if(name == "maxOf") {
        if(this->args.size() < 1) generator->error("at least one argument is required!", this->loc);
        Type* type = asType(0)->type;

        if(!instanceof<TypeVoid>(type) && !instanceof<TypeBasic>(type)) generator->error("the type must be a basic!", this->loc);
        if(instanceof<TypeVoid>(type)) return {LLVM::makeInt(64, 0, false), basicTypes[BasicType::Long]};
        else {
            TypeBasic* btype = (TypeBasic*)type;
            switch(btype->type) {
                case BasicType::Bool: return {LLVM::makeInt(64, 1, false), basicTypes[BasicType::Long]};
                case BasicType::Char: return {LLVM::makeInt(64, 127, false), basicTypes[BasicType::Long]};
                case BasicType::Uchar: return {LLVM::makeInt(64, 255, false), basicTypes[BasicType::Long]};
                case BasicType::Short: return {LLVM::makeInt(64, 32767, false), basicTypes[BasicType::Long]};
                case BasicType::Ushort: return {LLVM::makeInt(64, 65535, false), basicTypes[BasicType::Long]};
                case BasicType::Int: return {LLVM::makeInt(64, 2147483647, false), basicTypes[BasicType::Long]};
                case BasicType::Uint: return {LLVM::makeInt(64, 4294967295, false), basicTypes[BasicType::Long]};
                case BasicType::Long: return {LLVM::makeInt(64, 9223372036854775807, false), basicTypes[BasicType::Long]};
                case BasicType::Ulong: return {LLVM::makeInt(64, 18446744073709551615ull, false), basicTypes[BasicType::Ulong]};
                case BasicType::Cent: return {LLVMConstIntOfString(LLVMInt128TypeInContext(generator->context), "170141183460469231731687303715884105727", 10), basicTypes[BasicType::Cent]};
                case BasicType::Ucent: return {LLVMConstIntOfString(LLVMInt128TypeInContext(generator->context), "340282366920938463463374607431768211455", 10), basicTypes[BasicType::Ucent]};
                default: return {LLVM::makeInt(64, 0, false), basicTypes[BasicType::Long]};
            }
        }
    }

    generator->error("builtin with the name \033[1m" + name + "\033[22m does not exist!", this->loc);
    return {};
}

Node* NodeBuiltin::comptime() {
    name = trim(name);
    if(name[0] == '@') name = name.substr(1);

    if(name == "aliasExists") return new NodeBool(AST::aliasTable.find(this->getAliasName(0)) != AST::aliasTable.end());
    else if(name == "getLinkName") {
        if(instanceof<NodeIden>(this->args[0])) {
            NodeIden* niden = (NodeIden*)this->args[0];

            if(currScope->has(niden->name)) return new NodeString(currScope->getVar(niden->name, this->loc)->linkName, false);
            if(AST::funcTable.find(niden->name) != AST::funcTable.end()) return new NodeString(AST::funcTable[niden->name]->linkName, false);
        }
        generator->error("cannot get the link name of this value!", this->loc);
        return nullptr;
    }
    else if(name == "sizeOf") return new NodeInt((asType(0)->type->getSize()) / 8);
    else if(name == "tEquals") return new NodeBool(asType(0)->type->toString() == asType(1)->type->toString());
    else if(name == "isStructure") return new NodeBool(instanceof<TypeStruct>(asType(0)->type));
    else if(name == "isNumeric") return new NodeBool(instanceof<TypeBasic>(asType(0)->type));
    else if(name == "isFloat") return new NodeBool(isFloatType(asType(0)->type));
    else if(name == "isUnsigned") {
        Type* _type = asType(0)->type;
        if(!instanceof<TypeBasic>(_type)) return new NodeBool(false);
        return new NodeBool(((TypeBasic*)_type)->isUnsigned());
    }
    else if(name == "argsLength") return new NodeInt(AST::funcTable[currScope->funcName]->args.size());
    else if(name == "typeToString") return new NodeString(this->asType(0)->type->toString(), false);
    else if(name == "baseType") {
        Type* ty = this->asType(0)->type;
        if(instanceof<TypeArray>(ty)) this->type = ((TypeArray*)ty)->element;
        else if(instanceof<TypePointer>(ty)) this->type = ((TypePointer*)ty)->instance;
        else if(instanceof<TypeVector>(ty)) this->type = ((TypeVector*)ty)->mainType;
        else this->type = ty;
        return new NodeType(ty, this->loc);
    }
    else if(name == "isArray") {
        Type* ty = this->asType(0)->type;
        while(instanceof<TypeConst>(ty)) ty = ty->getElType();
        return new NodeBool(instanceof<TypeArray>(ty));
    }
    else if(name == "isVector") {
        Type* ty = this->asType(0)->type;
        while(instanceof<TypeConst>(ty)) ty = ty->getElType();
        return new NodeBool(instanceof<TypeVector>(ty));
    }
    else if(name == "isPointer") {
        Type* ty = this->asType(0)->type;
        while(instanceof<TypeConst>(ty)) ty = ty->getElType();
        return new NodeBool(instanceof<TypePointer>(ty));
    }
    else if(name == "getArgType") {
        this->type = currScope->getVar("_RaveArg" + asNumber(1).to_string())->type;
        return new NodeType(this->type, loc);
    }
    else if(name == "getCurrArgType") {
        this->type = currScope->getVar("_RaveArg" + std::to_string(generator->currentBuiltinArg), this->loc)->type;
        return new NodeType(this->type, loc);
    }
    else if(name == "contains") return new NodeBool(asStringIden(0).find(asStringIden(1)) != std::string::npos);
    else if(name == "hasMethod") {
        Type* ty = asType(0)->type;
        if(generator->toReplace.find(ty->toString()) != generator->toReplace.end()) ty = generator->toReplace[ty->toString()];

        if(!instanceof<TypeStruct>(ty)) return new NodeBool(false);
        TypeStruct* tstruct = (TypeStruct*)ty;
        
        std::string methodName = asStringIden(1);
        TypeFunc* fnType = nullptr;

        if(args.size() == 3) {
            if(instanceof<NodeCall>(args[2])) fnType = callToTFunc(((NodeCall*)args[2]));
            else if(instanceof<NodeType>(args[2])) fnType = (TypeFunc*)((NodeType*)args[2])->type;
            else {
                generator->error("undefined type of method \033[1m" + methodName + "\033[22m!", this->loc);
                return nullptr;
            }
        }

        auto it = AST::methodTable.find({tstruct->name, methodName});
        if(it != AST::methodTable.end() && (fnType == nullptr)) return new NodeBool(true);

        return new NodeBool(false);
    }
    else if(name == "hasDestructor") {
        Type* ty = asType(0)->type;
        if(!instanceof<TypeStruct>(ty)) return new NodeBool(false);
        TypeStruct* tstruct = (TypeStruct*)ty;

        if(AST::structTable.find(tstruct->name) == AST::structTable.end()) return new NodeBool(false);
        return new NodeBool(AST::structTable[tstruct->name]->destructor == nullptr);
    }
    else if(name == "minOf") {
        if(this->args.size() < 1) generator->error("at least one argument is required!", this->loc);
        Type* type = asType(0)->type;

        if(!instanceof<TypeVoid>(type) && !instanceof<TypeBasic>(type)) generator->error("the type must be a basic!", this->loc);
        if(instanceof<TypeVoid>(type)) return new NodeInt(0);
        else {
            TypeBasic* btype = (TypeBasic*)type;
            switch(btype->type) {
                case BasicType::Bool: case BasicType::Uchar: case BasicType::Ushort:
                case BasicType::Uint: case BasicType::Ulong: case BasicType::Ucent: return new NodeInt(0);
                case BasicType::Char: return new NodeInt(-128);
                case BasicType::Short: return new NodeInt(-32768);
                case BasicType::Int: return new NodeInt(-2147483647);
                case BasicType::Long: return new NodeInt(BigInt("-9223372036854775807"));
                case BasicType::Cent: return new NodeInt(BigInt("-170141183460469231731687303715884105728"));
                default: return new NodeInt(0);
            }
        }
    }
    else if(name == "maxOf") {
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

    AST::checkError("builtin with name \033[1m" + name + "\033[22m does not exist!", this->loc);
    return nullptr;
}