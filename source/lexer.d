module lexer;

public import tokens;
import std.string;
import std.array;
public import logger;
import std.stdio;
import std.conv : to;
public import ast;

class Lexer {
    private string input_str;
    private TokenList tokens;
    private int currchar;
    
    this(string input_str) {
        this.input_str=input_str~" ";
        this.currchar=0;
        this.tokens=new TokenList();
        
        int i=0;
        while(i<this.input_str.length) {
            // is a whitespace or newline or tab?
            if(this.input_str[i]==' '
             ||this.input_str[i]=='\t'
             ||this.input_str[i]=='\n') {
                 i+=1; continue;
            }
            
            // is a comment?
            if(this.input_str[i]=='#') {
                int j=i;
                while(j<this.input_str.length) {
                    if(this.input_str[j]!='\n') {
                        j+=1;
                    }
                    else {i=j; j=to!int(this.input_str.length);}
                }
            }

            // is a nequal?
            if(this.input_str[i]=='!'&&this.input_str[i+1]=='=') {
                this.tokens.add_new_token("!=");
                i+=2;
            }
            // is a operator?
            else if(isOperator(this.input_str[i])) {
                if(this.input_str[i]=='='&&this.input_str[i+1]=='=') {
                    this.tokens.add_new_token("==");
                    i+=2;
                }
                else {
                    this.tokens.add_new_token(""~this.input_str[i]);
                    i+=1; continue;
                }
            }
            // is a char?
            else if(this.input_str[i]=='\'') {
                // is a empty char?
                if(this.input_str[i+1]=='\'') {
                    error("An empty character was detected");
                }
                // else
                else {
                    this.tokens.add_new_token(
                        "\'"~this.input_str[i+1]~"\'"
                    );
                    i+=3;
                }
            }
            // is a number?
            else if(isNum(this.input_str[i])) {
                if(this.input_str[i+1]=='x') {
                    string temp="0x";
                    int j=i+2;
                    while(j<this.input_str.length) {
                        if(
                            !isOperator(this.input_str[j])
                          &&this.input_str[j]!=' '
                          &&this.input_str[j]!='\n'
                          &&this.input_str[j]!='\t'
                        ) {
                            temp~=this.input_str[j];
                            j+=1;
                        }
                        else {i=j; j=to!int(this.input_str.length);}
                    }
                    this.tokens.add_new_token(temp);
                }
                else {
                    string temp="";
                    int j=i;
                    while(j<this.input_str.length) {
                        if(isNum(this.input_str[j])) {
                            temp~=this.input_str[j];
                            j+=1;
                        }
                        else {i=j; j=to!int(this.input_str.length);}
                    }
                    this.tokens.add_new_token(temp);
                }
            }
            // is a string?
            else if(this.input_str[i]=='\"') {
                string temp="\"";
                int j=i+1;
                while(j<this.input_str.length) {
                    if(this.input_str[j]!='\"') {
                        temp~=this.input_str[j];
                        j+=1;
                    }
                    else {i=j; j=to!int(this.input_str.length);}
                }
                i+=1;
                this.tokens.add_new_token(temp~"\"");
            }
            // is a identifier.
            else {
                string temp="";
                int j=i;
                while(j<this.input_str.length) {
                    if(
                        !isOperator(this.input_str[j])
                       &&this.input_str[j]!='\"'
                       &&this.input_str[j]!='\''
                       &&this.input_str[j]!='!'
                       &&this.input_str[j]!=' '
                       &&this.input_str[j]!='\n'
                       &&this.input_str[j]!='\t'
                    ) {
                        temp~=this.input_str[j];
                        j+=1;
                    }
                    else {i=j; j=to!int(this.input_str.length);}
                }
                this.tokens.add_new_token(temp);
            }
        }
    }

    TokenList get_tokenlist() {return this.tokens;}
}