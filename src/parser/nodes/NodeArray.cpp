/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "../../include/parser/nodes/NodeArray.hpp"
#include "../../include/parser/nodes/NodeInt.hpp"
#include "../../include/utils.hpp"
#include "../../include/parser/ast.hpp"
#include "../../include/llvm.hpp"

NodeArray::NodeArray(int loc, std::vector<Node*> values) {
    this->loc = loc;
    this->values = std::vector<Node*>(values);
}

Type* NodeArray::getType() {
    if(values.size() > 0) return new TypeArray(new NodeInt(values.size()), values[0]->getType());
    return typeVoid;
}

NodeArray::~NodeArray() {
    for(size_t i=0; i<values.size(); i++) if(values[i] != nullptr) delete values[i];
}

std::vector<RaveValue> NodeArray::getValues() {
    std::vector<RaveValue> buffer;

    type = values[0]->getType();

    for(size_t i=0; i<values.size(); i++) {
        buffer.push_back(values[i]->generate());
        if(!LLVMIsConstant(buffer[buffer.size() - 1].value)) isConst = false;
    }

    return buffer;
}

RaveValue NodeArray::generate() {
    std::vector<RaveValue> genValues = getValues();

    // If this is a constant array - just return LLVM constant array with provided values
    if(isConst) return LLVM::makeCArray(type, genValues);

    RaveValue arr = LLVM::alloc(new TypeArray(new NodeInt(values.size()), type), "NodeArray");

    for(size_t i=0; i<values.size(); i++)
        LLVMBuildStore(generator->builder, genValues[i].value, generator->byIndex(arr, std::vector<LLVMValueRef>({LLVM::makeInt(pointerSize, i, true)})).value);

    return LLVM::load(arr, "loadNodeArray", loc);
}

void NodeArray::check() {isChecked = true;}
Node* NodeArray::comptime() {return this;}
Node* NodeArray::copy() {return new NodeArray(loc, values);}