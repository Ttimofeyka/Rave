/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "../../include/parser/nodes/NodeBool.hpp"
#include "../../include/utils.hpp"
#include "../../include/parser/ast.hpp"

NodeBool::NodeBool(bool value) {this->value = value;}
void NodeBool::check() {this->isChecked = true;}
LLVMValueRef NodeBool::generate() {return LLVMConstInt(LLVMInt1TypeInContext(generator->context),value,false);}
Type* NodeBool::getType() {return new TypeBasic(BasicType::Bool);}
Node* NodeBool::copy() {return new NodeBool(this->value);}
Node* NodeBool::comptime() {return this;}