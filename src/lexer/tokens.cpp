/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "../include/lexer/tokens.hpp"

bool TokType::isCompoundAssigment(char type) {
    return type == TokType::PluEqu || type == TokType::MinEqu || type == TokType::MulEqu || type == TokType::DivEqu;
}

bool TokType::isParent(char type) {
    return type == TokType::Lpar || type == TokType::Rpar;
}