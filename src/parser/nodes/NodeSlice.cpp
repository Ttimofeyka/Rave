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
#include "../../include/parser/ast.hpp"
#include "../../include/llvm.hpp"

NodeSlice::NodeSlice(Node* base, Node* start, Node* end, int loc) {
    this->base = base;
    this->start = start;
    this->end = end;
    this->loc = loc;
}

Type* NodeSlice::getType() {
    if(!instanceof<NodeInt>(start) || !instanceof<NodeInt>(end)) generator->error("NodeSlice temporarily supports only constant numbers!", loc); // TODO: Rework it
    BigInt one = ((NodeInt*)start)->value;
    BigInt two = ((NodeInt*)end)->value;
    if((two <= one) || (one < 0) || (instanceof<TypeArray>(base->getType()) && ((NodeInt*)((TypeArray*)base->getType())->count->comptime())->value.to_int() < two.to_int())) generator->error("incorrect slice values!", loc);
    return new TypeArray(new NodeInt(two.to_int() - one.to_int()), base->getType());
}

Node* NodeSlice::comptime() {return this;}
Node* NodeSlice::copy() {return new NodeSlice(this->base, this->start, this->end, this->loc);}
void NodeSlice::check() {this->isChecked = true;}

RaveValue NodeSlice::generate() {
    if(!instanceof<NodeInt>(start) || !instanceof<NodeInt>(end)) generator->error("NodeSlice temporarily supports only constant numbers!", loc); // TODO: Rework it
    int one = ((NodeInt*)start)->value.to_int();
    int two = ((NodeInt*)end)->value.to_int();
    if((two <= one) || (one < 0) || (instanceof<TypeArray>(base->getType()) && ((NodeInt*)((TypeArray*)base->getType())->count->comptime())->value.to_int() < two)) generator->error("incorrect slice values!", loc);

    if(instanceof<NodeIden>(base)) ((NodeIden*)base)->isMustBePtr = false;
    else if(instanceof<NodeGet>(base)) ((NodeGet*)base)->isMustBePtr = false;
    else if(instanceof<NodeIndex>(base)) ((NodeIndex*)base)->isMustBePtr = false;

    RaveValue lBase = base->generate();
    RaveValue buffer = LLVM::alloc(new TypeArray(new NodeInt(two - one), lBase.type->getElType()), "NodeSlice_buffer");
    RaveValue tempBuffer = LLVM::load(buffer, "load", loc);

    if(instanceof<TypeArray>(lBase.type)) for(int i=one, j=0; i<two; i++, j++) {
        tempBuffer.value = LLVMBuildInsertValue(generator->builder, tempBuffer.value, LLVMBuildExtractValue(generator->builder, lBase.value, j, "extract"), j, "insert");
    }

    return tempBuffer;
}