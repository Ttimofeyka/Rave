/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "../../include/parser/nodes/NodeAsm.hpp"
#include "../../include/utils.hpp"
#include "../../include/parser/ast.hpp"
#include "../../include/llvm.hpp"

NodeAsm::NodeAsm(std::string line, bool isVolatile, Type* type, std::string additions, std::vector<Node*> values, int loc) {
    this->line = line;
    this->isVolatile = isVolatile;
    this->type = type;
    this->additions = additions;
    this->values = std::vector<Node*>(values);
    this->loc = loc;
}

Node* NodeAsm::copy() {
    std::vector<Node*> buffer;
    for(size_t i=0; i<this->values.size(); i++) buffer.push_back(this->values[i]->copy());
    return new NodeAsm(this->line, this->isVolatile, this->type->copy(), this->additions, buffer, this->loc);
}

Type* NodeAsm::getType() {return this->type;}
Node* NodeAsm::comptime() {return nullptr;}
void NodeAsm::check() {isChecked = true;}

RaveValue NodeAsm::generate() {
    std::vector<LLVMValueRef> values;
    std::vector<LLVMTypeRef> types;

    TypeFunc* tfunc = new TypeFunc(this->type, {}, false);

    for(size_t i=0; i<this->values.size(); i++) {
        RaveValue value = this->values[i]->generate();
        tfunc->args.push_back(new TypeFuncArg(value.type, "i" + std::to_string(i)));

        values.push_back(value.value);
        types.push_back(LLVMTypeOf(value.value));
    }

    return LLVM::call({LLVMGetInlineAsm(
        LLVMFunctionType(generator->genType(this->type, this->loc), types.data(), types.size(), false),
        (char*)this->line.c_str(),
        this->line.size(),
        (char*)this->additions.c_str(),
        this->additions.size(),
        this->isVolatile,
        false,
        LLVMInlineAsmDialectATT,
        false
    ), tfunc}, values.data(), values.size(), (instanceof<TypeVoid>(this->type) ? "" : "asm_"));
}

NodeAsm::~NodeAsm() {
    if(this->type != nullptr && !instanceof<TypeBasic>(this->type) && !instanceof<TypeVoid>(this->type)) delete this->type;
    
    for(size_t i=0; i<this->values.size(); i++) if(this->values[i] != nullptr) delete this->values[i];
}