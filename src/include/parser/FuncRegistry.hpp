/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <string>
#include <vector>
#include <map>
#include "./FuncSignature.hpp"
#include "./ast.hpp"

class NodeFunc;

// Central registry for all functions, replacing string-based lookup tables
// This provides signature-based function lookup instead of name mangling
class FuncRegistry {
public:
    // Register a free function
    void registerFunc(NodeFunc* func);

    // Register a method (function belonging to a struct)
    void registerMethod(NodeFunc* func, const std::string& structName);

    // Register an operator overload
    void registerOperator(NodeFunc* func, const std::string& structName, int opType);

    // === Signature-based lookup methods (preferred) ===

    // Find function by exact signature match
    NodeFunc* findBySignature(const FuncSignature& sig) const;

    // Find best matching function for given signature
    // Uses type matching with scoring for overload resolution
    NodeFunc* findBestMatch(const FuncSignature& sig) const;

    // Check if a function with this exact signature exists
    bool hasFunction(const FuncSignature& sig) const;

    // Find operator overload by operator type and argument types
    NodeFunc* findOperator(int opType, const std::vector<Type*>& argTypes,
                           const std::string& structContext) const;

    // === Legacy string-based methods (for migration) ===

    // Find best matching function for given name and argument types
    // structContext: empty for free functions, struct name for methods
    NodeFunc* findBestMatch(const std::string& name,
                            const std::vector<Type*>& argTypes,
                            const std::string& structContext = "") const;

    // Find all overloads of a function by base name
    std::vector<NodeFunc*> findAllOverloads(const std::string& name,
                                            const std::string& structContext = "") const;

    // Check if a function with this base name exists
    bool hasBaseName(const std::string& name,
                     const std::string& structContext = "") const;

    // Get function by mangled name (for backward compatibility during migration)
    NodeFunc* getByMangledName(const std::string& mangledName) const;

    // Find function by exact signature match (legacy name)
    NodeFunc* findExact(const FuncSignature& sig) const { return findBySignature(sig); }

    // Clear all registered functions (for reinitialization)
    void clear();

    // Get singleton instance
    static FuncRegistry& instance();

private:
    FuncRegistry() = default;

    // Primary storage: base name -> vector of overloads
    std::map<std::string, std::vector<NodeFunc*>> functionsByName;

    // Method storage: struct name -> method name -> vector of overloads
    std::map<std::string, std::map<std::string, std::vector<NodeFunc*>>> methodsByStruct;

    // Operator storage: struct name -> operator type -> vector of overloads
    std::map<std::string, std::map<int, std::vector<NodeFunc*>>> operatorsByStruct;

    // Index for quick mangled name lookup (migration helper)
    std::map<std::string, NodeFunc*> mangledNameIndex;
};