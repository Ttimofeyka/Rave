module lexer.tokens;

import std.string;
import std.container : Array;
import std.array;
import std.stdio;
import std.conv;
import lexer.logger;
import std.uni : isNumber;

enum TokType {
    tok_num = 0, // number
    tok_id = 1, // identifier
    tok_equ = 2, // =
    tok_lbra = 3, // [
    tok_rbra = 4, // ]
    tok_lpar = 5, // (
    tok_rpar = 6, // )
    tok_plus = 7, // +
    tok_minus = 8, // -
    tok_multiply = 9, // *
    tok_divide = 10, // /
    tok_more = 11, // >
    tok_less = 12, // <
    tok_equal = 13, // ==
    tok_nequal = 14, // !=
    tok_string = 15, // string
    tok_char = 16, // character
    tok_comma = 17, // ,
    tok_cmd = 18, // keyword
    tok_2rbra = 19, // {
    tok_2lbra = 20, // }
    tok_struct_get = 21, // ->
    tok_semicolon = 22, // ;
    tok_shortplu = 23, // +=
    tok_shortmin = 24, // -=
    tok_shortmul = 25, // *=
    tok_shortdiv = 26, // /=
    tok_and = 27, // &&
    tok_or = 28, // ||
    tok_bit_and = 29, // &
    tok_bit_or = 30, // |
    tok_bit_ls = 31, // <<
    tok_bit_rs = 32, // >>
    tok_bit_xor = 33, // ^
    tok_bit_not = 34, // ~
    tok_not = 35, // !
    tok_type = 36, // :
    tok_eof = 37, // eof
    tok_dot = 38, // .

    tok_bit_and_eq = 39, // &=
    tok_and_eq = 40, // &&=
    tok_bit_or_eq = 41, // |=
    tok_or_eq = 42, // ||=
    tok_more_more_eq = 43, // >>=
    tok_less_less_eq = 44, // <<=
    tok_min_min = 45, // --
    tok_plu_plu = 46, // ++
    tok_r_min_min = 47, // -- on the right (unused by lexer)
    tok_r_plu_plu = 48, // ++ on the right (unused by lexer)
    tok_arrow = 49, // =>
    tok_hash = 50, // #
    tok_at = 51, // @
    tok_doc = 52, // documentation comment
    tok_question = 53, // ?
}

enum TokCmd {
    cmd_if = 0,
    cmd_else = 1,
    cmd_while = 2,
    cmd_do = 3,
    cmd_include = 4,
    cmd_extern = 5,
    cmd_asm = 11,
    cmd_struct = 12,
    cmd_ret = 14,
    cmd_break = 15,
    cmd_continue = 16,
    cmd_sizeof = 17,
    cmd_void = 19,
    cmd_enum = 20,
    cmd_static = 21,
    cmd_const = 22,
    // Preprocessor
    cmd_define = 13,
    cmd_ifdef = 23,
    cmd_ifndef = 24,
    cmd_end = 25,
    cmd_undefine = 26,
    cmd_protected = 27,
    cmd_error = 28,
    cmd_warn = 29,
    cmd_out = 30,
    cmd_ifequ = 31,
    cmd_exit = 32
}

struct SourceLocation {
    uint line;
    uint col;
    string fname;

    string toString() const {
        return fname ~ ":" ~ to!string(line + 1) ~ ":" ~ to!string(col);
    }
}

class Token {
    TokType type;
    TokCmd cmd;
    string value;
    SourceLocation loc;

    this(SourceLocation loc, string s) {
        this.loc = loc;
        this.value = s;
        if(s.length == 0) {
            this.type = TokType.tok_eof;
            return;
        }

        if(s[0]=='"') {
            this.type = TokType.tok_string;
        }
        else if(s[0]=='\'') {
            this.type = TokType.tok_char;
        }
        else if(s.length >= 2 && s[0..2] == "/*") {
            this.type = TokType.tok_doc;
        }
        else if(isNumber(s[0])) {
            this.type = TokType.tok_num;
        }
        else {
            switch(s) {
                // Operators
                case "(": this.type = TokType.tok_lpar; break;
                case ")": this.type = TokType.tok_rpar; break;
                case "[": this.type = TokType.tok_lbra; break;
                case "]": this.type = TokType.tok_rbra; break;
                case "{": this.type = TokType.tok_2lbra; break;
                case "}": this.type = TokType.tok_2rbra; break;
                case ",": this.type = TokType.tok_comma; break;
                case "->": this.type = TokType.tok_struct_get; break;
                case ";": this.type = TokType.tok_semicolon; break;
                case "*=": this.type = TokType.tok_shortmul; break;
                case "-=": this.type = TokType.tok_shortmin; break;
                case "/=": this.type = TokType.tok_shortdiv; break;
                case "+=": this.type = TokType.tok_shortplu; break;
                case "<<": this.type = TokType.tok_bit_ls; break;
                case ">>": this.type = TokType.tok_bit_rs; break;
                case "&&": this.type = TokType.tok_and; break;
                case "||": this.type = TokType.tok_or; break;
                case "|": this.type = TokType.tok_bit_or; break;
                case "&": this.type = TokType.tok_bit_and; break;
                case "^": this.type = TokType.tok_bit_xor; break;
                case "~": this.type = TokType.tok_bit_not; break;
                case "!": this.type = TokType.tok_not; break;
                case "!=": this.type = TokType.tok_nequal; break;
                case "==": this.type = TokType.tok_equal; break;
                case "=": this.type = TokType.tok_equ; break;
                case "+":  this.type = TokType.tok_plus; break;
                case "-":  this.type = TokType.tok_minus; break;
                case "*" :this.type = TokType.tok_multiply; break;
                case "/":  this.type = TokType.tok_divide; break;
                case ":":  this.type = TokType.tok_type; break;
                case ".": this.type = TokType.tok_dot; break;
                case "@": this.type = TokType.tok_at; break;
                case "&=": this.type = TokType.tok_bit_and_eq; break;
                case "&&=": this.type = TokType.tok_and_eq; break;
                case "|=": this.type = TokType.tok_bit_or_eq; break;
                case "||=": this.type = TokType.tok_or_eq; break;
                case "++": this.type = TokType.tok_plu_plu; break;
                case "--": this.type = TokType.tok_min_min; break;
                case "<": this.type = TokType.tok_less; break;
                case ">": this.type = TokType.tok_more; break;
                case ">>=": this.type = TokType.tok_more_more_eq; break;
                case "<<=": this.type = TokType.tok_less_less_eq; break;
                case "=>": this.type = TokType.tok_arrow; break;
                case "#": this.type = TokType.tok_hash; break;
                case "?": this.type = TokType.tok_question; break;
                case "if": this.type=TokType.tok_cmd; this.cmd=TokCmd.cmd_if; break;
                case "else": this.type=TokType.tok_cmd; this.cmd=TokCmd.cmd_else; break;
                case "while": this.type=TokType.tok_cmd; this.cmd=TokCmd.cmd_while; break;
                case "do": this.type=TokType.tok_cmd; this.cmd=TokCmd.cmd_do; break;
                case "extern": this.type=TokType.tok_cmd; this.cmd=TokCmd.cmd_extern; break;
                case "asm": this.type=TokType.tok_cmd; this.cmd=TokCmd.cmd_asm; break;
                case "struct": this.type=TokType.tok_cmd; this.cmd=TokCmd.cmd_struct; break;
                case "ret": this.type=TokType.tok_cmd; this.cmd=TokCmd.cmd_ret; break;
                case "break": this.type=TokType.tok_cmd; this.cmd=TokCmd.cmd_break; break;
                case "continue": this.type=TokType.tok_cmd; this.cmd=TokCmd.cmd_continue; break;
                case "enum": this.type=TokType.tok_cmd; this.cmd=TokCmd.cmd_enum; break;
                case "static": this.type=TokType.tok_cmd; this.cmd=TokCmd.cmd_static; break;
                case "const": this.type=TokType.tok_cmd; this.cmd=TokCmd.cmd_const; break;
                // Preprocessor commands
                case "@else": this.type=TokType.tok_cmd; this.cmd=TokCmd.cmd_else; break;
                case "@inc": this.type=TokType.tok_cmd; this.cmd=TokCmd.cmd_include; break;
                case "@def": this.type=TokType.tok_cmd; this.cmd=TokCmd.cmd_define; break;
                case "@ifdef": this.type=TokType.tok_cmd; this.cmd=TokCmd.cmd_ifdef; break;
                case "@ifndef": this.type=TokType.tok_cmd; this.cmd=TokCmd.cmd_ifndef; break;
                case "@end": this.type=TokType.tok_cmd; this.cmd=TokCmd.cmd_end; break;
                case "@undef": this.type=TokType.tok_cmd; this.cmd=TokCmd.cmd_undefine; break;
                case "@once": this.type=TokType.tok_cmd; this.cmd=TokCmd.cmd_protected; break;
                case "@err": this.type=TokType.tok_cmd; this.cmd=TokCmd.cmd_error; break;
                case "@warn": this.type=TokType.tok_cmd; this.cmd=TokCmd.cmd_warn; break;
                case "@out": this.type=TokType.tok_cmd; this.cmd=TokCmd.cmd_out; break;
                case "@exit": this.type=TokType.tok_cmd; this.cmd=TokCmd.cmd_ifequ; break;
                // Identifier
                default: this.type = TokType.tok_id; break;
            }
        }
    }

    this(SourceLocation loc, TokType type, TokCmd cmd, string value) {
        this.loc = loc;
        this.type = type;
        this.cmd = cmd;
        this.value = value;
    }
}

class TList {
    public Token[] tokens;

    public this() {
        this.tokens = [];
    }

    public Token opIndex(int index) {
        return tokens[index];
    }

    public void insertBack(Token token) {
        tokens ~= token;
    }

    public int length() const { return cast(int)tokens.length; }

    public void debugPrint() const {
        foreach(tok; tokens) {
            if(tok.type == TokType.tok_cmd) {
                writeln("Type: ", to!string(tok.cmd), " ", tok.value);
            }
            else {
                writeln("Type: ", to!string(tok.type), " ", tok.value);
            }
        }
    }
}
