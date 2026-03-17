/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "../include/parser/ParserUtils.hpp"
#include <algorithm>

bool isOperator(char tokType) {
    static const std::vector<char> ops = {
        TokType::Multiply, TokType::Divide, TokType::Plus, TokType::Minus,
        TokType::Rem, TokType::BitLeft, TokType::BitRight, TokType::BitXor,
        TokType::BitOr, TokType::Amp, TokType::Less, TokType::More,
        TokType::LessEqual, TokType::MoreEqual, TokType::Equal, TokType::Nequal,
        TokType::In, TokType::NeIn, TokType::Or, TokType::And,
        TokType::Equ, TokType::PluEqu, TokType::MinEqu, TokType::MulEqu, TokType::DivEqu
    };
    return std::find(ops.begin(), ops.end(), tokType) != ops.end();
}
