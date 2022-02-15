import std.string;
import std.container : Array;
import std.array;
import logger;
import std.uni : isNumber;

enum TokType {
    tok_num = 0, // Число
    tok_id = 1, // Идентификатор
    tok_equ = 2, //=
    tok_lbra = 3, //[
    tok_rbra = 4, //]
    tok_lpar = 5, //(
    tok_rpar = 6, //)
    tok_plus = 7, //+
    tok_minus = 8, //-
    tok_multiply = 9, //*(но может быть признаком указателя, погугли что такое указатель в С)
    tok_divide = 10, // /
    tok_more = 11, // >
    tok_less = 12, // <
    tok_equal = 13, // ==
    tok_nequal = 14, // !=
    tok_string = 15, // строка
    tok_char = 16, // символ
    tok_comma = 17, // ,
    tok_cmd = 18, // команда
    tok_2rbra = 19, //{
    tok_2lbra = 20, //}
    tok_struct_get = 21, //->
    tok_semicolon = 22, //;
    tok_shortplu = 23, //+=
    tok_shortmin = 24, //-=
    tok_shortmul = 25, //*=
    tok_shortdiv = 26, // /=
    tok_and = 27, // &&
    tok_or = 28, // ||
    tok_bit_and = 29, // &
    tok_bit_or = 30, // |
    tok_bit_ls = 31, // <<
    tok_bit_rs = 32, // >>
    tok_bit_xor = 33, // ^
    tok_bit_not = 34, // ~
    tok_not = 35 // !
}

enum TokCmd {
    cmd_if = 0,
    cmd_else = 1,
    cmd_while = 2,
    cmd_do = 3,
    cmd_include = 4,
    cmd_extern = 5,
    cmd_int = 6,
    cmd_string = 7,
    cmd_char = 8,
    cmd_byte = 9,
    cmd_bit = 10,
    cmd_asm = 11,
    cmd_struct = 12,
    cmd_define = 13,
    cmd_ret = 14,
    cmd_break = 15,
    cmd_continue = 16,
    cmd_sizeof = 17,
    cmd_float = 18,
    cmd_void = 19
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
	default: return "?";
	}
}

class Token {
    TokType type;
    TokCmd cmd;
    string value;

    this(string s) {
        this.value = s;
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
                case "def": this.type=TokType.tok_cmd;
                           this.cmd=TokCmd.cmd_define;
                           break;
                case "extern": this.type=TokType.tok_cmd;
                           this.cmd=TokCmd.cmd_extern;
                           break;
                case "include": this.type=TokType.tok_cmd;
                           this.cmd=TokCmd.cmd_include;
                           break;
                case "asm": this.type=TokType.tok_cmd;
                           this.cmd=TokCmd.cmd_asm;
                           break;
                case "int": this.type=TokType.tok_cmd;
                           this.cmd=TokCmd.cmd_int;
                           break;
                case "char": this.type=TokType.tok_cmd;
                           this.cmd=TokCmd.cmd_char;
                           break;
                case "string": this.type=TokType.tok_cmd;
                           this.cmd=TokCmd.cmd_string;
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
                case "float": this.type=TokType.tok_cmd;
                           this.cmd=TokCmd.cmd_float;
                           break;
                case "void": this.type=TokType.tok_cmd;
                           this.cmd=TokCmd.cmd_void;
                           break;
                case "bit": this.type=TokType.tok_cmd;
                           this.cmd=TokCmd.cmd_bit;
                           break;
                default: this.type = TokType.tok_id; break;
            }
        }
    }
}

class TList {
    Token[int] tokens;
    int i = 0;

    Token opIndex(int index) {
        return tokens[index];
    }

    void insertBack(Token token) {
        tokens[i] = token;
        i+=1;
    }

    int length() { return cast(int)tokens.length; }
}