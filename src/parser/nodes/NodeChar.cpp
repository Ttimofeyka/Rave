/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "../../include/parser/nodes/NodeChar.hpp"
#include "../../include/utils.hpp"
#include "../../include/parser/ast.hpp"
#include "../../include/utf8.h"
#include <iostream>

NodeChar::NodeChar(std::string value, bool isWide) {
    this->value = value;
    this->isWide = isWide;
}

Node* NodeChar::copy() {return new NodeChar(this->value, this->isWide);}
Type* NodeChar::getType() {return new TypeBasic(isWide ? BasicType::Uint : BasicType::Char);}
Node* NodeChar::comptime() {return this;}
void NodeChar::check() {this->isChecked = true;}

LLVMValueRef NodeChar::generate() {
    if(this->value.size() > 1 && this->value[0] == '\\') {
        if(isdigit(this->value[1])) return LLVMConstInt(isWide ? LLVMInt32TypeInContext(generator->context) : LLVMInt8TypeInContext(generator->context), std::stol(this->value.substr(1)), false);
    }
    if(this->value.size() < 2) return LLVMConstInt(isWide ? LLVMInt32TypeInContext(generator->context) : LLVMInt8TypeInContext(generator->context), this->value[0], false);
    return LLVMConstInt(LLVMInt32TypeInContext(generator->context), utf8::utf8to32(this->value)[0], false);
}