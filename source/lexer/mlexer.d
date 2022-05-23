module lexer.mlexer;

import std.string;
import std.array;
import lexer.tokens;
import lexer.logger;
import std.conv : to;
import std.stdio;
import std.algorithm: canFind;
import std.ascii : isWhite;
import std.string : strip;

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

    bool _hadAt = false;

    private char get(uint x = 0) {
        pragma(inline,true);
        return (i<lex.length) ? lex[i+x] : '\0';
    }

    private string getTInt() {
        string temp = "";
        char fg = get();
        if(fg == '0' && get(+1) == 'x') {
            temp ~= "0x";
            next(2);
            char g = get();
            while((g >= '0' && g <= '9')
               || (g >= 'a' && g <= 'f')
               || (g >= 'A' && g <= 'F') || g == '_') {
                temp ~= g;
                next(1);
                g = get();
            }
        }
        else if(fg == '0' && get(+1) == 'o') {
            temp ~= "0o";
            next(2);
            while((get() >= '0' && get() <= '7') || get() == '_') {
                temp ~= get();
                next(1);
            }
        }
        else if(fg == '0' && get(+1) == 'b') {
            temp ~= "0b";
            next(2);
            while((get() >= '0' && get() <= '1') || get() == '_') {
                temp ~= get();
                next(1);
            }
        }
        else if(fg == '0' && get(+1) == 'd') {
            temp ~= "0d";
            next(2);
            while((get() >= '0' && get() <= '9') || get() == '_') {
                temp ~= get();
                next(1);
            }
        }
        else {
            while((get() >= '0' && get() <= '9') || get() == '_') {
                temp ~= get();
                next(1);
            }
        }
        return temp;
    }

    private string getTNum() {
        auto t = getTInt();
        if(get() == '.') {
            next(1);
            t ~= "." ~ getTInt();
        }
        return t;
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
                    .replace("\\0","\0")
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
        char ch = get();
        while((ch >= 'a' && ch <= 'z')
           || (ch >= 'A' && ch <= 'Z')
           || (ch >= '0' && ch <= '9')
           || ch == '_'
           || (
               ch == ':' && get(-1) == ':'
               || ch == ':' && get(1) == ':'
           )) {
            temp ~= ch;
            next(1);
            ch = get();
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
        // loc.line += 1;
    }

    private string getMultiLineCom() {
        string s = "/*";
        next(1);
        // writeln("get, get+1 = '", get(), "', '", get(+1), "'");
        while((get() != '*' || get(+1) != '/') && get() != '\0') {
            if(get() == '\n') {
                s = s.strip().dup;
                loc.line += 1;
            }
            s ~= get();
            next(1);
        }
        if(get() == '*') {next(2);}
        return s;
    }

    private void skipMultiLineCom() {
        while((get() != '*' || get(+1) != '/') && get() != '\0') {
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
        this._hadAt = false;

        while(i<lex.length) {
            if(isWhite(cast(dchar)get())) {
                if(get() == '\n') {
                    loc.line += 1;
                    loc.col = 0;
                }
                next(1);
            }
            else switch(get()) {
                case '#':
                    tokens.insertBack(new Token(loc, "#", TokType.tok_hash));
                    next(1);
                    break;
                case '0': case '1': case '2':
                case '3': case '4': case '5':
                case '6': case '7': case '8': case '9':
                    tokens.insertBack(new Token(loc, getTNum(), TokType.tok_num));
                    break;
                case '\'': tokens.insertBack(new Token(loc, "'"~getTChar()~"'", TokType.tok_char)); break;
                case '"': tokens.insertBack(new Token(loc, "\""~getTStr()~"\"", TokType.tok_string)); break;
                case '(': tokens.insertBack(new Token(loc, "(", TokType.tok_lpar)); next(1); break;
                case ')': tokens.insertBack(new Token(loc, ")", TokType.tok_rpar)); next(1); break;
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
                        tokens.insertBack(new Token(loc, "+", TokType.tok_plus));
                        next(1);
                    }
                    break;
                case '*': 
                    if(get(+1)=='=') {
                        tokens.insertBack(new Token(loc, "*="));
                        next(2);
                    }
                    else {
                        tokens.insertBack(new Token(loc, "*", TokType.tok_multiply));
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
                    else if(get(+1) == '*') {
                        next(2);
                        if(get() == '!') { // documentation comment
                            // writeln("doc comment");
                            auto str = getMultiLineCom();
                            // writeln(str);
                            tokens.insertBack(new Token(loc, str));
                        }
                        else skipMultiLineCom();
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
                        tokens.insertBack(new Token(loc, "-", TokType.tok_minus));
                        next(1);
                    }
                    break;
                case ',': tokens.insertBack(new Token(loc, ",", TokType.tok_comma)); next(1); break;
                case '.': 
                    if(get(+1)=='.') {
                        tokens.insertBack(new Token(loc, "..", TokType.tok_multiargs)); 
                        next(1); 
                    }
                    else {
                        tokens.insertBack(new Token(loc, ".")); 
                        next(1); 
                    }
                    break;
                case ';': tokens.insertBack(new Token(loc, ";", TokType.tok_semicolon)); next(1); break;
                case ':': 
                    if(get(+1)==':') {
                        tokens.insertBack(new Token(loc, "::"));
                        next(2);
                    }
                    else {
                        tokens.insertBack(new Token(loc, ":"));
                        next(1);
                    }
                    break;
                case '^': tokens.insertBack(new Token(loc, "^")); next(1); break;
                case '@': _hadAt = true; tokens.insertBack(new Token(loc, "@")); next(1); break;
                case '~': tokens.insertBack(new Token(loc, "~")); next(1); break;
                case '!':
                    if(get(+1)=='=') {
                        tokens.insertBack(new Token(loc, "!=", TokType.tok_nequal));
                        next(2);
                    }
                    else {
                        tokens.insertBack(new Token(loc, "!"));
                        next(1);
                    }
                    break;
                case '=':
                    if(get(+1)=='=') {
                        tokens.insertBack(new Token(loc, "==", TokType.tok_equal));
                        next(2);
                    }
                    else if(get(+1)=='>') {
                        tokens.insertBack(new Token(loc, "=>"));
                        next(2);
                    }
                    else {
                        tokens.insertBack(new Token(loc, "=", TokType.tok_equ));
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
                    else if(get(+1)=='=') {
                        tokens.insertBack(new Token(loc, "<="));
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
                    else if(get(+1)=='=') {
                        tokens.insertBack(new Token(loc, ">=", TokType.tok_more_equal));
                        next(2);
                    }
                    else {
                        tokens.insertBack(new Token(loc, ">", TokType.tok_more));
                        next(1);
                    }
                    break;
                default:
                {
                    auto t = getTID();
                    tokens.insertBack(new Token(loc, (_hadAt ? "@" : "") ~ t));
                    _hadAt = false;
                } break;
            }
        }
    }

    TList getTokens() { return tokens; }
}
