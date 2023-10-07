/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "../../include/parser/nodes/NodeFloat.hpp"
#include "../../include/utils.hpp"
#include "../../include/parser/ast.hpp"
#include <limits>

NodeFloat::NodeFloat(double value) {this->value = value;}
NodeFloat::NodeFloat(double value, bool isDouble) {if(isDouble) this->type = new TypeBasic(BasicType::Double); this->value = value;}
NodeFloat::NodeFloat(double value, TypeBasic* type) {this->value = value; this->type = type;}

Type* NodeFloat::getType() {
    if(this->type != nullptr) return this->type;
    this->type = new TypeBasic((this->value > std::numeric_limits<float>::max()) ? BasicType::Double : BasicType::Float);
    return this->type;
}

LLVMValueRef NodeFloat::generate() {
    if(this->type == nullptr) this->type = new TypeBasic((this->value > std::numeric_limits<float>::max()) ? BasicType::Double : BasicType::Float);
    return LLVMConstReal(this->type->type == BasicType::Float ? LLVMFloatTypeInContext(generator->context) : LLVMDoubleTypeInContext(generator->context), this->value);
}

Node* NodeFloat::copy() {return new NodeFloat(this->value, (this->type == nullptr) ? nullptr : (TypeBasic*)this->type->copy());}
void NodeFloat::check() {this->isChecked = true;}
Node* NodeFloat::comptime() {return this;}