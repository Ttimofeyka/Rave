/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "../../include/parser/nodes/NodeSizeof.hpp"
#include "../../include/parser/nodes/NodeInt.hpp"
#include "../../include/parser/nodes/NodeString.hpp"
#include "../../include/parser/nodes/NodeType.hpp"

NodeSizeof::NodeSizeof(Node* value, int loc) : value(value), loc(loc) {}

Type* NodeSizeof::getType() { return basicTypes[BasicType::Int]; }

Node* NodeSizeof::comptime() { return this; }

Node* NodeSizeof::copy() { return new NodeSizeof(value->copy(), loc); }

void NodeSizeof::check() { isChecked = true; }

RaveValue NodeSizeof::generate() {
    if (instanceof<NodeType>(value)) {
        Type* tp = ((NodeType*)value)->getType();
        Types::replaceTemplates(&tp);

        if (instanceof<TypeBasic>(tp)) {
            switch (((TypeBasic*)tp)->type) {
                case BasicType::Uchar: case BasicType::Char: case BasicType::Bool:
                    return { LLVM::makeInt(32, 1, false), basicTypes[BasicType::Int] };
                case BasicType::Ushort: case BasicType::Short: case BasicType::Half: case BasicType::Bhalf:
                    return { LLVM::makeInt(32, 2, false), basicTypes[BasicType::Int] };
                case BasicType::Uint: case BasicType::Int: case BasicType::Float:
                    return { LLVM::makeInt(32, 4, false), basicTypes[BasicType::Int] };
                case BasicType::Ulong: case BasicType::Long: case BasicType::Double:
                    return { LLVM::makeInt(32, 8, false), basicTypes[BasicType::Int] };
                case BasicType::Ucent: case BasicType::Cent:
                    return { LLVM::makeInt(32, 16, false), basicTypes[BasicType::Int] };
                default: break;
            }
        }

        return { LLVMSizeOf(generator->genType(tp, loc)), basicTypes[BasicType::Long] };
    }

    return { LLVMSizeOf(LLVMTypeOf(value->generate().value)), basicTypes[BasicType::Long] };
}

NodeSizeof::~NodeSizeof() {
    if (value) delete value;
}
