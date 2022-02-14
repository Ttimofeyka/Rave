module lexer;

import std.string;
import std.array;
import std.container : DList;
import tokens;
import logger;
import std.conv : to;
import std.stdio;

class Lexer {
    string lex = "";
    auto tokens = new TList();
    int i = 0;

    private bool isOperator(string s) {
        return (
            s=="=="||s=="!="||s=="+"||
            s=="-"||s=="*"||s=="/"||s=="="
            ||s=="["||s=="]"||s=="{"||s=="}"
            ||s=="("||s==")"||s==","||s=="\""
            ||s=="'"||s==";"||s=="<"||s==">"
            ||s=="!"||s=="^"||s=="~"||s=="&&"
            ||s=="&"||s=="||"||s=="|"
        );
    }

    private string getTNum() {
        string temp = "";
        while(
            !isOperator(""~lex[i])
            &&lex[i]!=' '&&lex[i]!='\n'
            &&lex[i]!='\t'
        ) {temp~=lex[i]; i+=1;}
        return temp;
    }

    private char parseOctalEscape(string s)
    {
        assert(s.length == 4);
        assert(s[0] == '\\');
        return cast(char)s[1 .. $].to!int(8);
    }
    
    private char getTChar() {
        if(lex[i+1] == '\'') {
            lexer_error("Empty character!");
            return ' ';
        }
        else {
            string temp = "";
            if(lex[i+2]=='\'') {
                // Simple char
                i+=3;
                return lex[i-2];
            }
            else if(lex[i+3]=='\'') {
                // Hard char
                i+=4;
                temp = ""~lex[i-3];
                temp ~= lex[i-2];
                return to!char(
                    temp.replace("\\n","\n").replace("\\t","\t")
                    .replace("\\n","\r").replace("\\b","\b")
                );
            }
            else if(lex[i+4]=='\''){
                // Hardest char
                i+=5;
                temp = ""~lex[i-4];
                temp ~= lex[i-3];
                temp ~= lex[i-2];
                return parseOctalEscape(temp);
            }
            else {lexer_error("The char is too long!"); return 0;}
        }
    }

    private string getTStr() {
        string temp = "";
        i+=1;
        while(true) {
            if(lex[i]=='\\') {
                if(isNumeric(""~lex[i+1])) {
                    string t2 = "";
                    t2 ~= lex[i+1];
                    t2 ~= lex[i+2];
                    t2 ~= lex[i+3];
                    temp ~= parseOctalEscape(t2);
                    i+=4;
                }
                else {
                    if(lex[i+1]=='n') {
                        temp ~= "\n";
                    }
                    else if(lex[i+1]=='t') {
                        temp ~= "\t";
                    }
                    else if(lex[i+1]=='"') {
                        temp ~= "\"";
                    }
                    else if(lex[i+1]=='0') {
                        temp ~= "\0";
                    }
                    else if(lex[i+1]=='\'') {
                        temp ~= "\'";
                    }
                    else if(lex[i+1]=='b') {
                        temp ~= "\b";
                    }
                    i+=2;
                }
            }
            else if(lex[i] == '"') {
                i+=1;
                break;
            }
            else {
                temp ~= lex[i];
                i+=1;
            }
        }
        return temp;
    }

    private string getTID() {
        string temp = "";
        while(
            !isOperator(""~lex[i])
            &&lex[i]!=' '
            &&lex[i]!='\n'
            &&lex[i]!='\t'
        ) {
            temp ~= lex[i];
            i+=1;
        }
        return temp;
    }

    this(string lex) {
        this.lex = lex;
        while(i<lex.length) {
            switch(lex[i]) {
                case ' ':
                case '\n':
                case '\t':
                case '\r':
                    i+=1;
                    break;
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
                    if(lex[i+1]=='>') {
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
                case '^': tokens.insertBack(new Token("^")); i+=1; break;
                case '~': tokens.insertBack(new Token("~")); i+=1; break;
                case '!':
                    if(lex[i+1]=='=') {
                        tokens.insertBack(new Token("!="));
                        i+=2;
                    }
                    else {
                        tokens.insertBack(new Token("!"));
                        i+=1;
                    }
                    break;
                case '=':
                    if(lex[i+1]=='=') {
                        tokens.insertBack(new Token("=="));
                        i+=2;
                    }
                    else {
                        tokens.insertBack(new Token("="));
                        i+=1;
                    }
                    break;
                case '&':
                    if(lex[i+1]=='&') {
                        tokens.insertBack(new Token("&&"));
                        i+=2;
                    }
                    else {
                        tokens.insertBack(new Token("&"));
                        i+=1;
                    }
                    break;
                case '|':
                    if(lex[i+1]=='|') {
                        tokens.insertBack(new Token("||"));
                        i+=2;
                    }
                    else {
                        tokens.insertBack(new Token("|"));
                        i+=1;
                    }
                    break;
                case '<':
                    if(lex[i+1]=='<') {
                        tokens.insertBack(new Token("<<"));
                        i+=2;
                    }
                    else lexer_error("Undefined operator '<'!");
                    break;
                case '>':
                    if(lex[i+1]=='>') {
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