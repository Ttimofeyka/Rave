import std.string;
import std.array;
import logger;
import std.uni : isNumber;

enum tok_type {
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
    tok_shortdiv = 26 // /=
}

enum tok_command {
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
    cmd_continue = 16
}

class Token {
    tok_type type;
    tok_command cmd;
    string value;

    this(string s) {
        this.value = s;
        if(s[0]=='"') {
            if(s[s.length-1]=='"') {
                this.type = tok_type.tok_string;
            }
            else lexer_error("Undefined token <"~s~">!");
        }
        else if(s[0]=='\'') {
            if(s[s.length-1]=='\'') {
                this.type = tok_type.tok_char;
            }
            else lexer_error("Undefined token <"~s~">!");
        }
        else if(isNumber(cast(dchar)s[0])) {
            this.type = tok_type.tok_num;
        }
        else if(s[0]=='(') this.type = tok_type.tok_lpar;
        else if(s[0]==')') this.type = tok_type.tok_rpar;
        else if(s[0]=='[') this.type = tok_type.tok_lbra;
        else if(s[0]==']') this.type = tok_type.tok_rbra;
        else if(s[0]=='{') this.type = tok_type.tok_2lbra;
        else if(s[0]=='}') this.type = tok_type.tok_2rbra;
        else if(s[0]==',') this.type = tok_type.tok_comma;
        else if(s=="->") this.type = tok_type.tok_struct_get;
        else if(s==";") this.type = tok_type.tok_semicolon;
        else if(s=="*=") this.type = tok_type.tok_shortmul;
        else if(s=="-=") this.type = tok_type.tok_shortmin;
        else if(s=="/=") this.type = tok_type.tok_shortdiv;
        else if(s=="+=") this.type = tok_type.tok_shortplu;
        else {
            // Commands or Variables(or Defines)
            switch(s.toLower()) {
                case "if": this.type=tok_type.tok_cmd;
                           this.cmd=tok_command.cmd_if;
                           break;
                case "else": this.type=tok_type.tok_cmd;
                           this.cmd=tok_command.cmd_else;
                           break;
                case "while": this.type=tok_type.tok_cmd;
                           this.cmd=tok_command.cmd_while;
                           break;
                case "do": this.type=tok_type.tok_cmd;
                           this.cmd=tok_command.cmd_do;
                           break;
                case "define": this.type=tok_type.tok_cmd;
                           this.cmd=tok_command.cmd_define;
                           break;
                case "extern": this.type=tok_type.tok_cmd;
                           this.cmd=tok_command.cmd_extern;
                           break;
                case "include": this.type=tok_type.tok_cmd;
                           this.cmd=tok_command.cmd_include;
                           break;
                case "asm": this.type=tok_type.tok_cmd;
                           this.cmd=tok_command.cmd_asm;
                           break;
                case "int": this.type=tok_type.tok_cmd;
                           this.cmd=tok_command.cmd_int;
                           break;
                case "char": this.type=tok_type.tok_cmd;
                           this.cmd=tok_command.cmd_char;
                           break;
                case "string": this.type=tok_type.tok_cmd;
                           this.cmd=tok_command.cmd_string;
                           break;
                case "struct": this.type=tok_type.tok_cmd;
                           this.cmd=tok_command.cmd_struct;
                           break;
                case "ret": this.type=tok_type.tok_cmd;
                           this.cmd=tok_command.cmd_ret;
                           break;
                case "break": this.type=tok_type.tok_cmd;
                           this.cmd=tok_command.cmd_break;
                           break;
                case "continue": this.type=tok_type.tok_cmd;
                           this.cmd=tok_command.cmd_continue;
                           break;
                default: this.type = tok_type.tok_id; break;
            }
        }
    }
}

class TList {
    Token[int] tokens;
    int i = 0;

    this() {}
    this(Token t){tokens[i] = t; i+=1;}

    void insertFront(Token t){tokens[i] = t; i+=1;}
    int length(){return cast(int)tokens.length;}
    Token get(int n){return tokens[n];}
}