/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "../../include/parser/nodes/NodeSlice.hpp"
#include "../../include/parser/nodes/NodeInt.hpp"
#include "../../include/parser/nodes/NodeBinary.hpp"
#include "../../include/parser/nodes/NodeGet.hpp"
#include "../../include/parser/nodes/NodeDone.hpp"
#include "../../include/parser/nodes/NodeIden.hpp"
#include "../../include/parser/nodes/NodeStruct.hpp"
#include "../../include/parser/nodes/NodeVar.hpp"
#include "../../include/parser/nodes/NodeIndex.hpp"
#include "../../include/parser/nodes/NodeArray.hpp"
#include "../../include/parser/nodes/NodeFor.hpp"
#include "../../include/parser/nodes/NodeCall.hpp"
#include "../../include/parser/nodes/NodeFunc.hpp"
#include "../../include/parser/ast.hpp"
#include "../../include/llvm.hpp"

NodeSlice::NodeSlice(Node* base, Node* start, Node* end, int loc) {
    this->base = base;
    this->start = start;
    this->end = end;
    this->loc = loc;
}

Type* NodeSlice::getType() {
    if(!instanceof<NodeInt>(start) || !instanceof<NodeInt>(end)) return new TypePointer(base->getType());
    BigInt one = ((NodeInt*)start)->value;
    BigInt two = ((NodeInt*)end)->value;
    if((two <= one) || (one < 0) || (instanceof<TypeArray>(base->getType()) && ((NodeInt*)((TypeArray*)base->getType())->count->comptime())->value.to_int() < two.to_int())) generator->error("incorrect slice values!", loc);
    return new TypeArray(new NodeInt(two.to_int() - one.to_int()), base->getType());
}

Node* NodeSlice::comptime() {return this;}
Node* NodeSlice::copy() {return new NodeSlice(this->base, this->start, this->end, this->loc);}
void NodeSlice::check() {this->isChecked = true;}

NodeSlice::~NodeSlice() {
    if(this->base != nullptr) delete this->base;
    if(this->start != nullptr) delete this->start;
    if(this->end != nullptr) delete this->end;
}

RaveValue NodeSlice::generate() {
    if(instanceof<NodeIden>(base)) ((NodeIden*)base)->isMustBePtr = false;
    else if(instanceof<NodeGet>(base)) ((NodeGet*)base)->isMustBePtr = false;
    else if(instanceof<NodeIndex>(base)) ((NodeIndex*)base)->isMustBePtr = false;

    if(!instanceof<NodeInt>(start) || !instanceof<NodeInt>(end)) {
        RaveValue lStart = start->generate();
        RaveValue lEnd = end->generate();
        RaveValue sliceSize = LLVM::sub(lEnd, lStart);

        Node* equNode = nullptr;
        Type* elType = nullptr;

        if(instanceof<TypeStruct>(base->getType())) {
            std::string _struct = base->getType()->toString();
    
            if(AST::structTable.find(_struct) != AST::structTable.end()) {
                NodeStruct* _structPtr = AST::structTable[_struct];
                if(_structPtr->operators.find(TokType::Rbra) != _structPtr->operators.end()) {
                    equNode = new NodeCall(
                        this->loc, new NodeIden(AST::structTable[_struct]->operators[TokType::Rbra][_structPtr->operators[TokType::Rbra].begin()->first]->name, this->loc),
                        std::vector<Node*>({base, new NodeInt(0)})
                    );

                    elType = equNode->getType();

                    ((NodeCall*)equNode)->args[1] = new NodeIden("__RAVE_NODESLICE_I", loc);
                }
            }
        }
        else {
            equNode = new NodeIndex(base, {new NodeIden("__RAVE_NODESLICE_I", loc)}, loc);
            elType = base->getType()->getElType();
        }

        int typeSize = elType->getSize();

        if(typeSize < 8) typeSize = 8;

        RaveValue sizeOf = (new NodeInt(typeSize / 8))->generate();

        RaveValue buffer = LLVM::alloc(LLVM::mul(sliceSize, sizeOf), "NodeSlice_dynbuffer");
        ((TypePointer*)buffer.type)->instance = elType;

        NodeFor* _for = new NodeFor(
            {
                new NodeVar("__RAVE_NODESLICE_I", new NodeDone(lStart), false, false, false, {}, loc, new TypeBasic(BasicType::Int)),
                new NodeVar("__RAVE_NODESLICE_J", new NodeInt(0), false, false, false, {}, loc, new TypeBasic(BasicType::Int))
            },
            new NodeBinary(TokType::Less, new NodeIden("__RAVE_NODESLICE_I", loc), new NodeDone(lEnd), loc),
            {
                new NodeBinary(TokType::PluEqu, new NodeIden("__RAVE_NODESLICE_I", loc), new NodeInt(1), loc),
                new NodeBinary(TokType::PluEqu, new NodeIden("__RAVE_NODESLICE_J", loc), new NodeInt(1), loc)
            },
            new NodeBlock({
                new NodeBinary(TokType::Equ, new NodeIndex(new NodeDone(buffer), {new NodeIden("__RAVE_NODESLICE_J", loc)}, loc), equNode, loc)
            }), loc
        );

        _for->generate();

        return buffer;
    }

    int one = ((NodeInt*)start)->value.to_int();
    int two = ((NodeInt*)end)->value.to_int();
    if((two <= one) || (one < 0) || (instanceof<TypeArray>(base->getType()) && ((NodeInt*)((TypeArray*)base->getType())->count->comptime())->value.to_int() < two)) generator->error("incorrect slice values!", loc);

    RaveValue lBase = base->generate();

    if(instanceof<TypeStruct>(lBase.type)) {
        std::string _struct = lBase.type->toString();

        if(AST::structTable.find(_struct) != AST::structTable.end()) {
            NodeStruct* _structPtr = AST::structTable[_struct];
            if(_structPtr->operators.find(TokType::Rbra) != _structPtr->operators.end()) {
                NodeCall* call = new NodeCall(
                    this->loc, new NodeIden(AST::structTable[_struct]->operators[TokType::Rbra][_structPtr->operators[TokType::Rbra].begin()->first]->name, this->loc),
                    std::vector<Node*>({base, new NodeInt(0)})
                );

                RaveValue buffer = LLVM::alloc(new TypeArray(new NodeInt(two - one), call->getType()), "NodeSlice_buffer");
                RaveValue tempBuffer = LLVM::load(buffer, "load", loc);

                for(int i=one, j=0; i<two; i++, j++) {
                    tempBuffer.value = LLVMBuildInsertValue(generator->builder, tempBuffer.value, call->generate().value, j, "insert");
                    ((NodeInt*)call->args[1])->value += 1;
                }

                return tempBuffer;
            }
        }
    }

    RaveValue buffer = LLVM::alloc(new TypeArray(new NodeInt(two - one), lBase.type->getElType()), "NodeSlice_buffer");
    RaveValue tempBuffer = LLVM::load(buffer, "load", loc);

    if(instanceof<TypeArray>(lBase.type)) for(int i=one, j=0; i<two; i++, j++) {
        tempBuffer.value = LLVMBuildInsertValue(generator->builder, tempBuffer.value, LLVMBuildExtractValue(generator->builder, lBase.value, j, "extract"), j, "insert");
    }

    return tempBuffer;
}