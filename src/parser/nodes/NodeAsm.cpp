/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "../../include/parser/nodes/NodeAsm.hpp"
#include "../../include/utils.hpp"
#include "../../include/parser/ast.hpp"
#include "../../include/llvm.hpp"

NodeAsm::NodeAsm(std::string line, bool isVolatile, Type* type, std::string additions, std::vector<Node*> values, long loc) {
    this->line = line;
    this->isVolatile = isVolatile;
    this->type = type;
    this->additions = additions;
    this->values = std::vector<Node*>(values);
    this->loc = loc;
}

Node* NodeAsm::copy() {
    std::vector<Node*> buffer;
    for(int i=0; i<this->values.size(); i++) buffer.push_back(this->values[i]->copy());
    return new NodeAsm(this->line, this->isVolatile, this->type->copy(), this->additions, buffer, this->loc);
}

Type* NodeAsm::getType() {return this->type;}
Node* NodeAsm::comptime() {return nullptr;}
void NodeAsm::check() {this->isChecked = true;}

LLVMValueRef NodeAsm::generate() {
    std::vector<LLVMValueRef> values;
    std::vector<LLVMTypeRef> types;

    for(int i=0; i<this->values.size(); i++) {
        LLVMValueRef value = this->values[i]->generate();
        values.push_back(value);
        types.push_back(LLVMTypeOf(value));
    }

    return LLVM::call(LLVMGetInlineAsm(
        LLVMFunctionType(generator->genType(this->type, this->loc), types.data(), types.size(), false),
        (char*)this->line.c_str(),
        this->line.size(),
        (char*)this->additions.c_str(),
        this->additions.size(),
        this->isVolatile,
        false,
        LLVMInlineAsmDialectATT,
        false
    ), values.data(), values.size(), (instanceof<TypeVoid>(this->type) ? "" : "asm_"));
}