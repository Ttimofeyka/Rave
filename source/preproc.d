module preproc;

import tokens;
import std.string;
import std.stdio;
import std.array;
import std.path;
import std.conv;
import std.file : readText, isDir, exists, dirEntries, SpanMode;
import lexer;

class Preprocessor {
    TList tokens;
    TList[string] defines;
    TList newtokens;

    int i = 0; // Iterate by tokens

    private void error(string msg) {
        writeln("\033[0;31mError: ", msg, "\033[0;0m");
    }

    private Token get() {
        if(i < tokens.length()) return tokens[i];
        return new Token(SourceLocation(-1, -1, ""), "");
    }

    private TList getDefine() {
        TList define_toks = new TList();

        while(get().type != TokType.tok_semicolon) {
            define_toks.insertBack(get()); 
            i+=1;
        }

        return define_toks;
    }

    private Token copyToken(Token source) {
        alias v = source;
        return new Token(v.loc, v.type, v.cmd, v.value.dup);
    }

    private void insertDefine(TList t) {
        for(int j=0; j<t.length; j++) {
            newtokens.insertBack(copyToken(t[j]));
        }
    }

    private void insertFile(string pattern) {
        if(pattern.isDir) {
            foreach(fname; dirEntries(pattern, SpanMode.breadth)) {
                // writeln("-> ", fname.name);
                if(fname.isDir) continue;
                insertFile(fname.name);
            }
        }
        else {
            auto l = new Lexer(pattern, readText(pattern));
            auto preproc = new Preprocessor(l.getTokens());
            foreach(token; preproc.getTokens().tokens) {
                newtokens.insertBack(copyToken(token));
            }
        }
    }

    this(TList tokens) {
        this.tokens = tokens;
        this.newtokens = new TList();

        while(i<tokens.length) {
            if(get())
            if(get().type == TokType.tok_cmd) {
                if(get().cmd == TokCmd.cmd_define) {
                    i+=1;
                    if(get().type != TokType.tok_id) {
                        error("Expected an identifier after \"def\"!");
                        continue;
                    }
                    string name = get().value;
                    i+=1;
                    this.defines[name] = getDefine();
                    i+=1;
                }
                else if(get().cmd == TokCmd.cmd_include) {
                    i += 1;
                    if(get().type != TokType.tok_string) {
                        error("Expected a string after the \"inc\" keyword!");
                        continue;
                    }
                    string name = get().value.replace("\"", "");
                    i += 1;
                    if(get().type != TokType.tok_semicolon) {
                        error("Expected a semicolon at the end of an \"inc\" statement!");
                        continue;
                    }
                    i += 1;
                    // name = absolutePath(name);
                    // if(exists(name) && isDir(name)) insertFile(buildPath(name, "*"));
                    insertFile(name);
                }
                else if(get().cmd == TokCmd.cmd_ifdef) {
                    i+=1;
                    TList replaced = getDefine();
                    i+=1;
                    // TODO
                }
                else {
                    newtokens.insertBack(get());
                    i+=1;
                }
            }
            else if(get().type == TokType.tok_id) {
                if(get().value in defines) {
                    insertDefine(defines[get().value]);
                    i+=1;
                }
                else {
                    newtokens.insertBack(get());
                    i+=1;
                }
            }
            else {
                newtokens.insertBack(get());
                i+=1;
            }
        }
    }

    TList getTokens() {
        return newtokens;
    }
}