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
        _idx += 1;
        return text[_idx];
    }
    
    private char replaceAllEscapesInChar(string str) {
        string newchar = 
            str.replace("\\n","\n")
            .replace("\\r","\r")
            .replace("\\t","\t");
        if(newchar[0] == '\\') {
            return cast(char)(str[1..$]).to!int(8);
        }
        return to!char(newchar);
    }

    private string getNumEscape() {
        // Current symbol - number
        string buffer = "";
        while(isNumeric(peek()~"")) {
            buffer ~= peek();
            next();
        }
        _idx -= 1;
        return cast(char)(buffer.to!int(8))~"";
    }

    this(string text, int offset) {
        import core.stdc.stdlib : exit;
        this.text = text~"\n";
        bool isPreprocessorCommand = false;

        this.line = 0 - offset;

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
                            tokens ~= new Token(TokType.PluEqu,"+=",line); _idx++;
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
                        char _n = next();
                        if(_n =='|') {
                            tokens ~= new Token(TokType.Or,"||",line);
                            _idx++;
                        }
                        else if(_n == '.') {
                            tokens ~= new Token(TokType.BitXor,"|.",line);
                            _idx++;
                        }
                        else tokens ~= new Token(TokType.BitOr,"|",line);
                        break;
                    case '(': tokens ~= new Token(TokType.Rpar,"(",line); _idx++; break;
                    case ')': tokens ~= new Token(TokType.Lpar,")",line); _idx++; break;
                    case '{': tokens ~= new Token(TokType.Rbra,"{",line); _idx++; break;
                    case '}': tokens ~= new Token(TokType.Lbra,"}",line); _idx++; break;
                    case '[': tokens ~= new Token(TokType.Rarr,"[",line); _idx++; break;
                    case ']': tokens ~= new Token(TokType.Larr,"]",line); _idx++; break;
                    case ',': tokens ~= new Token(TokType.Comma,",",line); _idx++; break;
                    case ':': tokens ~= new Token(TokType.ValSel,":",line); _idx++; break;
                    case '%': tokens ~= new Token(TokType.Rem,"%",line); _idx++; break;
                    case '@':
                        string bbuff = "";
                        next();
                        while(!isNumeric(to!string(peek())) && peek() != '.' && peek() != '+' && peek() != '-' && peek() != '*' && peek() != '/' && peek() != '=' && peek() != '(') {
                            bbuff ~= peek();
                            next();
                        }
                        tokens ~= new Token(TokType.Builtin,bbuff,line);
                        break;
                    case '.':
                        auto n = next();
                        if(n == '.') {
                            n = next();
                            if(n == '.') {
                                tokens ~= new Token(TokType.VarArg,"...",line); _idx++; break;
                            }
                            writeln("Undefined operator '..' at ",line," line!"); 
                            exit(1);
                        }
                        tokens ~= new Token(TokType.Dot,".",line); break;
                    case '#': 
                        isMacroArgNum = true; _idx++;
                        break;
                    case '~':
                        // Destructor call or "~this"?
                        auto n = next();
                        if(n == 't') {
                            // "~this"
                            n = next();
                            if(n == 'h') {
                                n = next();
                                if(n == 'i') {
                                    n = next();
                                    if(n == 's') {
                                        n = next();
                                        if(n != '.') tokens ~= new Token(TokType.Identifier,"~this",line);
                                        else {tokens ~= new Token(TokType.Destructor,"~",line); _idx -= 4;}
                                    }
                                    else {tokens ~= new Token(TokType.Destructor,"~",line); _idx -= 3;}
                                }
                                else {tokens ~= new Token(TokType.Destructor,"~",line); _idx -= 2;}
                            }
                            else {tokens ~= new Token(TokType.Destructor,"~",line); _idx -= 1;}
                        }
                        else tokens ~= new Token(TokType.Destructor,"~",line); break;
                    case '/':
                        auto divnext = next();
                        if(divnext == '/') {
                            while(peek() != '\n') {next();}
                        }
                        else if(divnext == '=') {
                            tokens ~= new Token(TokType.DivEqu,"/=",line);
                            _idx += 1;
                        }
                        else if(divnext == '*') {
                            _idx += 1;
                            while(text[_idx] != '*' || text[_idx+1] != '/') {
                                _idx += 1;
                                if(_idx+1 >= text.length) break;
                            }
                            // Current char - '*'
                            next(); next();
                        }
                        else tokens ~= new Token(TokType.Divide,"/",line);
                        break;
                    case '>':
                        auto rnext = next(); 
                        if(rnext == '=') {
                            tokens ~= new Token(TokType.More,">=",line); _idx++;
                        }
                        else if(rnext == '.') {tokens ~= new Token(TokType.BitRight,">>",line); _idx++;}
                        else tokens ~= new Token(TokType.More,">",line); 
                        break;
                    case '<': 
                        auto lnext = next();
                        if(lnext == '=') {
                            tokens ~= new Token(TokType.LessEqual,"<=",line); _idx++;
                        }
                        else if(lnext == '.') {
                            tokens ~= new Token(TokType.BitLeft,"<<",line); _idx++;
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
                        char _n = next();
                        if(_n == '=') {
                            tokens ~= new Token(TokType.Nequal,"!=",line); _idx++;
                        }
                        else if(_n == '.') {
                            tokens ~= new Token(TokType.BitXor,"!!",line); _idx++;
                        }
                        else tokens ~= new Token(TokType.Ne,"!",line); 
                        break;
                    case ';': tokens ~= new Token(TokType.Semicolon,";",line); _idx++; break;
                    case '"':
                        string bufferstr = "";
                        next();
                        while(peek() != '"') {
                            if(peek() == '\\') {
                                if(text[_idx+1] == '"') {
                                    _idx += 1;
                                    bufferstr ~= "\"";
                                }
                                else if(text[_idx+1] == '\\') {
                                    _idx += 1;
                                    bufferstr ~= "\\";
                                }
                                else if(isNumeric(text[_idx+1]~"")) {
                                    _idx += 1;
                                    bufferstr ~= getNumEscape();
                                }
                                else if(text[_idx+1] == 'n') {
                                    _idx += 1;
                                    bufferstr ~= "\n";
                                }
                                else if(text[_idx+1] == 't') {
                                    _idx += 1;
                                    bufferstr ~= "\t";
                                }
                                else if(text[_idx+1] == 'r') {
                                    _idx += 1;
                                    bufferstr ~= "\r";
                                }
                                else if(text[_idx+1] == 'x') {
                                    _idx += 1;
                                    string s = "";
                                    _idx += 1;
                                    s ~= text[_idx];
                                    _idx += 1;
                                    s ~= text[_idx];
                                    bufferstr ~= to!string(s.to!int(16));
                                }
                                else if(text[_idx+1] == 'u') {
                                    _idx += 1;
                                    string s ="";
                                    _idx += 1;
                                    s ~= text[_idx];
                                    _idx += 1;
                                    s ~= text[_idx];
                                    _idx += 1;
                                    s ~= text[_idx];
                                    _idx += 1;
                                    s ~= text[_idx];
                                    bufferstr ~= to!string(s.to!int(16));
                                }
                                else {
                                    exit(1);
                                }
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
                        tokens ~= new Token(TokType.Char,replaceAllEscapesInChar(bufferchar)~"",line);
                        break;
                    default:
                        if(isDigit(cast(dchar)peek())) {
                            string buffernum = ""~peek();
                            if(next() == 'x') {
                                buffernum ~= "x";
                                next();
                                while(isDigit(cast(dchar)peek()) || peek() == 'A' || peek() == 'B' || peek() == 'C' || peek() == 'D' || peek() == 'E' || peek() == 'F') {
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
                                && peek() != '~'
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
                                    case "else":
                                    case "for":
                                    case "return":
                                    case "while":
                                    case "break":
                                    case "continue":
                                    case "namespace":
                                    case "with":
                                    case "asm":
                                        tokens ~= new Token(TokType.Command,toLower(bufferiden)); break;
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