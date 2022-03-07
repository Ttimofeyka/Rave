module lexer.preproc;

import lexer.tokens;
import std.string;
import std.stdio;
import std.array;
import std.path;
import std.conv;
import std.algorithm : remove;
import std.file : readText, isDir, exists, dirEntries, SpanMode;
import lexer.mlexer;

class Preprocessor {
    TList tokens;
    TList[string] defines;
    TList newtokens;

    bool[] _ifStack;
    int i = 0; // Iterate by tokens

    string stdlibIncPath;

    private bool canOutput() {
        // _ifStack: StdList<bool>;
        // canOutput => this->_ifStack.length == 0 || this->_ifStack->get(-1);
        return _ifStack.length == 0 || _ifStack[$ - 1];
    }

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
        if(pattern[0] == ':') {
            pattern = buildPath(stdlibIncPath, pattern[1..$]);
        }
        if(pattern.isDir) {
            foreach(fname; dirEntries(pattern, SpanMode.breadth)) {
                // writeln("-> ", fname.name);
                if(fname.isDir) continue;
                insertFile(fname.name);
            }
        }
        else {
            auto l = new Lexer(pattern, readText(pattern));
            auto preproc = new Preprocessor(l.getTokens(), stdlibIncPath, this.defines);
            foreach(token; preproc.getTokens().tokens) {
                newtokens.insertBack(copyToken(token));
            }
        }
    }

    this(TList tokens, string stdlibIncPath, TList[string] defines) {
        this.tokens = tokens;
        this.defines = defines;
        this.newtokens = new TList();
        this.stdlibIncPath = stdlibIncPath;

        while(i < tokens.length) {
            if(get().type == TokType.tok_at) {
                i += 1;
                if(get().type != TokType.tok_cmd) {
                    error("Expected a command after '@'! Got: " ~ get().value ~ "\nAt: " ~ get().loc.toString());
                }
                if(get().cmd == TokCmd.cmd_define) {
                    i += 1;
                    if(get().type != TokType.tok_id) {
                        error("Expected an identifier after \"def\"!");
                        continue;
                    }
                    string name = get().value;
                    i += 1;
                    this.defines[name] = getDefine();
                    i += 1;
                }
                else if(get().cmd == TokCmd.cmd_include) {
                    i += 1;
                    if(get().type != TokType.tok_string) {
                        error("Expected a string after the \"inc\" keyword!");
                        continue;
                    }
                    string name = get().value.replace("\"", "");
                    i += 1;
                    // name = absolutePath(name);
                    // if(exists(name) && isDir(name)) insertFile(buildPath(name, "*"));
                    if(canOutput()) insertFile(name);
                }
                else if(get().cmd == TokCmd.cmd_ifdef) {
                    i += 1;
                    if(get().type != TokType.tok_id) {
                        error("Expected an identifier after \"ifdef\"!");
                        continue;
                    }
                    auto name = get().value;
                    i += 1;
                    // writeln("#ifdef " ~ name ~ " -> " ~ (name !in defines ? "not defined" : "defined"));
                    if(canOutput()) _ifStack ~= !(name !in defines);
                }
                else if(get().cmd == TokCmd.cmd_ifndef) {
                    i += 1;
                    if(get().type != TokType.tok_id) {
                        error("Expected an identifier after \"ifndef\"!");
                        continue;
                    }
                    auto name = get().value;
                    i += 1;
                    if(canOutput()) _ifStack ~= name !in defines;
                }
                else if(get().cmd == TokCmd.cmd_endif) {
                    i += 1;

                    if(canOutput()) {
                        if(_ifStack.length == 0) {
                            error("Unmatched '@endif'!");
                        }
                        else {
                            remove(_ifStack, cast(size_t)(_ifStack.length) - 1);
                        }
                    }
                }
                else if(get().cmd == TokCmd.cmd_else) {
                    i += 1;

                    if(_ifStack.length == 0) {
                        error("Unmatched '@else'!");
                    }

                    _ifStack[$-1] = !_ifStack[$-1];
                }
                else {
                    error("Unknown command: '@" ~ to!string(get().cmd)[4..$] ~ "'!");
                }
            }
            else if(get().type == TokType.tok_id) {
                if(get().value in defines) {
                    if(canOutput()) insertDefine(defines[get().value]);
                    i += 1;
                }
                else {
                    if(canOutput()) newtokens.insertBack(get());
                    i += 1;
                }
            }
            else {
                if(canOutput()) newtokens.insertBack(get());
                i += 1;
            }
        }
    }

    TList getTokens() {
        return newtokens;
    }
}