module lexer.lexer;

import std.string;
import std.array;
import std.conv : to;
import std.stdio;
import std.ascii;
import lexer.tokens;

class Lexer {
    private string text;
    private Token[] tokens;
    private int _idx = 0;
    private int line = 0;
    private bool isMacroArgNum = false;

    private char peek() {
        pragma(inline,true);
        return text[_idx];
    }
    
    private char next() {
        pragma(inline,true);
        _idx++;
        return text[_idx];
    }

    this(string text) {
        this.text = text~"\n";
        bool isPreprocessorCommand = false;
        while(_idx<text.length) {
            if(text[_idx] == '\n') {
                _idx++;
                line++;
            }
            else if(isWhite(cast(char)text[_idx])) _idx++; // Skip whitespaces
            else {
                switch(peek()) {
                    case '+': 
                        if(next() == '=') {
                            tokens ~= new Token(TokType.DivEqu,"+=",line); _idx++;
                        }
                        else tokens ~= new Token(TokType.Plus,"+",line); 
                        break;
                    case '-': 
                        if(next() == '=') {
                            tokens ~= new Token(TokType.MinEqu,"-=",line); _idx++;
                        }
                        else tokens ~= new Token(TokType.Minus,"-",line); 
                        break;
                    case '*': 
                        if(next() == '=') {
                            tokens ~= new Token(TokType.MulEqu,"+=",line); _idx++;
                        }
                        else tokens ~= new Token(TokType.Multiply,"*",line); 
                        break;
                    case '&':
                        if(next()=='&') {
                            tokens ~= new Token(TokType.And,"&&",line);
                            _idx++;
                        }
                        else tokens ~= new Token(TokType.GetPtr,"&",line);
                        break;
                    case '|':
                        if(next()=='|') {
                            tokens ~= new Token(TokType.Or,"||",line);
                            _idx++;
                        }
                        break;
                    case '(': tokens ~= new Token(TokType.Rpar,"(",line); _idx++; break;
                    case ')': tokens ~= new Token(TokType.Lpar,")",line); _idx++; break;
                    case '{': tokens ~= new Token(TokType.Rbra,"{",line); _idx++; break;
                    case '}': tokens ~= new Token(TokType.Lbra,"}",line); _idx++; break;
                    case '[': tokens ~= new Token(TokType.Rarr,"[",line); _idx++; break;
                    case ']': tokens ~= new Token(TokType.Larr,"]",line); _idx++; break;
                    case ',': tokens ~= new Token(TokType.Comma,",",line); _idx++; break;
                    case ':': tokens ~= new Token(TokType.ValSel,":",line); _idx++; break;
                    case '.': tokens ~= new Token(TokType.Dot,".",line); _idx++; break;
                    case '#': 
                        isMacroArgNum = true; _idx++;
                        break;
                    case '/':
                        auto divnext = next();
                        if(divnext == '/') {
                            while(peek() != '\n') {next();}
                        }
                        else if(divnext == '=') {
                            tokens ~= new Token(TokType.DivEqu,"/=",line);
                            _idx += 1;
                        }
                        else tokens ~= new Token(TokType.Divide,"/",line);
                        break;
                    case '>': 
                        if(next() == '=') {
                            tokens ~= new Token(TokType.More,">=",line); _idx++;
                        }
                        else tokens ~= new Token(TokType.More,">",line); 
                        break;
                    case '<': 
                        if(next() == '=') {
                            tokens ~= new Token(TokType.LessEqual,"<=",line); _idx++;
                        }
                        else tokens ~= new Token(TokType.Less,"<",line); 
                        break;
                    case '=':
                        next();
                        if(peek() == '=') {
                            tokens ~= new Token(TokType.Equal,"==",line); _idx++;
                        }
                        else if(peek() == '>') {
                            tokens ~= new Token(TokType.ShortRet,"=>",line); _idx++;
                        }
                        else {
                            tokens ~= new Token(TokType.Equ,"=",line);
                        }
                        break;
                    case '!': 
                        if(next() == '=') {
                            tokens ~= new Token(TokType.Nequal,"!=",line); _idx++;
                        }
                        else tokens ~= new Token(TokType.Ne,"!",line); 
                        break;
                    case ';': tokens ~= new Token(TokType.Semicolon,";",line); _idx++; break;
                    case '"':
                        string bufferstr = "";
                        next();
                        while(peek() != '"') {
                            if(peek() == '\\' && text[_idx+1] == '"') {
                                _idx += 1;
                                bufferstr ~= "\"";
                            }
                            else bufferstr ~= ""~peek();
                            next();
                        }
                        next();
                        tokens ~= new Token(TokType.String,bufferstr,line);
                        break;
                    case '\'':
                        string bufferchar = "";
                        next();
                        while(peek() != '\'') {
                            if(peek() == '\\' && text[_idx+1] == '\'') {
                                _idx += 1;
                                bufferchar ~= "'";
                            }
                            else bufferchar ~= ""~peek();
                            next();
                        }
                        next();
                        tokens ~= new Token(TokType.Char,bufferchar,line);
                        break;
                    case '@':
                        next();
                        isPreprocessorCommand = true;
                        break;
                    default:
                        if(isDigit(cast(dchar)peek())) {
                            string buffernum = ""~peek();
                            if(next() == 'x') {
                                buffernum ~= "x";
                                next();
                                while(isDigit(cast(dchar)peek())) {
                                    buffernum ~= ""~peek();
                                    _idx += 1;
                                }
                                tokens ~= new Token(TokType.HexNumber,buffernum,line);
                            }
                            else {
                                bool isFloat = false;
                                while(isDigit(cast(dchar)peek()) || peek() == '.') {
                                    if(peek() == '.') {
                                        isFloat = true;
                                        buffernum ~= ".";
                                        _idx += 1;
                                        continue;
                                    }
                                    buffernum ~= ""~peek();
                                    _idx += 1;
                                }
                                if(!isFloat) {
                                    if(isMacroArgNum) {
                                        tokens ~= new Token(TokType.MacroArgNum,buffernum,line);
                                        isMacroArgNum = false;
                                    }
                                    else tokens ~= new Token(TokType.Number,buffernum,line);
                                }
                                else tokens ~= new Token(TokType.FloatNumber,buffernum,line);
                            }
                        }
                        else {
                            string bufferiden = "";
                            while(
                                !isWhite(cast(dchar)peek())
                                && peek() != '+'
                                && peek() != '-'
                                && peek() != '*'
                                && peek() != '/'
                                && peek() != '@'
                                && peek() != '('
                                && peek() != ')'
                                && peek() != '{'
                                && peek() != '}'
                                && peek() != ';'
                                && peek() != '='
                                && peek() != '!'
                                && peek() != '<'
                                && peek() != '>'
                                && peek() != ','
                                && peek() != '['
                                && peek() != ']'
                                && peek() != '.'
                                && peek() != '&'
                                && peek() != '#'
                            ) {
                                if(peek() == ':' && text[_idx+1] == ':') {
                                    bufferiden ~= "::";
                                    next();
                                    next();
                                }
                                else if(peek() == ':' && text[_idx+1] != ':') {
                                    break;
                                }
                                else {
                                    bufferiden ~= ""~peek();
                                    next();
                                }
                            }
                            if(isPreprocessorCommand) {
                                tokens ~= new Token(TokType.Command,"@"~bufferiden,line);
                                isPreprocessorCommand = false;
                            }
                            else {
                                if(bufferiden == "null") {
                                    tokens ~= new Token(TokType.Null,"null",line);
                                }
                                else if(bufferiden == "true") {
                                    tokens ~= new Token(TokType.True,"true",line);
                                }
                                else if(bufferiden == "false") {
                                    tokens ~= new Token(TokType.False,"false",line);
                                }
                                else switch(toLower(bufferiden)) {
                                    case "if":
                                        tokens ~= new Token(TokType.Command,"if"); break;
                                    case "else":
                                        tokens ~= new Token(TokType.Command,"else",line); break;
                                    case "ret":
                                        tokens ~= new Token(TokType.Command,"ret",line); break;
                                    case "while":
                                        tokens ~= new Token(TokType.Command,"while",line); break;
                                    case "break":
                                        tokens ~= new Token(TokType.Command,"break",line); break;
                                    case "continue":
                                        tokens ~= new Token(TokType.Command,"continue",line); break;
                                    case "addr":
                                        tokens ~= new Token(TokType.Command,"addr",line); break;
                                    case "itop":
                                        tokens ~= new Token(TokType.Command,"itop",line); break;
                                    case "ptoi":
                                        tokens ~= new Token(TokType.Command,"ptoi",line); break;
                                    case "namespace":
                                        tokens ~= new Token(TokType.Command,"namespace",line); break;
                                    default:
                                        tokens ~= new Token(TokType.Identifier,bufferiden,line); break;
                                }
                            }
                        }
                        break;
                }
            }
        }
        this.tokens ~= new Token(TokType.Eof,"EOF");
    }

    Token[] getTokens() {
        return this.tokens.dup;
    }

    void debugPrint() {
        for(int i=0; i<tokens.length-1; i++) {
		    writeln(to!string(tokens[i].type)~": \""~tokens[i].value~"\"");
	    }
    }
}