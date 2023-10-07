/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "../../include/parser/nodes/NodeInt.hpp"
#include "../../include/utils.hpp"
#include "../../include/parser/ast.hpp"

NodeInt::NodeInt(BigInt value, unsigned char sys) {
    this->value = value;
    this->sys = sys;
}

NodeInt::NodeInt(BigInt value, char type, Type* isVarVal, unsigned char sys, bool isUnsigned, bool isMustBeLong) {
    this->value = value;
    this->type = type;
    this->sys = sys;
    this->isVarVal = isVarVal;
    this->isUnsigned = isUnsigned;
    this->isMustBeLong = isMustBeLong;
}

Node* NodeInt::comptime() {return this;}
void NodeInt::check() {this->isChecked = true;}

Type* NodeInt::getType() {
    if(this->isMustBeLong) return new TypeBasic(BasicType::Long);
    char _type;
    if(this->isVarVal != nullptr && instanceof<TypeBasic>(this->isVarVal)) _type = this->type = ((TypeBasic*)this->isVarVal)->type;
    else {
        if(this->value < INT32_MAX) _type = BasicType::Int;
        else if(this->value < INT64_MAX) _type = BasicType::Long;
        else _type = BasicType::Cent;
    }
    if(!isUnsigned) return new TypeBasic(_type);
    switch(_type) {
        case BasicType::Char: return new TypeBasic(BasicType::Uchar);
        case BasicType::Short: return new TypeBasic(BasicType::Ushort);
        case BasicType::Int: return new TypeBasic(BasicType::Uint);
        case BasicType::Long: return new TypeBasic(BasicType::Ulong);
        case BasicType::Cent: return new TypeBasic(BasicType::Ucent);
        default: return new TypeBasic(_type);
     }
}

LLVMValueRef NodeInt::generate() {
    if(isMustBeLong) return LLVMConstIntOfString(LLVMInt64TypeInContext(generator->context),value.to_string().c_str(),this->sys);
    if(this->isVarVal != nullptr && instanceof<TypeBasic>(this->isVarVal)) {
        this->type = ((TypeBasic*)this->isVarVal)->type;
        switch(this->type) {
            case BasicType::Bool: return LLVMConstIntOfString(LLVMInt1TypeInContext(generator->context), value.to_string().c_str(), this->sys);
            case BasicType::Uchar:
            case BasicType::Char: return LLVMConstIntOfString(LLVMInt8TypeInContext(generator->context), value.to_string().c_str(), this->sys);
            case BasicType::Ushort:
            case BasicType::Short: return LLVMConstIntOfString(LLVMInt16TypeInContext(generator->context), value.to_string().c_str(), this->sys);
            case BasicType::Uint:
            case BasicType::Int: return LLVMConstIntOfString(LLVMInt32TypeInContext(generator->context), value.to_string().c_str(), this->sys);
            case BasicType::Ulong:
            case BasicType::Long: return LLVMConstIntOfString(LLVMInt64TypeInContext(generator->context), value.to_string().c_str(), this->sys);
            case BasicType::Ucent:
            case BasicType::Cent: return LLVMConstIntOfString(LLVMInt128TypeInContext(generator->context), value.to_string().c_str(), this->sys);
            default: break;
        }
    }
    if(value < INT32_MAX) {
        this->type = BasicType::Int;
        return LLVMConstIntOfString(LLVMInt32TypeInContext(generator->context), value.to_string().c_str(), this->sys);
    }
    if(value < INT64_MAX) {
        this->type = BasicType::Long;
        return LLVMConstIntOfString(LLVMInt64TypeInContext(generator->context), value.to_string().c_str(), this->sys);
    }
    this->type = BasicType::Cent;
    return LLVMConstIntOfString(LLVMInt128TypeInContext(generator->context), value.to_string().c_str(), this->sys);
}

Node* NodeInt::copy() {return new NodeInt(BigInt(this->value), this->type, this->isVarVal, this->sys, this->isUnsigned, this->isMustBeLong);}