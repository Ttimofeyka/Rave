module lexer.tokens;

// Lpar = )
// Rpar = (

enum TokType {
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

    Rem,

    BitLeft,
    BitRight,

    Builtin,
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
        return new Token(this.type,this.value);
    }
}