/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <vector>
#include "./ast.hpp"

// Forward declarations for functions in Types.hpp
class Type;
bool isFloatType(Type* type);
bool isBytePointer(Type* type);

namespace TypeMatching {
    // Check if two types are compatible for function argument matching
    // isExplicit: if true, disable implicit conversions
    bool areTypesCompatible(Type* param, Type* arg, bool isExplicit = false);

    // Check if argument list matches parameter list
    // Returns true if all arguments can be passed to parameters
    bool doArgumentsMatch(const std::vector<FuncArgSet>& params,
                          const std::vector<Type*>& args,
                          bool isExplicit = false);

    // Calculate match score for overload resolution
    // Higher score = better match, -1 = no match
    // This allows ranking multiple overloads
    int calculateMatchScore(const std::vector<FuncArgSet>& params,
                           const std::vector<Type*>& args,
                           bool isExplicit = false);
}