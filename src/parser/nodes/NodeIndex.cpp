/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "../../include/parser/nodes/NodeIndex.hpp"
#include "../../include/parser/nodes/NodeVar.hpp"
#include "../../include/parser/nodes/NodeIden.hpp"
#include "../../include/parser/nodes/NodeCast.hpp"
#include "../../include/parser/nodes/NodeCall.hpp"
#include "../../include/parser/nodes/NodeGet.hpp"
#include "../../include/parser/nodes/NodeUnary.hpp"
#include "../../include/parser/nodes/NodeStruct.hpp"
#include "../../include/parser/nodes/NodeInt.hpp"
#include "../../include/parser/nodes/NodeFunc.hpp"
#include "../../include/parser/nodes/NodeBinary.hpp"
#include "../../include/parser/nodes/NodeDone.hpp"
#include "../../include/parser/nodes/NodeNull.hpp"
#include "../../include/parser/nodes/NodeString.hpp"
#include "../../include/parser/ast.hpp"
#include <vector>
#include <string>
#include "../../include/utils.hpp"
#include "../../include/parser/Types.hpp"
#include "../../include/llvm.hpp"

NodeIndex::NodeIndex(Node* element, std::vector<Node*> indexes, int loc) {
    this->element = element;
    this->indexes = indexes;
    this->loc = loc;
}

Type* NodeIndex::getType() {
    Type* type = this->element->getType();
    
    // Remove const qualifiers
    while(instanceof<TypeConst>(type)) type = static_cast<TypeConst*>(type)->instance;
    
    if(instanceof<TypePointer>(type)) {
        Type* tp = static_cast<TypePointer*>(type)->instance;
        return instanceof<TypeVoid>(tp) ? new TypeBasic(BasicType::Char) : tp;
    }
    
    if(instanceof<TypeArray>(type)) return static_cast<TypeArray*>(type)->element;
    
    if(instanceof<TypeStruct>(type)) {
        TypeStruct* tstruct = static_cast<TypeStruct*>(type);
        auto& operators = AST::structTable[tstruct->name]->operators;
        auto it = operators.find(TokType::Rbra);
        if(it != operators.end() && !it->second.empty()) return it->second.begin()->second->type;
    }
    
    generator->error("Invalid type in NodeIndex::getType: " + type->toString(), this->loc);
    return nullptr;
}

void NodeIndex::check() {
    bool oldCheck = this->isChecked;
    this->isChecked = true;
    if(!oldCheck) this->element->check();
}

Node* NodeIndex::copy() {return new NodeIndex(this->element->copy(), this->indexes, this->loc);}
Node* NodeIndex::comptime() {return this;}

std::vector<LLVMValueRef> NodeIndex::generateIndexes() {
    std::vector<LLVMValueRef> buffer;
    for(int i=0; i<this->indexes.size(); i++) buffer.push_back(this->indexes[i]->generate());
    return buffer;
}

bool NodeIndex::isElementConst(Type* type) {
    Type* currType = type;

    while(instanceof<TypeConst>(currType)) currType = ((TypeConst*)currType)->instance;

    if(instanceof<TypePointer>(currType)) return (instanceof<TypeConst>(((TypePointer*)currType)->instance));
    if(instanceof<TypeArray>(currType)) return (instanceof<TypeConst>(((TypeArray*)currType)->element));

    return false;
}

LLVMValueRef NodeIndex::generate() {
    if(instanceof<NodeIden>(this->element)) {
        NodeIden* id = (NodeIden*)this->element;
        Type* _t = currScope->getVar(id->name, this->loc)->type;
        if(instanceof<TypeStruct>(_t) || ((instanceof<TypePointer>(_t) && instanceof<TypeStruct>(((TypePointer*)_t)->instance)))) {
            Type* tstruct = nullptr;
            if(instanceof<TypeStruct>(_t)) tstruct = _t;
            else tstruct = ((TypePointer*)_t)->instance;
            while(generator->toReplace.find(tstruct->toString()) != generator->toReplace.end()) tstruct = (TypeStruct*)generator->toReplace[tstruct->toString()];

            if(instanceof<TypeStruct>(tstruct)) {
                std::string structName = ((TypeStruct*)tstruct)->name;
                if(AST::structTable.find(structName) != AST::structTable.end()) {
                    if(AST::structTable[structName]->operators.find(TokType::Rbra) != AST::structTable[structName]->operators.end()) {
                        for(auto const& x : AST::structTable[structName]->operators[TokType::Rbra]) {
                            return (new NodeCall(this->loc, new NodeIden(AST::structTable[structName]->operators[TokType::Rbra][x.first]->name, this->loc),
                                std::vector<Node*>({id, this->indexes[0]})))->generate();
                        }
                    }
                }
            }
        }
        if(instanceof<TypePointer>(_t)) {
            if(instanceof<TypeConst>(((TypePointer*)_t)->instance)) this->elementIsConst = true;
        }
        else if(instanceof<TypeArray>(_t)) {
            if(instanceof<TypeConst>(((TypeArray*)_t)->element)) this->elementIsConst = true;
        }
        else if(instanceof<TypeConst>(_t)) this->elementIsConst = this->isElementConst(((TypeConst*)_t)->instance);
        LLVMValueRef ptr = currScope->get(id->name, this->loc);

        if(LLVMGetTypeKind(LLVMTypeOf(ptr)) == LLVMArrayTypeKind && LLVMGetTypeKind(LLVMTypeOf(currScope->getWithoutLoad(id->name, this->loc))) == LLVMArrayTypeKind) {
            LLVMValueRef copyVal = LLVM::alloc(LLVMTypeOf(ptr), ("NodeIndex_copyVal_"+std::to_string(this->loc)+"_").c_str());
            LLVMBuildStore(generator->builder, ptr, copyVal);
            ptr = copyVal;
        }
        else if(!LLVM::isPointer(ptr)) ptr = currScope->getWithoutLoad(id->name, this->loc);

        if(!generator->settings.noChecks && generator->settings.optLevel <= 2 &&
            ((AST::funcTable.find(currScope->funcName) != AST::funcTable.end() && AST::funcTable[currScope->funcName]->isNoChecks == false)
            || AST::funcTable.find(currScope->funcName) == AST::funcTable.end())
        ) {
            if(AST::funcTable.find("std::assert[_b_pc]") != AST::funcTable.end()) {
                (new NodeCall(this->loc, new NodeIden("std::assert[_b_pc]", this->loc), {
                    new NodeBinary(TokType::Nequal, new NodeDone(ptr), new NodeNull(nullptr, this->loc), this->loc),
                    new NodeString("Assert in '" + generator->file + "' file in function '" + currScope->funcName + "' at " + std::to_string(this->loc) + " line: trying to get a value from a null pointer!\n", false)
                }))->generate();
            }
        }

        LLVMValueRef index = generator->byIndex(ptr, this->generateIndexes());
        if(isMustBePtr) return index;
        return LLVM::load(index, ("NodeIndex_NodeIden_load_"+std::to_string(this->loc)+"_").c_str());
    }
    if(instanceof<NodeGet>(this->element)) {
        NodeGet* nget = (NodeGet*)this->element;
        nget->isMustBePtr = true;
        LLVMValueRef ptr = nget->generate();

        if(
            LLVMGetTypeKind(LLVMTypeOf(ptr)) == LLVMStructTypeKind ||
            (LLVM::isPointer(ptr) && LLVMGetTypeKind(LLVMGetElementType(LLVMTypeOf(ptr))) == LLVMStructTypeKind)
        ) {
            LLVMTypeRef structLType = LLVMTypeOf(ptr);
            if(LLVMGetTypeKind(structLType) != LLVMStructTypeKind) structLType = LLVMGetElementType(structLType);

            Type* structType = new TypeStruct(std::string(LLVMGetStructName(structLType)));
            while(generator->toReplace.find(structType->toString()) != generator->toReplace.end()) structType = generator->toReplace[structType->toString()];
            if(instanceof<TypeStruct>(structType)) {
                std::string structName = structType->toString();
                if(AST::structTable.find(structName) != AST::structTable.end()) {
                    if(AST::structTable[structName]->operators.find(TokType::Rbra) != AST::structTable[structName]->operators.end()) {
                        for(auto const& x : AST::structTable[structName]->operators[TokType::Rbra]) {
                            return (new NodeCall(
                                this->loc, new NodeIden(AST::structTable[structName]->operators[TokType::Rbra][x.first]->name, this->loc),
                                std::vector<Node*>({new NodeDone(ptr), this->indexes[0]})
                            ))->generate();
                        }
                    }
                }
            }
        }

        this->elementIsConst = nget->elementIsConst;
        if(LLVMGetTypeKind(LLVMGetElementType(LLVMTypeOf(ptr))) != LLVMArrayTypeKind) {ptr = LLVM::load(ptr, ("NodeIndex_NodeGet_load" + std::to_string(this->loc) + "_").c_str());}
        LLVMValueRef index = generator->byIndex(ptr, this->generateIndexes());
        if(isMustBePtr) return index;
        return LLVM::load(index, ("NodeIndex_NodeGet" + std::to_string(this->loc) + "_").c_str());
    }
    if(instanceof<NodeCall>(this->element)) {
        NodeCall* ncall = (NodeCall*)this->element;
        LLVMValueRef vr = ncall->generate();

        if(!generator->settings.noChecks && generator->settings.optLevel <= 2 &&
            ((AST::funcTable.find(currScope->funcName) != AST::funcTable.end() && AST::funcTable[currScope->funcName]->isNoChecks == false)
            || AST::funcTable.find(currScope->funcName) == AST::funcTable.end())
        ) {
            if(AST::funcTable.find("std::assert[_b_pc]") != AST::funcTable.end()) {
                (new NodeCall(this->loc, new NodeIden("std::assert[_b_pc]", this->loc), {
                    new NodeBinary(TokType::Nequal, new NodeDone(vr), new NodeNull(nullptr, this->loc), this->loc),
                    new NodeString("Assert in '" + generator->file + "' file in function '" + currScope->funcName + "' at " + std::to_string(this->loc) + " line: trying to get a value from a null pointer!\n", false)
                }))->generate();
            }
        }

        LLVMValueRef index = generator->byIndex(vr, this->generateIndexes());
        if(isMustBePtr) return index;
        return LLVM::load(index, ("NodeIndex_NodeCall_load" + std::to_string(this->loc)+"_").c_str());
    }
    if(instanceof<NodeDone>(element)) {
        LLVMValueRef index = generator->byIndex(element->generate(), this->generateIndexes());
        if(isMustBePtr) return index;
        return LLVM::load(index, "NodeDone_load");
    }
    if(instanceof<NodeCast>(this->element)) {
        NodeCast* ncast = (NodeCast*)this->element;
        LLVMValueRef val = ncast->generate();

        if(!generator->settings.noChecks && generator->settings.optLevel <= 2 &&
            ((AST::funcTable.find(currScope->funcName) != AST::funcTable.end() && AST::funcTable[currScope->funcName]->isNoChecks == false)
            || AST::funcTable.find(currScope->funcName) == AST::funcTable.end())
        ) {
            if(AST::funcTable.find("std::assert[_b_pc]") != AST::funcTable.end()) {
                (new NodeCall(this->loc, new NodeIden("std::assert[_b_pc]", this->loc), {
                    new NodeBinary(TokType::Nequal, new NodeDone(val), new NodeNull(nullptr, this->loc), this->loc),
                    new NodeString("Assert in '" + generator->file + "' file in function '" + currScope->funcName + "' at " + std::to_string(this->loc) + " line: trying to get a value from a null pointer!\n", false)
                }))->generate();
            }
        }

        LLVMValueRef index = generator->byIndex(val, this->generateIndexes());
        if(isMustBePtr) return index;
        return LLVM::load(index, "NodeIndex_NodeCast_load");
    }
    if(instanceof<NodeUnary>(this->element)) {
        NodeUnary* nunary = (NodeUnary*)this->element;
        LLVMValueRef val = nunary->generate();

        if(!generator->settings.noChecks && generator->settings.optLevel <= 2 &&
            ((AST::funcTable.find(currScope->funcName) != AST::funcTable.end() && AST::funcTable[currScope->funcName]->isNoChecks == false)
            || AST::funcTable.find(currScope->funcName) == AST::funcTable.end())
        ) {
            if(AST::funcTable.find("std::assert[_b_pc]") != AST::funcTable.end()) {
                (new NodeCall(this->loc, new NodeIden("std::assert[_b_pc]", this->loc), {
                    new NodeBinary(TokType::Nequal, new NodeDone(val), new NodeNull(nullptr, this->loc), this->loc),
                    new NodeString("Assert in '" + generator->file + "' file in function '" + currScope->funcName + "' at " + std::to_string(this->loc) + " line: trying to get a value from a null pointer!\n", false)
                }))->generate();
            }
        }

        LLVMValueRef index = generator->byIndex(val, this->generateIndexes());
        return index;
    }
    generator->error("NodeIndex::generate assert!",this->loc);
    return nullptr;
}