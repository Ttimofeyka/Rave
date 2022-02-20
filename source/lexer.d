module lexer;

import std.string;
import std.array;
import tokens;
import logger;
import std.conv : to;
import std.stdio;
import std.algorithm: canFind;
import std.ascii : isWhite;

const string[] operators = [
    "=", "==", "!=", "+=",
    "-=", "*=", "/=", "+",
    "-", "*", "/", "\"",
    "\'", "(", ")", "{",
    "}", ";", ",", "<", ">",
    "&", "&&", "^", "~", "|",
    "||", "[", "]",

    "|=", "&=",
    "||=", "&&=",
    ">>=", "<<=",
    "++", "--",
    "=>"
];

class Lexer {
    string lex;
    TList tokens;
    int i;
    SourceLocation loc;

    bool _hadHash = false;

    private char get(uint x = 0) {
        if(i < lex.length) {
            return lex[i + x];
        }
        else {
            return '\0';
        }
    }

    private string getTNum() {
        string temp = "";
        if(get() == '0' && get(+1) == 'x') {
            temp ~= "0x";
            next(2);
            while((get() >= '0' && get() <= '9')
               || (get() >= 'a' && get() <= 'f')
               || (get() >= 'A' && get() <= 'F')) {
                temp ~= get();
                next(1);
            }
        }
        else if(get() == '0' && get(+1) == 'o') {
            temp ~= "0o";
            next(2);
            while((get() >= '0' && get() <= '7')) {
                temp ~= get();
                next(1);
            }
        }
        else if(get() == '0' && get(+1) == 'b') {
            temp ~= "0b";
            next(2);
            while((get() >= '0' && get() <= '1')) {
                temp ~= get();
                next(1);
            }
        }
        else if(get() == '0' && get(+1) == 'd') {
            temp ~= "0d";
            next(2);
            while((get() >= '0' && get() <= '9')) {
                temp ~= get();
                next(1);
            }
        }
        else {
            while((get() >= '0' && get() <= '9')) {
                temp ~= get();
                next(1);
            }
        }
        return temp;
    }

    private char parseOctalEscape(string s)
    {
        assert(s.length == 4);
        assert(s[0] == '\\');
        return cast(char)s[1 .. $].to!int(8);
    }
    
    private char getTChar() {
        if(get(+1) == '\'') {
            lexer_error("Empty character!");
            return ' ';
        }
        else {
            string temp = "";
            if(get(+2)=='\'') {
                // Simple char
                next(3);
                return get(-2);
            }
            else if(get(+3)=='\'') {
                // Hard char
                next(4);
                temp = ""~get(-3);
                temp ~= get(-2);
                return to!char(
                    temp.replace("\\n","\n").replace("\\t","\t")
                    .replace("\\r","\r").replace("\\b","\b")
                );
            }
            else if(get(+4)=='\''){
                // Hardest char
                next(5);
                temp = ""~get(-4);
                temp ~= get(-3);
                temp ~= get(-2);
                return parseOctalEscape(temp);
            }
            else {lexer_error("The char is too long!"); return 0;}
        }
    }

    private string getTStr() {
        string temp = "";
        next(1);
        while(true) {
            if(get()=='\\') {
                if(isNumeric(""~get(+1))) {
                    string t2 = "";
                    t2 ~= get(+1);
                    t2 ~= get(+2);
                    t2 ~= get(+3);
                    temp ~= parseOctalEscape(t2);
                    next(4);
                }
                else {
                    if(get(+1)=='n') {
                        temp ~= "\n";
                    }
                    else if(get(+1)=='t') {
                        temp ~= "\t";
                    }
                    else if(get(+1)=='"') {
                        temp ~= "\"";
                    }
                    else if(get(+1)=='0') {
                        temp ~= "\0";
                    }
                    else if(get(+1)=='\'') {
                        temp ~= "\'";
                    }
                    else if(get(+1)=='b') {
                        temp ~= "\b";
                    }
                    else if(get(+1)=='r') {
                        temp ~= "\r";
                    }
                    next(2);
                }
            }
            else if(get() == '"') {
                next(1);
                break;
            }
            else {
                temp ~= get();
                next(1);
            }
        }
        return temp;
    }

    private string getTID() {
        string temp = "";
        while((get() >= 'a' && get() <= 'z')
           || (get() >= 'A' && get() <= 'Z')
           || (get() >= '0' && get() <= '9')
           || get() == '_') {
            temp ~= get();
            next(1);
        }
        return temp;
    }

    private void next(uint n) {
        loc.col += n;
        i += n;
    }

    private void skipLineCom() {
        while(get() != '\n' && get() != '\0') {
            next(1);
        }
        loc.line += 1;
    }

    private void skipMultiLineCom() {
        while((get() != '*' && get(+1) != '/') && get() != '\0') {
            if(get() == '\n') loc.line += 1;
            next(1);
        }
        if(get() == '*') {next(2);}
    }

    this(string fname, string lex) {
        this.lex = lex;
        this.loc.fname = fname;
        this.tokens = new TList();
        this.i = 0;
        this._hadHash = false;

        while(i<lex.length) {
            if(isWhite(cast(dchar)get())) {
                if(get() == '\n') {
                    if(_hadHash) {
                        _hadHash = false;
                        tokens.insertBack(new Token(loc, "#"));
                    }
                    loc.line += 1;
                    loc.col = 0;
                }
                next(1);
            }
            else switch(get()) {
                case '#':
                    _hadHash = true;
                    tokens.insertBack(new Token(loc, "#"));
                    next(1);
                    break;
                case '0': case '1': case '2':
                case '3': case '4': case '5':
                case '6': case '7': case '8': case '9':
                    tokens.insertBack(new Token(loc, getTNum()));
                    break;
                case '\'':
                    tokens.insertBack(new Token(loc, "'"~getTChar()~"'"));
                    break;
                case '"':
                    tokens.insertBack(new Token(loc, "\""~getTStr()~"\""));
                    break;
                case '(': tokens.insertBack(new Token(loc, "(")); next(1); break;
                case ')': tokens.insertBack(new Token(loc, ")")); next(1); break;
                case '{': tokens.insertBack(new Token(loc, "{")); next(1); break;
                case '}': tokens.insertBack(new Token(loc, "}")); next(1); break;
                case '[': tokens.insertBack(new Token(loc, "[")); next(1); break;
                case ']': tokens.insertBack(new Token(loc, "]")); next(1); break;
                case '+': 
                    if(get(+1)=='=') {
                        tokens.insertBack(new Token(loc, "+="));
                        next(2);
                    }
                    else if(get(+1)=='+') {
                        tokens.insertBack(new Token(loc, "++"));
                        next(2);
                    }
                    else {
                        tokens.insertBack(new Token(loc, "+"));
                        next(1);
                    }
                    break;
                case '*': 
                    if(get(+1)=='=') {
                        tokens.insertBack(new Token(loc, "*="));
                        next(2);
                    }
                    else {
                        tokens.insertBack(new Token(loc, "*"));
                        next(1);
                    }
                    break;
                case '/': 
                    if(get(+1)=='=') {
                        tokens.insertBack(new Token(loc, "/="));
                        next(2);
                    }
                    else if(get(+1)=='/') {
                        next(2);
                        skipLineCom();
                    }
                    else if(get(+1)=='*') {
                        next(2);
                        skipMultiLineCom();
                    }
                    else {
                        tokens.insertBack(new Token(loc, "/"));
                        next(1);
                    }
                    break;
                case '-':
                    if(get(+1)=='>') {
                        tokens.insertBack(new Token(loc, "->"));
                        next(2);
                    }
                    else if(get(+1)=='=') {
                        tokens.insertBack(new Token(loc, "-="));
                        next(2);
                    }
                    else if(get(+1)=='-') {
                        tokens.insertBack(new Token(loc, "--"));
                        next(2);
                    }
                    else {
                        tokens.insertBack(new Token(loc, "-"));
                        next(1);
                    }
                    break;
                case ',': tokens.insertBack(new Token(loc, ",")); next(1);break;
                case ';': tokens.insertBack(new Token(loc, ";")); next(1); break;
                case ':': tokens.insertBack(new Token(loc, ":")); next(1); break;
                case '^': tokens.insertBack(new Token(loc, "^")); next(1); break;
                case '~': tokens.insertBack(new Token(loc, "~")); next(1); break;
                case '!':
                    if(get(+1)=='=') {
                        tokens.insertBack(new Token(loc, "!="));
                        next(2);
                    }
                    else {
                        tokens.insertBack(new Token(loc, "!"));
                        next(1);
                    }
                    break;
                case '=':
                    if(get(+1)=='=') {
                        tokens.insertBack(new Token(loc, "=="));
                        next(2);
                    }
                    else if(get(+1)=='>') {
                        tokens.insertBack(new Token(loc, "=>"));
                        next(2);
                    }
                    else {
                        tokens.insertBack(new Token(loc, "="));
                        next(1);
                    }
                    break;
                case '&':
                    if(get(+1)=='&') {
                        if(get(+2)=='=') {
                            tokens.insertBack(new Token(loc, "&&="));
                            next(3);
                        }
                        else {
                            tokens.insertBack(new Token(loc, "&&"));
                            next(2);
                        }
                    }
                    else if(get(+1)=='=') {
                        tokens.insertBack(new Token(loc, "&="));
                        next(2);
                    }
                    else {
                        tokens.insertBack(new Token(loc, "&"));
                        next(1);
                    }
                    break;
                case '|':
                    if(get(+1)=='|') {
                        if(get(+2)=='=') {
                            tokens.insertBack(new Token(loc, "||="));
                            next(3);
                        }
                        else {
                            tokens.insertBack(new Token(loc, "||"));
                            next(2);
                        }
                    }
                    else if(get(+1)=='=') {
                        tokens.insertBack(new Token(loc, "|="));
                        next(2);
                    }
                    else {
                        tokens.insertBack(new Token(loc, "|"));
                        next(1);
                    }
                    break;
                case '<':
                    if(get(+1)=='<') {
                        tokens.insertBack(new Token(loc, "<<"));
                        next(2);
                    }
                    else {
                        tokens.insertBack(new Token(loc, "<"));
                        next(1);
                    }
                    break;
                case '>':
                    if(get(+1)=='>') {
                        tokens.insertBack(new Token(loc, ">>"));
                        next(2);
                    }
                    else {
                        tokens.insertBack(new Token(loc, ">"));
                        next(1);
                    }
                    break;
                default:
                    tokens.insertBack(new Token(loc, getTID()));
                    break;
            }
        }
    }

    TList getTokens() { return tokens; }
}
