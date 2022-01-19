module tokens;

import std.string;
import std.array;
import std.ascii : isDigit;
import std.stdio;

enum tok_type {
    tok_int,
    tok_char,
    tok_string,
    tok_hex,

    // Commands
    tok_def,
    tok_ret,
    tok_intcmd,
    tok_charcmd,
    tok_stringcmd,
    tok_set,

    // Operators
    tok_sarrcode,
    tok_earrcode,

    tok_lparen,
    tok_rparen,

    tok_equal, tok_nequal,

    tok_plus,
    tok_minus,
    tok_multiply,
    tok_divide,
    // Other
    tok_identifier,
    tok_endl,
    tok_none,
}

bool isNum(char c){return isDigit(cast(dchar)c);}

bool isOperator(char c){
    switch(c) {
        case '+': case '-':
        case '*': case '/':
        case '{': case '}':
        case '(': case ')':
        case ';': case '=': return true;
        default: return false;
    }
}

tok_type get_tok_type(string value) {
    string val=value.strip();

    switch(val) {
        // Main toks
        case "+": return tok_type.tok_plus;
        case "-": return tok_type.tok_minus;
        case "*": return tok_type.tok_multiply;
        case "/": return tok_type.tok_divide;
        case "{": return tok_type.tok_sarrcode;
        case "}": return tok_type.tok_earrcode;
        case "string": return tok_type.tok_stringcmd;
        case "int": return tok_type.tok_intcmd;
        case "char": return tok_type.tok_charcmd;
        case "def": return tok_type.tok_def;
        case "ret": return tok_type.tok_ret;
        case "(": return tok_type.tok_lparen;
        case ")": return tok_type.tok_rparen;
        case ";": return tok_type.tok_endl;
        case "=": return tok_type.tok_set;
        case "==": return tok_type.tok_equal;
        case "!=": return tok_type.tok_nequal;
        default:
            // Other's toks
            if(isNum(val[0])) {
                // is hex
                if(val.indexOf('x')!=-1) {
                    return tok_type.tok_hex;
                }
                // is integer
                return tok_type.tok_int;
            }
            // Is a char
            else if(val[0]=='\'') return tok_type.tok_char;
            // Is a string
            else if(val[0]=='\"') return tok_type.tok_string;
            // Is a identifier
            else return tok_type.tok_identifier;
    }
}

class Token {
    private tok_type type;
    private string value;
    Token[int] childs;

    this(string value) {
        this.value=value.strip();
        this.type=get_tok_type(this.value);
    }

    string get_val() {return this.value;}
    tok_type get_type() {return this.type;}
}

class TokenList {
    private Token[int] tokens;
    private int currtoken;

    this() {this.currtoken=0;}
    this(Token[int] tokens) {this.tokens=tokens;}

    Token get_token(int num) {return this.tokens[num];}
    void set_token(Token tok, int num) {this.tokens[num]=tok;}
    void add_token(Token tok) {
        this.tokens[this.currtoken]=tok;
        this.currtoken+=1;
    }
    void add_new_token(string tok) {
        this.tokens[this.currtoken]=new Token(tok);
        this.currtoken+=1;
    }
    Token[int] get_tokens() {return this.tokens;}
    long length() {return this.tokens.length;}
    Token get_currtoken() {return this.tokens[this.currtoken];}
    void move_ptr(int i){this.currtoken+=i;}
    bool has(int n){return (n in this.tokens);}
    void set_ptr(int i){this.currtoken=i;}
}