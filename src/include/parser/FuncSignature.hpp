/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <string>
#include <vector>
#include "./Type.hpp"

// Forward declaration
struct FuncArgSet;

// Represents a function's signature for lookup purposes
// This replaces string-based name mangling with proper type-based matching
struct FuncSignature {
    std::string baseName;           // Clean function name (no mangling)
    std::string structContext;      // Empty for free functions, struct name for methods
    std::vector<Type*> paramTypes;  // Parameter types

    FuncSignature() = default;

    FuncSignature(const std::string& name, const std::vector<Type*>& params, const std::string& structCtx = "")
        : baseName(name), structContext(structCtx), paramTypes(params) {}

    // Check if this signature matches the given argument types
    // isExplicit: if true, disable implicit conversions
    bool matches(const std::vector<Type*>& args, bool isExplicit = false) const;

    // Comparison operators for use as map key
    bool operator==(const FuncSignature& other) const;
    bool operator!=(const FuncSignature& other) const { return !(*this == other); }
    bool operator<(const FuncSignature& other) const;

    // Generate mangled name for LLVM linkage (only for code generation)
    std::string toMangledName() const;

    // Check if this is a method (has struct context)
    bool isMethod() const { return !structContext.empty(); }

    // Check if this is an operator (base name starts with '(')
    bool isOperator() const { return !baseName.empty() && baseName[0] == '('; }
};

// Helper to create signature from function arguments
FuncSignature makeSignature(const std::string& name, const std::vector<FuncArgSet>& args, const std::string& structCtx = "");