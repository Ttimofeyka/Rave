/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "../../include/parser/nodes/NodeString.hpp"
#include "../../include/utils.hpp"
#include "../../include/parser/ast.hpp"
#include "../../include/utf8.h"
#include "../../include/llvm.hpp"

NodeString::NodeString(std::string value, bool isWide) {
    this->value = value;
    this->isWide = isWide;
}

Node* NodeString::copy() {return new NodeString(this->value, this->isWide);}
Type* NodeString::getType() {return new TypePointer(new TypeBasic((isWide ? BasicType::Uint : BasicType::Char)));}
Node* NodeString::comptime() {return this;}
void NodeString::check() {this->isChecked = true;}

LLVMValueRef NodeString::generate() {
    LLVMTypeRef elemType = isWide ? LLVMInt32TypeInContext(generator->context) : LLVMInt8TypeInContext(generator->context);
    LLVMTypeRef arrayType = LLVMArrayType(elemType, value.size() + 1);

    LLVMValueRef globalStr = LLVMAddGlobal(generator->lModule, arrayType, "_str");
    LLVMSetGlobalConstant(globalStr, true);
    LLVMSetUnnamedAddr(globalStr, true);
    LLVMSetLinkage(globalStr, LLVMPrivateLinkage);
    LLVMSetAlignment(globalStr, isWide ? 4 : 1);

    if(!isWide) LLVMSetInitializer(globalStr, LLVMConstStringInContext(generator->context, value.c_str(), value.size(), false));
    else {
        std::u32string u32Str = utf8::utf8to32(value);
        std::vector<LLVMValueRef> values(u32Str.size());
        for(int i=0; i<u32Str.size(); i++) values[i] = LLVMConstInt(elemType, u32Str[i], false);
        LLVMSetInitializer(globalStr, LLVMConstArray(elemType, values.data(), values.size()));
    }

    LLVMValueRef indices[2] = {
        LLVMConstInt(LLVMInt32TypeInContext(generator->context), 0, false),
        LLVMConstInt(LLVMInt32TypeInContext(generator->context), 0, false)
    };

    return LLVM::constInboundsGep(globalStr, indices, 2);
}