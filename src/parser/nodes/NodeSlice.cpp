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
    if((two <= one) || (one < 0) || (instanceof<TypeArray>(base->getType()) && ((TypeArray*)base->getType())->count < two.to_int())) generator->error("incorrect slice values!", loc);
    return new TypeArray(one.to_int(), base->getType());
}

Node* NodeSlice::comptime() {return this;}
Node* NodeSlice::copy() {return new NodeSlice(this->base, this->start, this->end, this->loc);}
void NodeSlice::check() {this->isChecked = true;}

LLVMValueRef NodeSlice::generate() {
    if(!instanceof<NodeInt>(start) || !instanceof<NodeInt>(end)) generator->error("NodeSlice temporarily supports only constant numbers!", loc); // TODO: Rework it
    int one = ((NodeInt*)start)->value.to_int();
    int two = ((NodeInt*)end)->value.to_int();
    if((two <= one) || (one < 0) || (instanceof<TypeArray>(base->getType()) && ((TypeArray*)base->getType())->count < two)) generator->error("incorrect slice values!", loc);

    if(instanceof<NodeIden>(base)) ((NodeIden*)base)->isMustBePtr = false;
    else if(instanceof<NodeGet>(base)) ((NodeGet*)base)->isMustBePtr = false;
    else if(instanceof<NodeIndex>(base)) ((NodeIndex*)base)->isMustBePtr = false;

    LLVMValueRef lBase = base->generate();
    LLVMValueRef buffer = LLVM::alloc(LLVMArrayType(LLVMGetElementType(LLVMTypeOf(lBase)), two - one), "NodeSlice_buffer");
    LLVMValueRef tempBuffer = LLVM::load(buffer, "load");

    for(int i=one, j=0; i<two; i++, j++) {
        if(LLVMGetTypeKind(LLVMTypeOf(lBase)) == LLVMArrayTypeKind)
            tempBuffer = LLVMBuildInsertValue(generator->builder, tempBuffer, LLVMBuildExtractValue(generator->builder, lBase, j, "extract"), j, "insert");
    }

    return tempBuffer;
}