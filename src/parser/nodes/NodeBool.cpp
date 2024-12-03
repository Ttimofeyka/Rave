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
RaveValue NodeBool::generate() {return {LLVM::makeInt(1, value, false), basicTypes[BasicType::Bool]};}
Type* NodeBool::getType() {return basicTypes[BasicType::Bool];}
Node* NodeBool::copy() {return new NodeBool(this->value);}
Node* NodeBool::comptime() {return this;}