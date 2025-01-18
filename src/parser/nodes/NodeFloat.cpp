/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "../../include/parser/nodes/NodeFloat.hpp"
#include "../../include/utils.hpp"
#include "../../include/parser/ast.hpp"
#include <limits>
#include <iostream>

NodeFloat::NodeFloat(double value) {this->value = std::to_string(value);}
NodeFloat::NodeFloat(std::string value) {this->value = value;}
NodeFloat::NodeFloat(double value, bool isDouble) {if(isDouble) this->type = basicTypes[BasicType::Double]; this->value = std::to_string(value);}
NodeFloat::NodeFloat(double value, TypeBasic* type) {this->value = std::to_string(value); this->type = type;}
NodeFloat::NodeFloat(std::string value, TypeBasic* type) {this->value = value; this->type = type;}

Type* NodeFloat::getType() {
    if(this->type != nullptr) return this->type;

    if(this->isMustBeFloat) {
        this->type = basicTypes[BasicType::Float];
        return this->type;
    }

    R128 r128;
    r128FromString(&r128, this->value.c_str(), nullptr);
    this->type = new TypeBasic((r128 > R128(std::numeric_limits<float>::max())) ? BasicType::Double : (BasicType::Float));

    return this->type;
}

RaveValue NodeFloat::generate() {
    if(this->isMustBeFloat) {
        if(this->type == nullptr || ((TypeBasic*)this->type)->type != BasicType::Float) this->type = basicTypes[BasicType::Float];
        return {LLVMConstRealOfString(LLVMFloatTypeInContext(generator->context), this->value.c_str()), basicTypes[BasicType::Float]};
    }

    if(this->type == nullptr) this->type = basicTypes[BasicType::Double];
    else if(this->type->type == BasicType::Half) return {LLVMConstRealOfString(LLVMHalfTypeInContext(generator->context), this->value.c_str()), basicTypes[BasicType::Half]};
    else if(this->type->type == BasicType::Bhalf) return {LLVMConstRealOfString(LLVMBFloatTypeInContext(generator->context), this->value.c_str()), basicTypes[BasicType::Bhalf]};
    else if(this->type->type == BasicType::Real) return {LLVMConstRealOfString(LLVMFP128TypeInContext(generator->context), this->value.c_str()), basicTypes[BasicType::Real]};
    return {LLVMConstRealOfString(LLVMDoubleTypeInContext(generator->context), this->value.c_str()), basicTypes[BasicType::Double]};
}

Node* NodeFloat::copy() {
    NodeFloat* nf = new NodeFloat(this->value, (this->type == nullptr) ? nullptr : (TypeBasic*)this->type->copy());
    nf->isMustBeFloat = this->isMustBeFloat;
    return nf;
}

void NodeFloat::check() {this->isChecked = true;}
Node* NodeFloat::comptime() {return this;}