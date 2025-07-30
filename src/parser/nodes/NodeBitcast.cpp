/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "../../include/parser/ast.hpp"
#include "../../include/parser/nodes/NodeBitcast.hpp"
#include "../../include/llvm.hpp"
#include <iostream>

NodeBitcast::NodeBitcast(Type* type, Node* value, int loc) {
    this->type = type;
    this->value = value;
    this->loc = loc;
}

void NodeBitcast::check() {
    if (isChecked) return;
    isChecked = true;

    value->check();
}

NodeBitcast::~NodeBitcast() {
    if (this->value != nullptr) delete this->value;
    if (this->type != nullptr && !instanceof<TypeBasic>(this->type) && !instanceof<TypeVoid>(this->type)) delete this->type;
}

Type* NodeBitcast::getType() {return this->type->copy();}
Node* NodeBitcast::copy() {return new NodeBitcast(this->type->copy(), this->value->copy(), this->loc);}
Node* NodeBitcast::comptime() {return nullptr;}

RaveValue NodeBitcast::generate() {
    RaveValue result = this->value->generate();
    Types::replaceTemplates(&this->type);
    
    return {
        LLVMBuildBitCast(
            generator->builder, 
            result.value, 
            generator->genType(this->type, this->loc), 
            "NodeBitcast"
        ),
        this->type
    };
}
