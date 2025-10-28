/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "../../include/parser/nodes/NodeInt.hpp"
#include "../../include/utils.hpp"
#include "../../include/parser/ast.hpp"

NodeInt::NodeInt(BigInt value, unsigned char sys) 
    : value(value), sys(sys), type(BasicType::Int), isVarVal(nullptr), 
      isUnsigned(false), isMustBeLong(false), isMustBeChar(false), isMustBeShort(false) {}

NodeInt::NodeInt(BigInt value, char type, Type* isVarVal, unsigned char sys, bool isUnsigned, bool isMustBeLong)
    : value(value), type(type), sys(sys), isVarVal(isVarVal), 
      isUnsigned(isUnsigned), isMustBeLong(isMustBeLong), isMustBeChar(false), isMustBeShort(false) {}

Node* NodeInt::comptime() { return this; }

void NodeInt::check() { isChecked = true; }

char NodeInt::determineBaseType() const {
    if (isMustBeLong) return BasicType::Long;
    if (isMustBeShort) return BasicType::Short;
    if (isMustBeChar) return BasicType::Char;

    if (isVarVal != nullptr && instanceof<TypeBasic>(isVarVal)) 
        return ((TypeBasic*)isVarVal)->type;

    if (value >= INT32_MIN && value <= INT32_MAX) return BasicType::Int;
    if (value >= INT64_MIN && value <= INT64_MAX) return BasicType::Long;
    return BasicType::Cent;
}

Type* NodeInt::getType() {
    char baseType = determineBaseType();

    if (!isUnsigned) return basicTypes[baseType];

    static constexpr char unsignedTypes[] = {BasicType::Uchar, BasicType::Ushort, BasicType::Uint, BasicType::Ulong, BasicType::Ucent };

    return basicTypes[unsignedTypes[baseType - BasicType::Char]];
}

RaveValue NodeInt::generate() {
    type = determineBaseType();

    LLVMTypeRef intType = (isVarVal && instanceof<TypeBasic>(isVarVal)) 
        ? getTypeForBasicType(type)
        : LLVMIntTypeInContext(generator->context, 
            type == BasicType::Char ? 8 : type == BasicType::Short ? 16 :
            type == BasicType::Int ? 32 : type == BasicType::Long ? 64 : 128);

    return {LLVMConstIntOfString(intType, value.to_string().c_str(), sys), basicTypes[type]};
}

LLVMTypeRef NodeInt::getTypeForBasicType(char type) {
    static constexpr LLVMTypeRef (*typeFuncs[])(LLVMContextRef) = {
        LLVMInt1TypeInContext,   // Bool
        LLVMInt8TypeInContext,   // Char
        LLVMInt16TypeInContext,  // Short
        LLVMInt32TypeInContext,  // Int
        LLVMInt64TypeInContext,  // Long
        LLVMInt128TypeInContext, // Cent
        nullptr, nullptr, nullptr, nullptr, nullptr, // Float types (not used)
        LLVMInt8TypeInContext,   // Uchar
        LLVMInt16TypeInContext,  // Ushort
        LLVMInt32TypeInContext,  // Uint
        LLVMInt64TypeInContext,  // Ulong
        LLVMInt128TypeInContext  // Ucent
    };

    if (type >= BasicType::Bool && type <= BasicType::Ucent && typeFuncs[type])
        return typeFuncs[type](generator->context);

    return LLVMInt32TypeInContext(generator->context);
}

Node* NodeInt::copy() {
    return new NodeInt(BigInt(value), type, isVarVal, sys, isUnsigned, isMustBeLong);
}
