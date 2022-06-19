module lexer.tokens;

import std.string;
import std.container : Array;
import std.array;
import std.stdio;
import std.conv;
import lexer.logger;
import std.uni : isNumber;

enum TokType {
    tok_num, // number
    tok_id, // identifier
    tok_equ, // =
    tok_lbra, // [
    tok_rbra, // ]
    tok_lpar, // (
    tok_rpar, // )
    tok_plus, // +
    tok_minus, // -
    tok_multiply, // *
    tok_divide, // /
    tok_more, // >
    tok_less, // <
    tok_equal, // ==
    tok_nequal, // !=
    tok_string, // string
    tok_char, // character
    tok_comma, // ,
    tok_cmd, // keyword
    tok_2rbra, // {
    tok_2lbra, // }
    tok_struct_get, // ->
    tok_semicolon, // ;
    tok_shortplu, // +=
    tok_shortmin, // -=
    tok_shortmul, // *=
    tok_shortdiv, // /=
    tok_and, // &&
    tok_or, // ||
    tok_bit_and, // &
    tok_bit_or, // |
    tok_bit_ls, // <<
    tok_bit_rs, // >>
    tok_bit_xor, // ^
    tok_bit_not, // ~
    tok_not, // !
    tok_type, // :
    tok_eof, // eof
    tok_dot, // .
    tok_more_equal, // >=
    tok_less_equal, // <=

    tok_bit_and_eq, // &=
    tok_and_eq, // &&=
    tok_bit_or_eq, // |=
    tok_or_eq, // ||=
    tok_more_more_eq, // >>=
    tok_less_less_eq, // <<=
    tok_min_min, // --
    tok_plu_plu, // ++
    tok_r_min_min, // -- on the right (unused by lexer)
    tok_r_plu_plu, // ++ on the right (unused by lexer)
    tok_arrow, // =>
    tok_hash, // #
    tok_at, // @
    tok_doc, // documentation comment
    tok_question, // ?
    tok_namesp_get, // ::
    tok_multiargs, // ..
    tok_true,
    tok_false,
    tok_proc, // %
}

enum TokCmd {
    cmd_if,
    cmd_else,
    cmd_while,
    cmd_do,
    cmd_include,
    cmd_extern,
    cmd_asm,
    cmd_struct,
    cmd_ret,
    cmd_break,
    cmd_continue,
    cmd_sizeof,
    cmd_void,
    cmd_enum,
    cmd_static,
    cmd_const,
    // Preprocessor
    cmd_define,
    cmd_ifdef,
    cmd_ifndef,
    cmd_end,
    cmd_undefine,
    cmd_protected,
    cmd_error,
    cmd_warn,
    cmd_out,
    cmd_ifequ,
    cmd_exit,
    cmd_namespace,
    cmd_using,
    cmd_import,
    cmd_insert,
    cmd_macro,
    cmd_endm,
    cmd_nexttok_str,
    cmd_nexttok_unstr,
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

    this(SourceLocation loc, string s, TokType t) {
        this.type = t;
        this.value = s;
        this.loc = loc;
    }

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
                case "::": this.type = TokType.tok_namesp_get; break;
                case "<=": this.type = TokType.tok_less_equal; break;
                case ">=": this.type = TokType.tok_more_equal; break;
                case "true": this.type = TokType.tok_true; break;
                case "false": this.type = TokType.tok_false; break;
                // Commands
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
                case "namespace": this.type=TokType.tok_cmd; this.cmd=TokCmd.cmd_namespace; break;
                case "using": this.type=TokType.tok_cmd; this.cmd=TokCmd.cmd_using; break;
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
                case "@imp": this.type=TokType.tok_cmd; this.cmd=TokCmd.cmd_import; break;
                case "@ins": this.type=TokType.tok_cmd; this.cmd=TokCmd.cmd_insert; break;
                case "@macro": this.type=TokType.tok_cmd; this.cmd=TokCmd.cmd_macro; break;
                case "@endm": this.type=TokType.tok_cmd; this.cmd=TokCmd.cmd_endm; break;
                case "@strify": this.type=TokType.tok_cmd; this.cmd=TokCmd.cmd_nexttok_str; break;
                case "@unstrify": this.type=TokType.tok_cmd; this.cmd=TokCmd.cmd_nexttok_unstr; break;
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
        pragma(inline,true);
        return tokens[index];
    }

    public void insertBack(Token token) {
        pragma(inline,true);
        tokens ~= token;
    }

    public void insertBack(TList t) {
        pragma(inline,true);
        tokens ~= t.tokens.dup;
    }

    public int length() const { pragma(inline,true); return cast(int)tokens.length; }

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

    public override string toString()
    {
        string toret = "";
        for(int z=0; z<tokens.length; z++) {
            toret ~= tokens[z].value ~ " ";
        }
        return strip(toret);
    }
}
