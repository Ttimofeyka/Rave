/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "../include/parser/TypeMatching.hpp"
#include "../include/parser/Types.hpp"
#include "../include/parser/nodes/NodeStruct.hpp"
#include "../include/parser/nodes/NodeFunc.hpp"
#include "../include/parser/ast.hpp"
#include "../include/lexer/tokens.hpp"

namespace TypeMatching {

bool areTypesCompatible(Type* param, Type* arg, bool isExplicit) {
    if (param == nullptr || arg == nullptr) return false;

    // Handle const types
    Type* t1 = Types::stripConst(arg);
    Type* t2 = Types::stripConst(param);

    // Handle struct with implicit conversion operator
    if (instanceof<TypeStruct>(t2) && !instanceof<TypeStruct>(t1)) {
        if (isExplicit) return false;

        std::string name = t1->toString();
        if (AST::structTable.find(name) != AST::structTable.end()) {
            auto& operators = AST::structTable[name]->operators;
            if (operators.find(TokType::Equ) != operators.end()) {
                Type* ty = operators[TokType::Equ].begin()->second->args[1].type;
                        if ((::isBytePointer(ty) && ::isBytePointer(t1)) || Types::typesEqual(ty, t1)) {
                    return true;
                }
            }
        }
    }

    // Handle basic types with float distinction
    if (instanceof<TypeBasic>(t1) && instanceof<TypeBasic>(t2)) {
        auto* tb1 = (TypeBasic*)t1;
        auto* tb2 = (TypeBasic*)t2;
        if (tb1->type != tb2->type && (::isFloatType(tb1) || ::isFloatType(tb2))) {
            return false;
        }
        return true;
    }

    // Handle struct/pointer conversion
    if (!Types::typesEqual(t1, t2)) {
        if (instanceof<TypeStruct>(t1) && instanceof<TypePointer>(t2)) return true;
        if (instanceof<TypeStruct>(t2) && instanceof<TypePointer>(t1)) return true;
        return false;
    }

    return true;
}

bool doArgumentsMatch(const std::vector<FuncArgSet>& params,
                      const std::vector<Type*>& args,
                      bool isExplicit) {
    if (params.size() != args.size()) return false;

    for (size_t i = 0; i < args.size(); i++) {
        Type* t1 = args[i];
        Type* t2 = params[i].type;

        if (!areTypesCompatible(t2, t1, isExplicit)) {
            // Additional check for struct with implicit conversion
            if (instanceof<TypeStruct>(t2) && !instanceof<TypeStruct>(t1) && !isExplicit) {
                std::string name = t1->toString();
                if (AST::structTable.find(name) != AST::structTable.end()) {
                    auto& operators = AST::structTable[name]->operators;
                    if (operators.find(TokType::Equ) != operators.end()) {
                        Type* ty = operators[TokType::Equ].begin()->second->args[1].type;
                if ((::isBytePointer(ty) && ::isBytePointer(t1)) || Types::typesEqual(ty, t1)) {
                            continue;
                        }
                    }
                }
            }
            return false;
        }
    }
    return true;
}

int calculateMatchScore(const std::vector<FuncArgSet>& params,
                        const std::vector<Type*>& args,
                        bool isExplicit) {
    if (params.size() != args.size()) return -1;

    int score = 0;

    for (size_t i = 0; i < args.size(); i++) {
        Type* t1 = args[i];
        Type* t2 = params[i].type;

        if (t1 == nullptr || t2 == nullptr) return -1;

        // Exact match gives highest score
        if (Types::typesEqual(t1, t2)) {
            score += 10;
            continue;
        }

        // Handle const stripping
        Type* tc1 = Types::stripConst(t1);
        Type* tc2 = Types::stripConst(t2);

        if (Types::typesEqual(tc1, tc2)) {
            score += 8;
            continue;
        }

        // Handle struct/pointer conversion (lower score)
        if (instanceof<TypeStruct>(tc1) && instanceof<TypePointer>(tc2)) {
            score += 5;
            continue;
        }
        if (instanceof<TypeStruct>(tc2) && instanceof<TypePointer>(tc1)) {
            score += 5;
            continue;
        }

        // Basic type compatibility
        if (instanceof<TypeBasic>(tc1) && instanceof<TypeBasic>(tc2)) {
            auto* tb1 = (TypeBasic*)tc1;
            auto* tb2 = (TypeBasic*)tc2;
            // Float/int mixing is not allowed
            if (tb1->type != tb2->type && (::isFloatType(tb1) || ::isFloatType(tb2))) {
                return -1;
            }
            score += 3;
            continue;
        }

        // Implicit conversion via operator
        if (instanceof<TypeStruct>(tc2) && !instanceof<TypeStruct>(tc1) && !isExplicit) {
            std::string name = tc1->toString();
            if (AST::structTable.find(name) != AST::structTable.end()) {
                auto& operators = AST::structTable[name]->operators;
                if (operators.find(TokType::Equ) != operators.end()) {
                    score += 2;
                    continue;
                }
            }
        }

        return -1; // No match
    }

    return score;
}

} // namespace TypeMatching