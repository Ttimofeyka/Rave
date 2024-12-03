/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "../../include/parser/nodes/NodeSizeof.hpp"
#include "../../include/parser/nodes/NodeInt.hpp"
#include "../../include/parser/nodes/NodeString.hpp"
#include "../../include/parser/nodes/NodeType.hpp"

NodeSizeof::NodeSizeof(Node* value, long loc) {
    this->value = value;
    this->loc = loc;
}

Type* NodeSizeof::getType() {return basicTypes[BasicType::Int];}
Node* NodeSizeof::comptime() {return this;}
Node* NodeSizeof::copy() {return new NodeSizeof(this->value->copy(), this->loc);}
void NodeSizeof::check() {this->isChecked = true;}

RaveValue NodeSizeof::generate() {
    if(instanceof<NodeType>(this->value)) {
        Type* tp = ((NodeType*)this->value)->getType();
        while(generator->toReplace.find(tp->toString()) != generator->toReplace.end()) tp = generator->toReplace[tp->toString()];
        if(instanceof<TypeBasic>(tp)) {
            switch(((TypeBasic*)tp)->type) {
                case BasicType::Uchar: case BasicType::Char: case BasicType::Bool: return {LLVMConstInt(LLVMInt32TypeInContext(generator->context), 1, false), basicTypes[BasicType::Int]};
                case BasicType::Ushort: case BasicType::Short: case BasicType::Half: case BasicType::Bhalf: return {LLVMConstInt(LLVMInt32TypeInContext(generator->context), 2, false), basicTypes[BasicType::Int]};
                case BasicType::Uint: case BasicType::Int:
                case BasicType::Float: return {LLVMConstInt(LLVMInt32TypeInContext(generator->context), 4, false), basicTypes[BasicType::Int]};
                case BasicType::Ulong: case BasicType::Long:
                case BasicType::Double: return {LLVMConstInt(LLVMInt32TypeInContext(generator->context), 8, false), basicTypes[BasicType::Int]};
                case BasicType::Ucent: case BasicType::Cent: return {LLVMConstInt(LLVMInt32TypeInContext(generator->context), 16, false), basicTypes[BasicType::Int]};
                default: break;
            }
        }
        else return {LLVMSizeOf(generator->genType(tp, loc)), basicTypes[BasicType::Long]};
    }
    return {LLVMSizeOf(LLVMTypeOf(this->value->generate().value)), basicTypes[BasicType::Long]};
}