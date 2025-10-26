/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "../../include/parser/ast.hpp"
#include "../../include/parser/Types.hpp"
#include <llvm-c/Target.h>
#include "../../include/parser/nodes/NodeBinary.hpp"
#include "../../include/lexer/tokens.hpp"
#include "../../include/parser/nodes/NodeIden.hpp"
#include "../../include/parser/nodes/NodeInt.hpp"
#include "../../include/parser/nodes/NodeFloat.hpp"
#include "../../include/parser/nodes/NodeString.hpp"
#include "../../include/parser/nodes/NodeNull.hpp"
#include "../../include/parser/nodes/NodeBool.hpp"
#include "../../include/parser/nodes/NodeDone.hpp"
#include "../../include/parser/nodes/NodeType.hpp"
#include "../../include/llvm.hpp"
#include "../../include/compiler.hpp"
#include <iostream>

NodeBinary::NodeBinary(char op, Node* first, Node* second, int loc, bool isStatic) {
    this->op = op;
    this->first = first;
    this->second = second;
    this->loc = loc;
    this->isStatic = isStatic;
}

Node* NodeBinary::copy() {return new NodeBinary(this->op, first->copy(), second->copy(), loc, this->isStatic);}
void NodeBinary::check() {isChecked = true;}

Node* NodeBinary::comptime() {
    Node* first = this->first;
    Node* second = this->second;

    while (instanceof<NodeIden>(first) && AST::aliasTable.find(((NodeIden*)first)->name) != AST::aliasTable.end()) first = AST::aliasTable[((NodeIden*)first)->name];
    while (instanceof<NodeIden>(second) && AST::aliasTable.find(((NodeIden*)second)->name) != AST::aliasTable.end()) second = AST::aliasTable[((NodeIden*)second)->name];
    
    while (!instanceof<NodeType>(first) && !instanceof<NodeBool>(first) && !instanceof<NodeString>(first) && !instanceof<NodeIden>(first) && !instanceof<NodeInt>(first) && !instanceof<NodeFloat>(first)) first = first->comptime();
    while (!instanceof<NodeType>(second) && !instanceof<NodeBool>(second) && !instanceof<NodeString>(second) && !instanceof<NodeIden>(second) && !instanceof<NodeInt>(second) && !instanceof<NodeFloat>(second)) second = second->comptime();

    if ((instanceof<NodeIden>(first) || instanceof<NodeType>(first)) && (instanceof<NodeIden>(second) || instanceof<NodeType>(second))) {
        Type* firstType = (instanceof<NodeIden>(first) ? new TypeStruct(((NodeIden*)first)->name) : ((NodeType*)first)->type);
        Type* secondType = (instanceof<NodeIden>(second) ? new TypeStruct(((NodeIden*)second)->name) : ((NodeType*)second)->type);

        Types::replaceTemplates(&firstType);
        Types::replaceTemplates(&secondType);

        switch (this->op) {
            case TokType::Equal: return new NodeBool(firstType->toString() == secondType->toString());
            case TokType::Nequal: return new NodeBool(firstType->toString() != secondType->toString());
            default: return new NodeBool(false);
        }
    }

    NodeBool* eqNeqResult = nullptr;

    switch (this->op) {
        case TokType::Equal: case TokType::Nequal:
            eqNeqResult = new NodeBool(true);

            if (instanceof<NodeString>(first) && instanceof<NodeString>(second)) eqNeqResult->value = ((NodeString*)first)->value == ((NodeString*)second)->value;
            else if (instanceof<NodeBool>(first)) {
                if (instanceof<NodeBool>(second)) eqNeqResult->value = ((NodeBool*)first)->value == ((NodeBool*)second)->value;
                else if (instanceof<NodeInt>(second)) eqNeqResult->value = ((NodeInt*)second)->value == (((NodeBool*)first)->value ? "1" : "0");
                else if (instanceof<NodeFloat>(second)) {
                    std::string _float = ((NodeFloat*)second)->value;
                    int dot = _float.find('.');

                    if (dot != std::string::npos) {
                        while (_float.back() == '0') _float.pop_back();
                        if (_float.back() == '.') _float.pop_back();
                    }

                    eqNeqResult->value = _float == (((NodeBool*)first)->value ? "1" : "0");
                }
            }
            else if (instanceof<NodeInt>(first)) {
                if (instanceof<NodeInt>(second)) eqNeqResult->value = ((NodeInt*)first)->value == ((NodeInt*)second)->value;
                else if (instanceof<NodeBool>(second)) eqNeqResult->value = ((NodeInt*)first)->value == (((NodeBool*)second)->value ? "1" : "0");
                else if (instanceof<NodeFloat>(second)) {
                    std::string _float = ((NodeFloat*)second)->value;
                    int dot = _float.find('.');

                    if (dot != std::string::npos) {
                        while (_float.back() == '0') _float.pop_back();
                        if (_float.back() == '.') _float.pop_back();
                    }

                    eqNeqResult->value = _float == ((NodeInt*)first)->value.to_string();
                }
            }
            else if (instanceof<NodeFloat>(first)) {
                std::string _float = ((NodeFloat*)first)->value;
                int dot = _float.find('.');
                if (dot != std::string::npos) {
                    while (_float.back() == '0') _float.pop_back();
                    if (_float.back() == '.') _float.pop_back();
                }
                
                if (instanceof<NodeFloat>(second)) {
                    std::string _float2 = ((NodeFloat*)second)->value;
                    int dot2 = _float2.find('.');
                    if (dot2 != std::string::npos) {
                        while (_float2.back() == '0') _float2.pop_back();
                        if (_float2.back() == '.') _float2.pop_back();
                    }
                    eqNeqResult->value = _float == _float2;
                }
                else if (instanceof<NodeInt>(second)) eqNeqResult->value = _float == ((NodeInt*)second)->value.to_string();
                else if (instanceof<NodeBool>(second)) eqNeqResult->value = _float == (((NodeBool*)second)->value ? "1" : "0");
            }
            
            if (this->op == TokType::Nequal) eqNeqResult->value = !eqNeqResult->value;

            return eqNeqResult;

        // TODO: implicit cast of basic types to bool for And & Or
        case TokType::And: return new NodeBool(((NodeBool*)first->comptime())->value && ((NodeBool*)second->comptime())->value);
        case TokType::Or: return new NodeBool(((NodeBool*)first->comptime())->value || ((NodeBool*)second->comptime())->value);

        // TODO: bitwise compile-time operators, not only for bool
        case TokType::Amp: return new NodeBool(((NodeBool*)first->comptime())->value && ((NodeBool*)second->comptime())->value);
        case TokType::BitOr: return new NodeBool(((NodeBool*)first->comptime())->value || ((NodeBool*)second->comptime())->value);

        case TokType::More: case TokType::Less: case TokType::MoreEqual: case TokType::LessEqual:
            if (instanceof<NodeInt>(first)) {
                if (instanceof<NodeInt>(second)) {
                    if (this->op == TokType::More) return new NodeBool(((NodeInt*)first)->value > ((NodeInt*)second)->value);
                    else if (this->op == TokType::Less) return new NodeBool(((NodeInt*)first)->value < ((NodeInt*)second)->value);
                    else if (this->op == TokType::MoreEqual) return new NodeBool(((NodeInt*)first)->value >= ((NodeInt*)second)->value);
                    else return new NodeBool(((NodeInt*)first)->value <= ((NodeInt*)second)->value);
                }
                else if (instanceof<NodeFloat>(second)) {
                    if (((NodeInt*)first)->type == BasicType::Cent || ((NodeInt*)first)->type == BasicType::Ucent) return new NodeBool(false); // TODO: is this a error?
                    R128 r128;
                    r128FromString(&r128, ((NodeFloat*)second)->value.c_str(), nullptr);
                    if (this->op == TokType::More) return new NodeBool(r128 > R128(((NodeInt*)first)->value.to_long_long()));
                    else if (this->op == TokType::Less) return new NodeBool(r128 < R128(((NodeInt*)first)->value.to_long_long()));
                    else if (this->op == TokType::MoreEqual) return new NodeBool(r128 >= R128(((NodeInt*)first)->value.to_long_long()));
                    else return new NodeBool(r128 <= R128(((NodeInt*)first)->value.to_long_long()));
                }
                else if (instanceof<NodeBool>(second)) return new NodeBool(((NodeInt*)first)->value > (((NodeBool*)second)->value ? 1 : 0));
                else return new NodeBool(false);
            }
            else if (instanceof<NodeFloat>(first)) {
                if (instanceof<NodeFloat>(second)) {
                    R128 r128_1, r128_2;
                    r128FromString(&r128_1, ((NodeFloat*)first)->value.c_str(), nullptr);
                    r128FromString(&r128_2, ((NodeFloat*)first)->value.c_str(), nullptr);
                    if (this->op == TokType::More) return new NodeBool(r128_1 > r128_2);
                    else if (this->op == TokType::Less) return new NodeBool(r128_1 < r128_2);
                    else if (this->op == TokType::MoreEqual) return new NodeBool(r128_1 >= r128_2);
                    else return new NodeBool(r128_1 <= r128_2);
                }
                else if (instanceof<NodeInt>(second)) {
                    if (((NodeInt*)second)->type == BasicType::Cent || ((NodeInt*)second)->type == BasicType::Ucent) return new NodeBool(false); // TODO: is this a error?
                    R128 r128;
                    r128FromString(&r128, ((NodeFloat*)first)->value.c_str(), nullptr);
                    if (this->op == TokType::More) return new NodeBool(r128 > R128(((NodeInt*)second)->value.to_long_long()));
                    else if (this->op == TokType::Less) return new NodeBool(r128 < R128(((NodeInt*)second)->value.to_long_long()));
                    else if (this->op == TokType::MoreEqual) return new NodeBool(r128 >= R128(((NodeInt*)second)->value.to_long_long()));
                    else return new NodeBool(r128 <= R128(((NodeInt*)second)->value.to_long_long()));
                }
                // TODO: add bool
            }
            return new NodeBool(false);
        case TokType::Plus:
        case TokType::Minus:
        case TokType::Multiply:
        case TokType::Divide:
            if (instanceof<NodeInt>(first)) {
                if (instanceof<NodeInt>(second)) return new NodeInt(((NodeInt*)first)->value + ((NodeInt*)second)->value);
                else if (instanceof<NodeFloat>(second)) {
                    R128 r128_1;
                    r128FromInt(&r128_1, ((NodeInt*)first)->value.to_long_long());

                    R128 r128_2;
                    r128FromString(&r128_2, ((NodeFloat*)second)->value.c_str(), nullptr);

                    R128 r128_result;
                    switch (op) {
                        case TokType::Plus: r128_result = r128_1 + r128_2; break;
                        case TokType::Minus: r128_result = r128_1 - r128_2; break;
                        case TokType::Multiply: r128_result = r128_1 * r128_2; break;
                        case TokType::Divide: r128_result = r128_1 / r128_2; break;
                        default: break;
                    }

                    char buffer[48];
                    r128ToString(buffer, 48, &r128_result);

                    return new NodeFloat(std::string(buffer));
                }
            }
            else if (instanceof<NodeFloat>(first)) {
                R128 r128_1;
                r128FromString(&r128_1, ((NodeFloat*)second)->value.c_str(), nullptr);

                R128 r128_2;
                if (instanceof<NodeInt>(second)) r128FromInt(&r128_2, ((NodeInt*)second)->value.to_long_long());
                else if (instanceof<NodeFloat>(second)) r128FromString(&r128_2, ((NodeFloat*)second)->value.c_str(), nullptr);

                R128 r128_result;
                switch (op) {
                    case TokType::Plus: r128_result = r128_1 + r128_2; break;
                    case TokType::Minus: r128_result = r128_1 - r128_2; break;
                    case TokType::Multiply: r128_result = r128_1 * r128_2; break;
                    case TokType::Divide: r128_result = r128_1 / r128_2; break;
                    default: break;
                }

                char buffer[48];
                r128ToString(buffer, 48, &r128_result);

                return new NodeFloat(std::string(buffer));
            }

            return new NodeBool(false);
        default: return new NodeBool(false);
    }
}

Type* NodeBinary::getType() {
    switch (this->op) {
        case TokType::Equ: case TokType::PluEqu: case TokType::MinEqu: case TokType::DivEqu: case TokType::MulEqu: return typeVoid;
        case TokType::Equal: case TokType::Nequal: case TokType::More: case TokType::Less: case TokType::MoreEqual: case TokType::LessEqual:
            if (instanceof<TypeVector>(first->getType())) return first->getType();
            return basicTypes[BasicType::Bool];
        case TokType::And: case TokType::Or: case TokType::In: case TokType::NeIn: return basicTypes[BasicType::Bool];
        default:
            Type* firstType = first->getType();
            Type* secondType = second->getType();
            if (firstType == nullptr) return secondType;
            if (secondType == nullptr) return firstType;
            return (firstType->getSize() >= secondType->getSize()) ? firstType : secondType;
    }
    return nullptr;
}


RaveValue NodeBinary::generate() {
    return Binary::operation(op, first, second, loc);
}

NodeBinary::~NodeBinary() {
    if (first != nullptr) delete first;
    if (second != nullptr) delete second;
}
