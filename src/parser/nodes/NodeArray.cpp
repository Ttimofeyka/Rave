/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "../../include/parser/nodes/NodeArray.hpp"
#include "../../include/utils.hpp"
#include "../../include/parser/ast.hpp"
#include "../../include/llvm.hpp"

NodeArray::NodeArray(long loc, std::vector<Node*> values) {
    this->loc = loc;
    this->values = std::vector<Node*>(values);
}

Type* NodeArray::getType() {
    if(this->values.size() > 0) return new TypeArray(this->values.size(), this->values[0]->getType());
    return new TypeVoid();
}

std::vector<LLVMValueRef> NodeArray::getValues() {
    std::vector<LLVMValueRef> buffer;
    LLVMValueRef v0 = values[0]->generate();
    buffer.push_back(v0);
    this->type = LLVMTypeOf(v0);
    if(!LLVMIsConstant(v0)) isConst = false;
    for(int i=1; i<this->values.size(); i++) {
        buffer.push_back(values[i]->generate());
        if(!LLVMIsConstant(buffer[buffer.size()-1])) isConst = false;
    }
    return buffer;
}

LLVMValueRef NodeArray::generate() {
    std::vector<LLVMValueRef> genValues = this->getValues();
    if(isConst) return LLVMConstArray(this->type, genValues.data(), values.size());
    LLVMValueRef arr = LLVMBuildAlloca(generator->builder, LLVMArrayType(this->type, this->values.size()), "NodeArray");
    for(int i=0; i<this->values.size(); i++) {
        LLVMBuildStore(
            generator->builder, genValues[i],
            generator->byIndex(arr, std::vector<LLVMValueRef>({LLVMConstInt(LLVMInt32TypeInContext(generator->context), i, false)}))
        );
    }
    return LLVM::load(arr, "loadNodeArray");
}

void NodeArray::check() {this->isChecked = true;}
Node* NodeArray::comptime() {return this;}
Node* NodeArray::copy() {return new NodeArray(this->loc, this->values);}