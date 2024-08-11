/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "../../include/parser/nodes/NodeConstStruct.hpp"
#include "../../include/parser/nodes/NodeBinary.hpp"
#include "../../include/parser/nodes/NodeGet.hpp"
#include "../../include/parser/nodes/NodeDone.hpp"
#include "../../include/parser/nodes/NodeIden.hpp"
#include "../../include/parser/nodes/NodeStruct.hpp"
#include "../../include/parser/ast.hpp"
#include "../../include/llvm.hpp"

NodeConstStruct::NodeConstStruct(std::string name, std::vector<Node*> values, int loc) {
    this->structName = name;
    this->values = values;
    this->loc = loc;
}

Type* NodeConstStruct::getType() {return new TypeStruct(this->structName);}
Node* NodeConstStruct::comptime() {return this;}
Node* NodeConstStruct::copy() {return new NodeConstStruct(this->structName, this->values, this->loc);}
void NodeConstStruct::check() {this->isChecked = true;}

LLVMValueRef NodeConstStruct::generate() {
    LLVMTypeRef constStructType = generator->genType(new TypeStruct(this->structName), this->loc);
    std::vector<LLVMValueRef> llvmValues;

    for(int i=0; i<this->values.size(); i++) llvmValues.push_back(this->values[i]->generate());
    return LLVMConstNamedStruct(constStructType, llvmValues.data(), llvmValues.size());
}