/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

module lexer.tokens;

// Lpar = )
// Rpar = (

enum TokType : char {
    String,
    Char,
    Identifier,
    Number,
    FloatNumber,
    HexNumber,
    Command,
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
    Null,
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
    MacroArgNum,
    GetPtr,
    VarArg,

    SliceOper,
    SlicePtrOper,

    Rem,

    BitLeft,
    BitRight,
    BitXor,
    BitNot,

    Builtin,

    Destructor,

    BitOr,
    BitAnd,

    None,
}

class Token {
    TokType type;
    string value;
    int line = -1;

    this(TokType type, string value) {
        this.type = type;
        this.value = value;
    }

    this(TokType type) {
        this.type = type;
        this.value = "";
    }

    this(TokType type, string value, int line) {
        this.type = type;
        this.value = value;
        this.line = line;
    }

    Token copy() {
        pragma(inline,true);
        return new Token(this.type,this.value);
    }
}