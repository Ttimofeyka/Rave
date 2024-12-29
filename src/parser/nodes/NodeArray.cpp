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
    if(this->values.size() > 0) return new TypeArray(new NodeInt(this->values.size()), this->values[0]->getType());
    return typeVoid;
}

NodeArray::~NodeArray() {
    for(int i=0; i<values.size(); i++) if(this->values[i] != nullptr) delete this->values[i];
}

std::vector<RaveValue> NodeArray::getValues() {
    std::vector<RaveValue> buffer;
    RaveValue v0 = values[0]->generate();

    buffer.push_back(v0);
    this->type = v0.type;

    if(!LLVMIsConstant(v0.value)) isConst = false;

    for(int i=1; i<this->values.size(); i++) {
        buffer.push_back(values[i]->generate());

        if(!LLVMIsConstant(buffer[buffer.size() - 1].value)) {
            isConst = false;
            break;
        }
    }
    return buffer;
}

RaveValue NodeArray::generate() {
    std::vector<RaveValue> genValues = this->getValues();

    // If this is a constant array - just return LLVM constant array with provided values
    if(isConst) return LLVM::makeCArray(this->type, genValues);

    RaveValue arr = LLVM::alloc(new TypeArray(new NodeInt(this->values.size()), this->type), "NodeArray");

    for(int i=0; i<this->values.size(); i++) {
        LLVMBuildStore(generator->builder, genValues[i].value, generator->byIndex(arr, std::vector<LLVMValueRef>({LLVM::makeInt(32, i, false)})).value);
    }

    return LLVM::load(arr, "loadNodeArray", loc);
}

void NodeArray::check() {this->isChecked = true;}
Node* NodeArray::comptime() {return this;}
Node* NodeArray::copy() {return new NodeArray(this->loc, this->values);}