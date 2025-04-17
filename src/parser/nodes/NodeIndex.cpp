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
#include "../../include/parser/nodes/NodeBuiltin.hpp"
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

NodeIndex::~NodeIndex() {
    if(element != nullptr) delete element;
    for(int i=0; i<indexes.size(); i++) if(indexes[i] != nullptr) delete indexes[i];
}

Type* NodeIndex::getType() {
    Type* type = this->element->getType();
    
    // Remove const qualifiers
    while(instanceof<TypeConst>(type)) type = static_cast<TypeConst*>(type)->instance;
    
    if(instanceof<TypePointer>(type)) {
        Type* tp = static_cast<TypePointer*>(type)->instance;
        return instanceof<TypeVoid>(tp) ? basicTypes[BasicType::Char] : tp;
    }
    
    if(instanceof<TypeArray>(type)) return static_cast<TypeArray*>(type)->element;
    
    if(instanceof<TypeStruct>(type)) {
        TypeStruct* tstruct = static_cast<TypeStruct*>(type);
        auto& operators = AST::structTable[tstruct->name]->operators;
        auto it = operators.find(TokType::Rbra);
        if(it != operators.end() && !it->second.empty()) return it->second.begin()->second->type;
    }

    if(instanceof<TypeVector>(type)) return static_cast<TypeVector*>(type)->getElType();
    
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
    for(int i=0; i<this->indexes.size(); i++) buffer.push_back(this->indexes[i]->generate().value);
    return buffer;
}

bool NodeIndex::isElementConst(Type* type) {
    Type* currType = type;

    while(instanceof<TypeConst>(currType)) currType = ((TypeConst*)currType)->instance;

    if(instanceof<TypePointer>(currType)) return (instanceof<TypeConst>(((TypePointer*)currType)->instance));
    if(instanceof<TypeArray>(currType)) return (instanceof<TypeConst>(((TypeArray*)currType)->element));

    return false;
}

RaveValue NodeIndex::generate() {
    if(instanceof<NodeIden>(this->element)) {
        NodeIden* id = (NodeIden*)this->element;
        if(currScope->localVars.find(id->name) != currScope->localVars.end()) currScope->localVars[id->name]->isUsed = true;
        Type* _t = currScope->getVar(id->name, this->loc)->type;

        if(instanceof<TypeVector>(_t)) {
            if(isMustBePtr) return currScope->getWithoutLoad(id->name, this->loc);
            return {LLVMBuildExtractElement(generator->builder, this->element->generate().value, this->indexes[0]->generate().value, "NodeIndex_vector"), this->getType()};
        }

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
        RaveValue ptr = currScope->get(id->name, this->loc);

        if(instanceof<TypeArray>(ptr.type) && instanceof<TypeArray>(currScope->getWithoutLoad(id->name, this->loc).type)) LLVM::makeAsPointer(ptr);
        else if(!instanceof<TypePointer>(ptr.type)) ptr = currScope->getWithoutLoad(id->name, this->loc);

        if(!generator->settings.noChecks && generator->settings.optLevel <= 2 && !LLVMIsAConstant(ptr.value) &&
            (AST::funcTable.find(currScope->funcName) == AST::funcTable.end() || !AST::funcTable[currScope->funcName]->isNoChecks)
        ) {
            auto assertFunc = AST::funcTable.find("std::assert[_b_pc]");
            if(assertFunc != AST::funcTable.end()) {
                (new NodeCall(loc, new NodeIden(assertFunc->second->name, this->loc), {
                    new NodeBinary(TokType::Nequal, new NodeDone(ptr), new NodeNull(nullptr, loc), loc),
                    new NodeString("Assert in '" + generator->file + "' file in function '" + currScope->funcName + "' at " + std::to_string(loc) + " line: trying to get a value from a null pointer!\n", false)
                }))->generate();
            }
        }

        RaveValue index = generator->byIndex(ptr, this->generateIndexes());

        if(isMustBePtr) return index;

        RaveValue loaded = LLVM::load(index, ("NodeIndex_NodeIden_load_" + std::to_string(this->loc) + "_").c_str(), loc);
        if(instanceof<TypeVector>(loaded.type)) return {LLVMBuildExtractElement(generator->builder, loaded.value, indexes[indexes.size() - 1]->generate().value, "NodeIndex_vector"), loaded.type->getElType()};
        
        return loaded;
    }

    if(instanceof<NodeGet>(this->element)) {
        NodeGet* nget = (NodeGet*)this->element;
        nget->isMustBePtr = true;

        if(nget->isPtrForIndex) {
            RaveValue ptr = nget->generate();
            if(instanceof<TypePointer>(((TypePointer*)ptr.type)->instance)) ptr = LLVM::load(ptr, "NodeIndex_NodeGet_ptrIndex", loc);

            RaveValue index = indexes[0]->generate();
            index.type = basicTypes[BasicType::Long];
            index.value = LLVMBuildIntCast(generator->builder, index.value, LLVMInt64TypeInContext(generator->context), "icast");
            
            ptr.value = LLVMBuildIntToPtr(
                generator->builder,
                LLVMBuildAdd(generator->builder, LLVMBuildMul(
                    generator->builder,
                    LLVM::makeInt(64, (((TypePointer*)(ptr.type))->instance->getSize()) / 8, false), index.value, "add"
                ), LLVMBuildPtrToInt(generator->builder, ptr.value, LLVMInt64TypeInContext(generator->context), "itop"), "add2"),
                generator->genType(ptr.type, loc), "ptoi"
            );
            return ptr;
        }

        RaveValue ptr = nget->generate();

        if(instanceof<TypeStruct>(ptr.type) || (instanceof<TypePointer>(ptr.type) && instanceof<TypeStruct>(ptr.type->getElType()))) {
            Type* structType = ptr.type;
            if(!instanceof<TypeStruct>(structType)) structType = ptr.type->getElType();

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

        if(instanceof<TypeVector>(ptr.type)) {
            if(isMustBePtr) return ptr;
            return {LLVMBuildExtractElement(generator->builder, ptr.value, this->indexes[0]->generate().value, "NodeDone_TypeVector"), ptr.type->getElType()};
        }

        this->elementIsConst = nget->elementIsConst;
        if(!instanceof<TypeArray>(ptr.type->getElType())) {ptr = LLVM::load(ptr, ("NodeIndex_NodeGet_load" + std::to_string(this->loc) + "_").c_str(), loc);}
        RaveValue index = generator->byIndex(ptr, this->generateIndexes());

        if(isMustBePtr) return index;

        return LLVM::load(index, ("NodeIndex_NodeGet" + std::to_string(this->loc) + "_").c_str(), loc);
    }

    if(instanceof<NodeCall>(this->element)) {
        NodeCall* ncall = (NodeCall*)this->element;
        RaveValue vr = ncall->generate();

        if(instanceof<TypeVector>(vr.type)) {
            if(isMustBePtr) return vr;
            return {LLVMBuildExtractElement(generator->builder, vr.value, this->indexes[0]->generate().value, "NodeDone_TypeVector"), vr.type->getElType()};
        }

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

        RaveValue index = generator->byIndex(vr, this->generateIndexes());
        if(isMustBePtr) return index;

        return LLVM::load(index, ("NodeIndex_NodeCall_load" + std::to_string(this->loc) + "_").c_str(), loc);
    }

    if(instanceof<NodeDone>(element)) {
        NodeDone* done = ((NodeDone*)element);

        if(instanceof<TypeVector>(done->value.type)) {
            if(isMustBePtr) return done->value;
            return {LLVMBuildExtractElement(generator->builder, done->value.value, this->indexes[0]->generate().value, "NodeDone_TypeVector"), done->value.type->getElType()};
        }

        RaveValue index = generator->byIndex(element->generate(), this->generateIndexes());
        if(isMustBePtr) return index;
        return LLVM::load(index, "NodeDone_load", loc);
    }

    if(instanceof<NodeCast>(this->element)) {
        NodeCast* ncast = (NodeCast*)this->element;
        RaveValue val = ncast->generate();

        if(instanceof<TypeVector>(val.type)) {
            if(isMustBePtr) return val;
            return {LLVMBuildExtractElement(generator->builder, val.value, this->indexes[0]->generate().value, "NodeIndex_vector"), this->getType()};
        }

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

        RaveValue index = generator->byIndex(val, this->generateIndexes());
        if(isMustBePtr) return index;

        return LLVM::load(index, "NodeIndex_NodeCast_load", loc);
    }

    if(instanceof<NodeBuiltin>(this->element)) {
        NodeBuiltin* nbuiltin = (NodeBuiltin*)this->element;
        RaveValue value = nbuiltin->generate();

        if(instanceof<TypeVector>(value.type)) {
            if(isMustBePtr) return value;
            return {LLVMBuildExtractElement(generator->builder, value.value, this->indexes[0]->generate().value, "NodeIndex_vector"), this->getType()};
        }

        RaveValue index = generator->byIndex(value, this->generateIndexes());
        if(isMustBePtr) return index;

        return LLVM::load(index, "NodeIndex_NodeBuiltin_load", loc);
    }

    if(instanceof<NodeUnary>(this->element)) {
        NodeUnary* nunary = (NodeUnary*)this->element;
        Type* nunaryType = nunary->base->getType();

        if((nunary->type == TokType::GetPtr) && (instanceof<TypeStruct>(nunaryType) || (instanceof<TypePointer>(nunaryType) && instanceof<TypeStruct>(((TypePointer*)nunaryType)->instance)))) {
            TypeStruct* tstruct = instanceof<TypeStruct>(nunaryType) ? (TypeStruct*)nunaryType : (TypeStruct*)(((TypePointer*)nunaryType)->instance);
            
            if(AST::structTable.find(tstruct->name) != AST::structTable.end()) {
                auto& operators = AST::structTable[tstruct->name]->operators;
                if(operators.find('&') != operators.end()) {
                    std::map<std::string, NodeFunc*> functions = operators['&'];
                    Node* value = nunary;

                    return (new NodeCall(loc, new NodeIden(functions.begin()->second->name, loc), {nunary, indexes[0]}))->generate();
                }
            }
        }

        RaveValue val = nunary->generate();

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

        RaveValue index = generator->byIndex(val, this->generateIndexes());
        return index;
    }

    generator->error("NodeIndex::generate assert!",this->loc);
    return {};
}
