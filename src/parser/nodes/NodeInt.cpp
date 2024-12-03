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
    if(this->isMustBeLong) return basicTypes[BasicType::Long];
    if(this->isMustBeShort) return basicTypes[BasicType::Short];
    if(this->isMustBeChar) return basicTypes[BasicType::Char];

    char baseType;
    if(this->isVarVal != nullptr && instanceof<TypeBasic>(this->isVarVal)) baseType = (((TypeBasic*)this->isVarVal)->type);
    else {
        if(this->value >= INT32_MIN && this->value <= INT32_MAX) baseType = BasicType::Int;
        else if(this->value >= INT64_MIN && this->value <= INT64_MAX) baseType = BasicType::Long;
        else baseType = BasicType::Cent;
    }

    if(!isUnsigned) return basicTypes[baseType];

    constexpr char unsignedTypes[] = {
        BasicType::Uchar, BasicType::Ushort, BasicType::Uint,
        BasicType::Ulong, BasicType::Ucent
    };
    
    return basicTypes[unsignedTypes[baseType - BasicType::Char]];
}

RaveValue NodeInt::generate() {
    LLVMTypeRef intType = nullptr;
    char baseType = BasicType::Int;

    if(isMustBeLong) {
        baseType = BasicType::Long;
        intType = LLVMInt64TypeInContext(generator->context);
    }
    else if(isMustBeShort) {
        baseType = BasicType::Short;
        intType = LLVMInt16TypeInContext(generator->context);
    }
    else if(isMustBeChar) {
        baseType = BasicType::Char;
        intType = LLVMInt8TypeInContext(generator->context);
    }
    else if(this->isVarVal != nullptr && instanceof<TypeBasic>(this->isVarVal)) {
        baseType = ((TypeBasic*)this->isVarVal)->type;
        intType = getTypeForBasicType(baseType);
    }
    else {
        if(value >= INT32_MIN && value <= INT32_MAX) {
            baseType = BasicType::Int;
            intType = LLVMInt32TypeInContext(generator->context);
        }
        else if(value >= INT64_MIN && value <= INT64_MAX) {
            baseType = BasicType::Long;
            intType = LLVMInt64TypeInContext(generator->context);
        }
        else {
            baseType = BasicType::Cent;
            intType = LLVMInt128TypeInContext(generator->context);
        }
    }

    this->type = baseType;
    return {LLVMConstIntOfString(intType, value.to_string().c_str(), this->sys), basicTypes[baseType]};
}

LLVMTypeRef NodeInt::getTypeForBasicType(char type) {
    switch(type) {
        case BasicType::Bool: return LLVMInt1TypeInContext(generator->context);
        case BasicType::Uchar:
        case BasicType::Char: return LLVMInt8TypeInContext(generator->context);
        case BasicType::Ushort:
        case BasicType::Short: return LLVMInt16TypeInContext(generator->context);
        case BasicType::Uint:
        case BasicType::Int: return LLVMInt32TypeInContext(generator->context);
        case BasicType::Ulong:
        case BasicType::Long: return LLVMInt64TypeInContext(generator->context);
        case BasicType::Ucent:
        case BasicType::Cent: return LLVMInt128TypeInContext(generator->context);
        default: return LLVMInt32TypeInContext(generator->context);
    }
}

Node* NodeInt::copy() {return new NodeInt(BigInt(this->value), this->type, this->isVarVal, this->sys, this->isUnsigned, this->isMustBeLong);}