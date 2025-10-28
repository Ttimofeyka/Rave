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

NodeFloat::NodeFloat(double value, TypeBasic* type, bool isDouble) 
    : value(std::to_string(value)), type(type) {
    if (isDouble && !type) this->type = basicTypes[BasicType::Double];
}

NodeFloat::NodeFloat(std::string value, TypeBasic* type) 
    : value(value), type(type) {}

Type* NodeFloat::getType() {
    if (type) return type;

    if (isMustBeFloat) {
        type = basicTypes[BasicType::Float];
        return type;
    }

    R128 r128;
    r128FromString(&r128, value.c_str(), nullptr);
    type = new TypeBasic((r128 > R128(std::numeric_limits<float>::max())) ? BasicType::Double : BasicType::Float);
    return type;
}

namespace {
    struct LLVMTypeMapping {
        BasicType::BasicType basicType;
        LLVMTypeRef (*getTypeFunc)(LLVMContextRef);
    };

    static const LLVMTypeMapping typeMappings[] = {
        {BasicType::Half, LLVMHalfTypeInContext},
        {BasicType::Bhalf, LLVMBFloatTypeInContext},
        {BasicType::Float, LLVMFloatTypeInContext},
        {BasicType::Double, LLVMDoubleTypeInContext},
        {BasicType::Real, LLVMFP128TypeInContext}
    };

    static const size_t typeMappingsCount = sizeof(typeMappings) / sizeof(typeMappings[0]);

    LLVMTypeRef getLLVMTypeForBasicType(BasicType::BasicType basicType, LLVMContextRef context) {
        for (size_t i=0; i<typeMappingsCount; i++) {
            if (typeMappings[i].basicType == basicType)
                return typeMappings[i].getTypeFunc(context);
        }
        return LLVMDoubleTypeInContext(context);
    }
}

RaveValue NodeFloat::generate() {
    if (isMustBeFloat) {
        if (!type || ((TypeBasic*)type)->type != BasicType::Float) type = basicTypes[BasicType::Float];

        return { LLVMConstRealOfString(LLVMFloatTypeInContext(generator->context), value.c_str()), basicTypes[BasicType::Float] };
    }

    if (!type) type = basicTypes[BasicType::Double];

    const auto basicType = static_cast<TypeBasic*>(type);
    LLVMTypeRef llvmType = getLLVMTypeForBasicType(static_cast<BasicType::BasicType>(basicType->type), generator->context);
    Type* resultType = basicTypes[static_cast<BasicType::BasicType>(basicType->type)];

    return { LLVMConstRealOfString(llvmType, value.c_str()), resultType };
}

Node* NodeFloat::copy() {
    NodeFloat* nf = new NodeFloat(value, type ? static_cast<TypeBasic*>(type->copy()) : nullptr);
    nf->isMustBeFloat = isMustBeFloat;
    return nf;
}

void NodeFloat::check() { isChecked = true; }

Node* NodeFloat::comptime() { return this; }
