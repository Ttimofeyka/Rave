/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <llvm-c/Core.h>
#include "Node.hpp"
#include "../Types.hpp"
#include <vector>
#include <string>
#include "../../BigInt.hpp"

// AST variant of an integer number.

class NodeInt : public Node {
public:
    BigInt value;
    char type;
    Type* isVarVal = nullptr;
    unsigned char sys;
    bool isUnsigned = false;
    bool isMustBeLong = false;
    bool isMustBeChar = false;
    bool isMustBeShort = false;

    NodeInt(BigInt value, unsigned char sys = 10);
    NodeInt(BigInt value, char type, Type* isVarVal, unsigned char sys, bool isUnsigned, bool isMustBeLong);
    Type* getType() override;
    RaveValue generate() override;
    Node* copy() override;
    Node* comptime() override;
    void check() override;
    LLVMTypeRef getTypeForBasicType(char type);

private:
    char determineBaseType() const;
};
