/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <string>

namespace TokType {
enum TokType : char {
    String,
    Char,
    Identifier,
    Number,
    FloatNumber,
    HexNumber,
    Plus,
    Minus,
    Multiply,
    Divide,
    Comma,
    Semicolon,
    Rpar,
    Lpar,
    Rbra,
    Lbra,
    Equ,
    Equal,
    Nequal,
    Ne,
    More,
    Less,
    MoreEqual,
    LessEqual,
    ShortRet,
    True,
    False,
    Eof,
    Larr,
    Rarr,
    ValSel,
    Dot,
    PluEqu,
    MinEqu,
    MulEqu,
    DivEqu,
    And,
    Or,
    Amp,
    VarArg,

    SliceOper,

    Rem,

    BitLeft,
    BitRight,
    BitXor,
    BitNot,

    Builtin,

    Destructor,

    BitOr,

    In,
    NeIn,

    None,
};

extern bool isCompoundAssigment(char type);
extern bool isParent(char type);
}

class Token {
public:
    char type;
    std::string value;
    int line = -1;

    Token(char type, std::string value) {
        this->type = type;
        this->value = value;
    }

    Token(char type) {
        this->type = type;
        this->value = "";
    }

    Token(char type, std::string value, int line) {
        this->type = type;
        this->value = value;
        this->line = line;
    }

    inline Token* copy() {
        return new Token(this->type, this->value);
    }
};

std::string tokenToString(char type);