/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "../include/parser/FuncSignature.hpp"
#include "../include/parser/TypeMatching.hpp"
#include "../include/parser/TypeUtils.hpp"

bool FuncSignature::matches(const std::vector<Type*>& args, bool isExplicit) const {
    if (paramTypes.size() != args.size()) return false;

    for (size_t i = 0; i < paramTypes.size(); i++) {
        if (!TypeMatching::areTypesCompatible(paramTypes[i], args[i], isExplicit)) {
            return false;
        }
    }
    return true;
}

bool FuncSignature::operator==(const FuncSignature& other) const {
    if (baseName != other.baseName) return false;
    if (structContext != other.structContext) return false;
    if (paramTypes.size() != other.paramTypes.size()) return false;

    for (size_t i = 0; i < paramTypes.size(); i++) {
        if (paramTypes[i]->toString() != other.paramTypes[i]->toString()) {
            return false;
        }
    }
    return true;
}

bool FuncSignature::operator<(const FuncSignature& other) const {
    if (baseName != other.baseName) return baseName < other.baseName;
    if (structContext != other.structContext) return structContext < other.structContext;
    if (paramTypes.size() != other.paramTypes.size()) return paramTypes.size() < other.paramTypes.size();

    for (size_t i = 0; i < paramTypes.size(); i++) {
        if (paramTypes[i]->toString() != other.paramTypes[i]->toString()) {
            return paramTypes[i]->toString() < other.paramTypes[i]->toString();
        }
    }
    return false;
}

std::string FuncSignature::toMangledName() const {
    std::string result;

    // Add struct context for methods
    if (!structContext.empty()) {
        result = "{" + structContext + "}";
    }

    result += baseName;

    // Add type signature
    if (!paramTypes.empty()) {
        result += "[";
        for (size_t i = 0; i < paramTypes.size(); i++) {
            result += "_" + typeToString(paramTypes[i]);
        }
        result += "]";
    }

    return result;
}

FuncSignature makeSignature(const std::string& name, const std::vector<FuncArgSet>& args, const std::string& structCtx) {
    std::vector<Type*> types;
    for (const auto& arg : args) {
        types.push_back(arg.type);
    }
    return FuncSignature(name, types, structCtx);
}