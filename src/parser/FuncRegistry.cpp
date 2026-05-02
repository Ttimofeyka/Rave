/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "../include/parser/FuncRegistry.hpp"
#include "../include/parser/TypeMatching.hpp"
#include "../include/parser/nodes/NodeFunc.hpp"
#include "../include/parser/TypeUtils.hpp"

FuncRegistry& FuncRegistry::instance() {
    static FuncRegistry registry;
    return registry;
}

void FuncRegistry::registerFunc(NodeFunc* func) {
    if (func == nullptr) return;

    // Use the mangled name (with namespace) for the lookup key
    // func->name includes namespace but may have type mangling for overloads
    // We need to extract the base name (namespace + original name) for the key

    std::string baseName = func->name;

    // Remove type mangling suffix if present (e.g., "[_int,_float]")
    size_t bracketPos = baseName.find('[');
    if (bracketPos != std::string::npos) {
        baseName = baseName.substr(0, bracketPos);
    }

    // Add to functions by name
    auto& overloads = functionsByName[baseName];
    overloads.push_back(func);

    // Add to mangled name index for backward compatibility
    mangledNameIndex[func->name] = func;
}

void FuncRegistry::registerMethod(NodeFunc* func, const std::string& structName) {
    if (func == nullptr) return;

    const std::string& methodName = func->origName;

    // Add to methods by struct
    auto& structMethods = methodsByStruct[structName];
    auto& overloads = structMethods[methodName];
    overloads.push_back(func);

    // Add to mangled name index
    mangledNameIndex[func->name] = func;
}

void FuncRegistry::registerOperator(NodeFunc* func, const std::string& structName, int opType) {
    if (func == nullptr) return;

    // Add to operators by struct
    auto& structOps = operatorsByStruct[structName];
    auto& overloads = structOps[opType];
    overloads.push_back(func);

    // Add to mangled name index
    mangledNameIndex[func->name] = func;
}

NodeFunc* FuncRegistry::findBySignature(const FuncSignature& sig) const {
    if (sig.isMethod()) {
        // Search for method
        auto structIt = methodsByStruct.find(sig.structContext);
        if (structIt != methodsByStruct.end()) {
            auto methodIt = structIt->second.find(sig.baseName);
            if (methodIt != structIt->second.end()) {
                for (NodeFunc* func : methodIt->second) {
                    if (func->args.size() == sig.paramTypes.size()) {
                        bool match = true;
                        for (size_t i = 0; i < sig.paramTypes.size(); i++) {
                            if (!Types::typesEqual(func->args[i].type, sig.paramTypes[i])) {
                                match = false;
                                break;
                            }
                        }
                        if (match) return func;
                    }
                }
            }
        }
    }
    else {
        // Search for free function
        auto funcIt = functionsByName.find(sig.baseName);
        if (funcIt != functionsByName.end()) {
            for (NodeFunc* func : funcIt->second) {
                if (func->args.size() == sig.paramTypes.size()) {
                    bool match = true;
                    for (size_t i = 0; i < sig.paramTypes.size(); i++) {
                        if (func->args[i].type->toString() != sig.paramTypes[i]->toString()) {
                            match = false;
                            break;
                        }
                    }
                    if (match) return func;
                }
            }
        }
    }

    return nullptr;
}

NodeFunc* FuncRegistry::findBestMatch(const FuncSignature& sig) const {
    NodeFunc* bestMatch = nullptr;
    int bestScore = -1;

    if (sig.isMethod()) {
        // Search for method
        auto structIt = methodsByStruct.find(sig.structContext);
        if (structIt != methodsByStruct.end()) {
            // Try exact name match first
            auto methodIt = structIt->second.find(sig.baseName);
            if (methodIt != structIt->second.end()) {
                for (NodeFunc* func : methodIt->second) {
                    int score = TypeMatching::calculateMatchScore(func->args, sig.paramTypes, func->isExplicit);
                    if (score > bestScore) {
                        bestScore = score;
                        bestMatch = func;
                    }
                }
            }

            // If no match, try looking for overloaded methods
            if (bestMatch == nullptr) {
                for (const auto& [methodName, overloads] : structIt->second) {
                    // Check if method name matches (without type suffix)
                    if (methodName == sig.baseName ||
                        methodName.find(sig.baseName + "[") == 0 ||
                        methodName.find(sig.baseName + "<") == 0) {
                        for (NodeFunc* func : overloads) {
                            int score = TypeMatching::calculateMatchScore(func->args, sig.paramTypes, func->isExplicit);
                            if (score > bestScore) {
                                bestScore = score;
                                bestMatch = func;
                            }
                        }
                    }
                }
            }
        }
    }
    else {
        // Search for free function
        auto funcIt = functionsByName.find(sig.baseName);
        if (funcIt != functionsByName.end()) {
            for (NodeFunc* func : funcIt->second) {
                int score = TypeMatching::calculateMatchScore(func->args, sig.paramTypes, func->isExplicit);
                if (score > bestScore) {
                    bestScore = score;
                    bestMatch = func;
                }
            }
        }
    }

    return bestMatch;
}

bool FuncRegistry::hasFunction(const FuncSignature& sig) const {
    if (sig.isMethod()) {
        auto structIt = methodsByStruct.find(sig.structContext);
        if (structIt != methodsByStruct.end()) {
            auto methodIt = structIt->second.find(sig.baseName);
            if (methodIt != structIt->second.end()) {
                return !methodIt->second.empty();
            }
        }
        return false;
    }
    else {
        auto funcIt = functionsByName.find(sig.baseName);
        return funcIt != functionsByName.end() && !funcIt->second.empty();
    }
}

NodeFunc* FuncRegistry::findOperator(int opType, const std::vector<Type*>& argTypes,
                                     const std::string& structContext) const {
    auto structIt = operatorsByStruct.find(structContext);
    if (structIt == operatorsByStruct.end()) return nullptr;

    auto opIt = structIt->second.find(opType);
    if (opIt == structIt->second.end()) return nullptr;

    NodeFunc* bestMatch = nullptr;
    int bestScore = -1;

    for (NodeFunc* func : opIt->second) {
        int score = TypeMatching::calculateMatchScore(func->args, argTypes, func->isExplicit);
        if (score > bestScore) {
            bestScore = score;
            bestMatch = func;
        }
    }

    return bestMatch;
}

// Legacy string-based methods

NodeFunc* FuncRegistry::findBestMatch(const std::string& name,
                                      const std::vector<Type*>& argTypes,
                                      const std::string& structContext) const {
    // Create a temporary signature and use the new method
    FuncSignature sig(name, argTypes, structContext);
    return findBestMatch(sig);
}

std::vector<NodeFunc*> FuncRegistry::findAllOverloads(const std::string& name,
                                                       const std::string& structContext) const {
    std::vector<NodeFunc*> result;

    if (!structContext.empty()) {
        auto structIt = methodsByStruct.find(structContext);
        if (structIt != methodsByStruct.end()) {
            for (const auto& [methodName, overloads] : structIt->second) {
                if (methodName == name || methodName.find(name + "[") == 0 || methodName.find(name + "<") == 0) {
                    result.insert(result.end(), overloads.begin(), overloads.end());
                }
            }
        }
    }
    else {
        auto funcIt = functionsByName.find(name);
        if (funcIt != functionsByName.end()) {
            result = funcIt->second;
        }
    }

    return result;
}

bool FuncRegistry::hasBaseName(const std::string& name, const std::string& structContext) const {
    if (!structContext.empty()) {
        auto structIt = methodsByStruct.find(structContext);
        if (structIt != methodsByStruct.end()) {
            for (const auto& [methodName, overloads] : structIt->second) {
                if (methodName == name || methodName.find(name + "[") == 0 || methodName.find(name + "<") == 0) {
                    return true;
                }
            }
        }
        return false;
    }
    else {
        return functionsByName.find(name) != functionsByName.end();
    }
}

NodeFunc* FuncRegistry::getByMangledName(const std::string& mangledName) const {
    auto it = mangledNameIndex.find(mangledName);
    return it != mangledNameIndex.end() ? it->second : nullptr;
}

void FuncRegistry::clear() {
    functionsByName.clear();
    methodsByStruct.clear();
    operatorsByStruct.clear();
    mangledNameIndex.clear();
}