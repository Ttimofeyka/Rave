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
#include "../../include/parser/nodes/NodeVar.hpp"
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

    bool isConst = true;

    for(int i=0; i<this->values.size(); i++) {
        LLVMValueRef generated = this->values[i]->generate();

        if(!LLVMIsConstant(generated)) isConst = false;

        llvmValues.push_back(generated);
    }

    if(isConst) return LLVMConstNamedStruct(constStructType, llvmValues.data(), llvmValues.size());
    else {
        if(currScope == nullptr) generator->error("constant structure with dynamic values cannot be created outside a function!", this->loc);
        LLVMValueRef temp = LLVM::alloc(constStructType, "constStruct_temp");

        std::vector<std::string> elements;
        for(int i=0; i<AST::structTable[this->structName]->elements.size(); i++) {
            if(instanceof<NodeVar>(AST::structTable[this->structName]->elements[i])) elements.push_back(((NodeVar*)AST::structTable[this->structName]->elements[i])->name);
        }

        for(int i=0; i<this->values.size(); i++) {
            (new NodeBinary(TokType::Equ, new NodeGet(new NodeDone(temp), elements[i], true, this->loc), new NodeDone(llvmValues[i]), this->loc))->generate();
        }

        return LLVM::load(temp, "constStruct_tempLoad");
    }
}