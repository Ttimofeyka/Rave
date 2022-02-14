module preproc;

import tokens;
import std.string;
import std.array;

class Preprocessor {
    TList tokens;
    TList[string] defines;
    TList newtokens;

    int i = 0; // Iterate by tokens

    private TList getDefine() {
        TList define_toks = new TList();

        while(tokens[i].type != TokType.tok_semicolon) {
            define_toks.insertBack(tokens[i]);
            i+=1;
        }

        return define_toks;
    }

    private void insertDefine(TList t) {
        for(int j=0; j<t.length; j++) {
            newtokens.insertBack(t[i]);
        }
    }

    this(TList tokens) {
        this.tokens = tokens;
        this.newtokens = new TList();

        while(i<tokens.length) {
            if(tokens[i].type == TokType.tok_cmd) {
                if(tokens[i].cmd == TokCmd.cmd_define) {
                    i+=1;
                    string name = tokens[i].value;
                    i+=1;
                    this.defines[name] = getDefine();
                    i+=1;
                }
                else {
                    newtokens.insertBack(tokens[i]);
                    i+=1;
                }
            }
            else if(tokens[i].type == TokType.tok_id) {
                if(tokens[i].value in defines) {
                    insertDefine(defines[tokens[i].value]);
                    i+=1;
                }
                else {
                    newtokens.insertBack(tokens[i]);
                    i+=1;
                }
            }
            else {
                newtokens.insertBack(tokens[i]);
                i+=1;
            }
        }
    }

    TList getTokens() {
        return newtokens;
    }
}