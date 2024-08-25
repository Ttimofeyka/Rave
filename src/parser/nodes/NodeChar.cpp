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

RaveValue NodeChar::generate() {
    if(this->value.size() > 1 && this->value[0] == '\\') {
        if(isdigit(this->value[1])) return {LLVM::makeInt(isWide ? 32 : 8, std::stol(value.substr(1)), false), new TypeBasic(isWide ? BasicType::Int : BasicType::Char)};
        switch(this->value[1]) {
            case 'n': return {LLVM::makeInt(isWide ? 32 : 8, 10, false), new TypeBasic(isWide ? BasicType::Int : BasicType::Char)};
            case 'v': return {LLVM::makeInt(isWide ? 32 : 8, 11, false), new TypeBasic(isWide ? BasicType::Int : BasicType::Char)};
            case 'r': return {LLVM::makeInt(isWide ? 32 : 8, 13, false), new TypeBasic(isWide ? BasicType::Int : BasicType::Char)};
            default: break;
        }
    }
    if(this->value.size() < 2) return {LLVM::makeInt(isWide ? 32 : 8, value[0], false), new TypeBasic(isWide ? BasicType::Int : BasicType::Char)};
    return {LLVM::makeInt(32, utf8::utf8to32(this->value)[0], false), new TypeBasic(BasicType::Int)};
}