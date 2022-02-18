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
    "||", "[", "]"
];

class Lexer {
    string lex = "";
    auto tokens = new TList();
    int i = 0;

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
        while(get() >= '0' && get() <= '9') {
            temp ~= get();
            i += 1;
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
                i+=3;
                return get(-2);
            }
            else if(get(+3)=='\'') {
                // Hard char
                i+=4;
                temp = ""~get(-3);
                temp ~= get(-2);
                return to!char(
                    temp.replace("\\n","\n").replace("\\t","\t")
                    .replace("\\r","\r").replace("\\b","\b")
                );
            }
            else if(get(+4)=='\''){
                // Hardest char
                i+=5;
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
        i+=1;
        while(true) {
            if(get()=='\\') {
                if(isNumeric(""~get(+1))) {
                    string t2 = "";
                    t2 ~= get(+1);
                    t2 ~= get(+2);
                    t2 ~= get(+3);
                    temp ~= parseOctalEscape(t2);
                    i+=4;
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
                    i+=2;
                }
            }
            else if(get() == '"') {
                i+=1;
                break;
            }
            else {
                temp ~= get();
                i+=1;
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
            i+=1;
        }
        return temp;
    }

    this(string lex) {
        this.lex = lex;
        while(i<lex.length) {
            if(isWhite(cast(dchar)get())) {
                i+=1;
            }
            else switch(get()) {
                case '0': case '1': case '2':
                case '3': case '4': case '5':
                case '6': case '7': case '8': case '9':
                    tokens.insertBack(new Token(getTNum()));
                    break;
                case '\'':
                    tokens.insertBack(new Token("'"~getTChar()~"'"));
                    break;
                case '"':
                    tokens.insertBack(new Token("\""~getTStr()~"\""));
                    break;
                case '+': tokens.insertBack(new Token("+")); i+=1; break;
                case '/': tokens.insertBack(new Token("/")); i+=1; break;
                case '(': tokens.insertBack(new Token("(")); i+=1; break;
                case ')': tokens.insertBack(new Token(")")); i+=1; break;
                case '{': tokens.insertBack(new Token("{")); i+=1; break;
                case '}': tokens.insertBack(new Token("}")); i+=1; break;
                case '[': tokens.insertBack(new Token("[")); i+=1; break;
                case ']': tokens.insertBack(new Token("]")); i+=1; break;
                case '-':
                    if(get(+1)=='>') {
                        tokens.insertBack(new Token("->"));
                        i+=2;
                    }
                    else {
                        tokens.insertBack(new Token("-"));
                        i+=1;
                    }
                    break;
                case '*': tokens.insertBack(new Token("*")); i+=1; break;
                case ',': tokens.insertBack(new Token(",")); i+=1;break;
                case ';': tokens.insertBack(new Token(";")); i+=1; break;
                case ':': tokens.insertBack(new Token(":")); i+=1; break;
                case '^': tokens.insertBack(new Token("^")); i+=1; break;
                case '~': tokens.insertBack(new Token("~")); i+=1; break;
                case '!':
                    if(get(+1)=='=') {
                        tokens.insertBack(new Token("!="));
                        i+=2;
                    }
                    else {
                        tokens.insertBack(new Token("!"));
                        i+=1;
                    }
                    break;
                case '=':
                    if(get(+1)=='=') {
                        tokens.insertBack(new Token("=="));
                        i+=2;
                    }
                    else {
                        tokens.insertBack(new Token("="));
                        i+=1;
                    }
                    break;
                case '&':
                    if(get(+1)=='&') {
                        tokens.insertBack(new Token("&&"));
                        i+=2;
                    }
                    else {
                        tokens.insertBack(new Token("&"));
                        i+=1;
                    }
                    break;
                case '|':
                    if(get(+1)=='|') {
                        tokens.insertBack(new Token("||"));
                        i+=2;
                    }
                    else {
                        tokens.insertBack(new Token("|"));
                        i+=1;
                    }
                    break;
                case '<':
                    if(get(+1)=='<') {
                        tokens.insertBack(new Token("<<"));
                        i+=2;
                    }
                    else lexer_error("Undefined operator '<'!");
                    break;
                case '>':
                    if(get(+1)=='>') {
                        tokens.insertBack(new Token(">"));
                        i+=2;
                    }
                    else lexer_error("Undefined operator '>'!");
                    break;
                default:
                    tokens.insertBack(new Token(getTID()));
                    break;
            }
        }
    }

    TList getTokens() { return tokens;}
}
