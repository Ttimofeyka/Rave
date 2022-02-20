import std.string;
import std.container : Array;
import std.array;
import std.stdio;
import std.conv;
import logger;
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
    cmd_define = 13,
    cmd_ret = 14,
    cmd_break = 15,
    cmd_continue = 16,
    cmd_sizeof = 17,
    cmd_void = 19,
    cmd_enum = 20,
    cmd_static = 21,
    cmd_const = 22,
    // Preprocessor IF
    cmd_ifdef = 23,
    cmd_ifndef = 24,
    cmd_endif = 25,
}

string tokTypeToStr(TokType type)
{
	switch(type)
	{
	case TokType.tok_num: return "num";
	case TokType.tok_id: return "id";
	case TokType.tok_equ: return "equ";
	case TokType.tok_lbra: return "lbra";
	case TokType.tok_rbra: return "rbra";
	case TokType.tok_lpar: return "lpar";
	case TokType.tok_rpar: return "rpar";
	case TokType.tok_plus: return "plus";
	case TokType.tok_minus: return "minus";
	case TokType.tok_multiply: return "multiply";
	case TokType.tok_divide: return "divide";
	case TokType.tok_more: return "more";
	case TokType.tok_less: return "less";
	case TokType.tok_equal: return "equal";
	case TokType.tok_nequal: return "nequal";
	case TokType.tok_string: return "string";
	case TokType.tok_char: return "char";
	case TokType.tok_comma: return "comma";
	case TokType.tok_cmd: return "cmd";
	case TokType.tok_2rbra: return "2rbra";
	case TokType.tok_2lbra: return "2lbra";
	case TokType.tok_struct_get: return "struct_get";
	case TokType.tok_semicolon: return "semicolon";
	case TokType.tok_shortplu: return "shortplu";
	case TokType.tok_shortmin: return "shortmin";
	case TokType.tok_shortmul: return "shortmul";
	case TokType.tok_shortdiv: return "shortdiv";
	case TokType.tok_and: return "and";
	case TokType.tok_or: return "or";
	case TokType.tok_bit_and: return "bit_and";
	case TokType.tok_bit_or: return "bit_or";
	case TokType.tok_bit_ls: return "bit_ls";
	case TokType.tok_bit_rs: return "bit_rs";
	case TokType.tok_bit_xor: return "bit_xor";
	case TokType.tok_bit_not: return "bit_not";
	case TokType.tok_not: return "not";
    case TokType.tok_type: return "type";
    case TokType.tok_eof: return "eof";
    case TokType.tok_dot: return "dot";
    case TokType.tok_min_min: return "min_min";
    case TokType.tok_plu_plu: return "plu_plu";
    case TokType.tok_bit_and_eq: return "bit_and_eq";
    case TokType.tok_bit_or_eq: return "bit_or_eq";
    case TokType.tok_and_eq: return "and_eq";
    case TokType.tok_or_eq: return "or_eq";
    case TokType.tok_more_more_eq: return "more_more_eq";
    case TokType.tok_less_less_eq: return "less_less_eq";
    case TokType.tok_arrow: return "arrow";
	default: return "?";
	}
}

struct SourceLocation {
    uint line;
    uint col;
    string fname;
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
            // debug {
            //     writeln("Error: Token(\"\") called!");
            // }
            this.type = TokType.tok_eof;
            return;
        }

        if(s[0]=='"') {
            if(s[s.length-1]=='"') {
                this.type = TokType.tok_string;
            }
            else lexer_error("Undefined token <"~s~">!");
        }
        else if(s[0]=='\'') {
            if(s[s.length-1]=='\'') {
                this.type = TokType.tok_char;
            }
            else lexer_error("Undefined token <"~s~">!");
        }
        else if(isNumber(cast(dchar)s[0])) {
            this.type = TokType.tok_num;
        }
        else if(s[0]=='(') this.type = TokType.tok_lpar;
        else if(s[0]==')') this.type = TokType.tok_rpar;
        else if(s[0]=='[') this.type = TokType.tok_lbra;
        else if(s[0]==']') this.type = TokType.tok_rbra;
        else if(s[0]=='{') this.type = TokType.tok_2lbra;
        else if(s[0]=='}') this.type = TokType.tok_2rbra;
        else if(s[0]==',') this.type = TokType.tok_comma;
        else if(s=="->") this.type = TokType.tok_struct_get;
        else if(s==";") this.type = TokType.tok_semicolon;
        else if(s=="*=") this.type = TokType.tok_shortmul;
        else if(s=="-=") this.type = TokType.tok_shortmin;
        else if(s=="/=") this.type = TokType.tok_shortdiv;
        else if(s=="+=") this.type = TokType.tok_shortplu;
        else if(s=="<<") this.type = TokType.tok_bit_ls;
        else if(s==">>") this.type = TokType.tok_bit_rs;
        else if(s=="&&") this.type = TokType.tok_and;
        else if(s=="||") this.type = TokType.tok_or;
        else if(s=="|") this.type = TokType.tok_bit_or;
        else if(s=="&") this.type = TokType.tok_bit_and;
        else if(s=="^") this.type = TokType.tok_bit_xor;
        else if(s=="~") this.type = TokType.tok_bit_not;
        else if(s=="!") this.type = TokType.tok_not;
        else if(s=="!=") this.type = TokType.tok_nequal;
        else if(s=="==") this.type = TokType.tok_equal;
        else if(s=="=") this.type = TokType.tok_equ;
        else if(s=="+=") this.type = TokType.tok_shortplu;
        else if(s=="+")  this.type = TokType.tok_plus;
        else if(s=="-=") this.type = TokType.tok_shortmin;
        else if(s=="-")  this.type = TokType.tok_minus;
        else if(s=="*") this.type = TokType.tok_multiply;
        else if(s=="*=") this.type = TokType.tok_shortmul;
        else if(s=="/")  this.type = TokType.tok_divide;
        else if(s=="/=") this.type = TokType.tok_shortdiv;
        else if(s==":")  this.type = TokType.tok_type;
        else if(s==".") this.type = TokType.tok_dot;
        else if(s=="&=") this.type = TokType.tok_bit_and_eq;
        else if(s=="&&=") this.type = TokType.tok_and_eq;
        else if(s=="|=") this.type = TokType.tok_bit_or_eq;
        else if(s=="||=") this.type = TokType.tok_or_eq;
        else if(s=="++") this.type = TokType.tok_plu_plu;
        else if(s=="--") this.type = TokType.tok_min_min;
        else if(s==">>=") this.type = TokType.tok_more_more_eq;
        else if(s=="<<=") this.type = TokType.tok_less_less_eq;
        else if(s=="=>") this.type = TokType.tok_arrow;
        else {
            // Commands or Variables(or Defines)
            switch(s.toLower()) {
                case "if": this.type=TokType.tok_cmd;
                           this.cmd=TokCmd.cmd_if;
                           break;
                case "else": this.type=TokType.tok_cmd;
                           this.cmd=TokCmd.cmd_else;
                           break;
                case "while": this.type=TokType.tok_cmd;
                           this.cmd=TokCmd.cmd_while;
                           break;
                case "do": this.type=TokType.tok_cmd;
                           this.cmd=TokCmd.cmd_do;
                           break;
                case "extern": this.type=TokType.tok_cmd;
                           this.cmd=TokCmd.cmd_extern;
                           break;
                case "asm": this.type=TokType.tok_cmd;
                           this.cmd=TokCmd.cmd_asm;
                           break;
                case "struct": this.type=TokType.tok_cmd;
                           this.cmd=TokCmd.cmd_struct;
                           break;
                case "ret": this.type=TokType.tok_cmd;
                           this.cmd=TokCmd.cmd_ret;
                           break;
                case "break": this.type=TokType.tok_cmd;
                           this.cmd=TokCmd.cmd_break;
                           break;
                case "continue": this.type=TokType.tok_cmd;
                           this.cmd=TokCmd.cmd_continue;
                           break;
                case "sizeof": this.type=TokType.tok_cmd;
                           this.cmd=TokCmd.cmd_sizeof;
                           break;
                case "enum": this.type=TokType.tok_cmd;
                           this.cmd=TokCmd.cmd_enum;
                           break;
                case "static": this.type=TokType.tok_cmd;
                           this.cmd=TokCmd.cmd_static;
                           break;
                case "const": this.type=TokType.tok_cmd;
                           this.cmd=TokCmd.cmd_const;
                           break;
                // Preprocessor commands
                case "inc": this.type=TokType.tok_cmd;
                           this.cmd=TokCmd.cmd_include;
                           break;
                case "def": this.type=TokType.tok_cmd;
                           this.cmd=TokCmd.cmd_define;
                           break;
                case "ifdef": this.type=TokType.tok_cmd;
                           this.cmd=TokCmd.cmd_ifdef;
                           break;
                case "endif": this.type=TokType.tok_cmd;
                           this.cmd=TokCmd.cmd_endif;
                           break;
                // ...or identifier
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
                writeln("Type: ", tokTypeToStr(tok.type), " ", tok.value);
            }
        }
    }
}
