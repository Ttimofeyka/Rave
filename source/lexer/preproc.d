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
import core.stdc.stdlib : exit;
import std.datetime;
import std.conv : to;
import std.system;
import std.random : uniform;

class Preprocessor {
    TList tokens;
    TList[string] defines;
    TList newtokens;
    bool hadErrors = false;

    bool[] _ifStack;
    int i = 0; // Iterate by tokens

    string stdlibIncPath;

    private bool canOutput() {
        // _ifStack: StdList<bool>;
        // canOutput => this->_ifStack.length == 0 || this->_ifStack->get(-1);
        if(_ifStack.length == 0) return true;
        foreach(c; _ifStack) { if(!c) return false; }
        return true;
    }

    private void error(string msg) {
        writeln("\033[0;31mError: ", msg, "\033[0;0m");
        hadErrors = true;
    }

    private string user_error(string msg) {
        return "\033[0;31m"~msg~"\033[0;0m";
    }

    private string user_warn(string msg) {
        return "\033[0;33m"~msg~"\033[0;0m";
    }

    private Token get(int n = 0) {
        if(i < tokens.length()) return tokens[i + n];
        return new Token(SourceLocation(-1, -1, ""), "");
    }

    private TList getDefine() {
        TList define_toks = new TList();

        while(get().type != TokType.tok_at && get(1).cmd != TokCmd.cmd_end) {
            define_toks.insertBack(get()); 
            i+=1;
        }
        i += 1;

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

    private TList getBeforeOper() {
        TList define_toks = new TList();

        while(get().type != TokType.tok_equal && get().type != TokType.tok_nequal) {
            Token currToken = get();
            if(currToken.value in defines) {
                int z = 0;
                while(defines[currToken.value].length < z) {
                    define_toks.insertBack(defines[currToken.value][z]);
                    z += 1;
                }
                i += 1;
                continue;
            }
            define_toks.insertBack(currToken);
            i += 1;
        }

        return define_toks;
    }

    private string analyze_outcmd(int t) {
        string output = "";

        i += 1;

        for(int y=i; y<this.tokens.length; i++) {
            if(this.tokens[i].cmd == TokCmd.cmd_end) break;
            if(this.tokens[i].value in defines) {
                int z = 0;
                while(z<defines[tokens[i].value].length) {
                    output ~= defines[tokens[i].value][z].value~"\"";
                    z += 1;
                }
                i += 1;
            }
            else output ~= this.tokens[i].value;
        }

        if(t == 0) return user_error(output[0..$-1]); // Error
        else if(t == 1) return user_warn(output[0..$-1]); // Warning
        else  return output[0..$-1]; // Just output
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
                   defines["_FILE"] = new TList();
                   defines["_FILE"].insertBack(new Token(SourceLocation(0,0,""), name~".rave"));
                   if(canOutput()) insertFile(name~".rave");
                   defines["_FILE"].tokens = defines["_MFILE"].tokens.dup;
                }
                else if(get().cmd == TokCmd.cmd_ifdef) {
                    i += 1;
                    if(get().type != TokType.tok_id) {
                        error("Expected an identifier after \"@ifdef\"!");
                        continue;
                    }
                    auto name = get().value;
                    i += 1;
                    // writeln("#ifdef " ~ name ~ " -> " ~ (name !in defines ? "not defined" : "defined"));
                    _ifStack ~= !(name !in defines);
                }
                else if(get().cmd == TokCmd.cmd_ifndef) {
                    i += 1;
                    if(get().type != TokType.tok_id) {
                        error("Expected an identifier after \"@ifndef\"!");
                        continue;
                    }
                    auto name = get().value;
                    i += 1;
                    _ifStack ~= name !in defines;
                }
                else if(get().cmd == TokCmd.cmd_end) {
                    i += 1;

                    if(_ifStack.length == 0) {
                        error("Unmatched '@end'!");
                    }
                    else {
                        remove(_ifStack, cast(size_t)(_ifStack.length) - 1);
                    }
                }
                else if(get().cmd == TokCmd.cmd_else) {
                    i += 1;

                    if(_ifStack.length == 0) {
                        error("Unmatched '@else'!");
                    }

                    _ifStack[$-1] = !_ifStack[$-1];
                }
                else if(get().cmd == TokCmd.cmd_protected) {
                    SysTime st = Clock.currTime();
                    string randomname = defines["_FILE"][0].value;

                    randomname ~= to!string(st.day) ~ to!string(st.hour);
                    randomname ~= to!string(os);
                    randomname ~= to!string(st.year) ~ to!string(st.month);
                    randomname ~= to!string(st.second);

                    // Finally randomize
                    randomname ~= to!string(uniform!"[]"(0,555));

                    if(canOutput()) {
                         _ifStack ~= randomname !in defines;
                         defines[randomname] = new TList();
                    }

                    i += 1;
                }
                else if(get().cmd == TokCmd.cmd_undefine) {
                    i += 1;
                    if(get().type != TokType.tok_id) {
                        error("Expected an identifier after \"@undef\"!");
                        continue;
                    }
                    string name = get().value;
                    i += 1;

                    this.defines.remove(name);
                }
                else if(get().cmd == TokCmd.cmd_error) {
                    string to_out = analyze_outcmd(0);
                    if(canOutput()) writeln(to_out);
                    i += 1;
                }
                else if(get().cmd == TokCmd.cmd_warn) {
                    string to_out = analyze_outcmd(1);
                    if(canOutput()) writeln(to_out);
                    i += 1;
                }
                else if(get().cmd == TokCmd.cmd_out) {
                    string to_out = analyze_outcmd(2);
                    if(canOutput()) writeln(to_out);
                    i += 1;
                }
                else if(get().cmd == TokCmd.cmd_ifequ) {
                    i += 1;
                    string v = get().value;
                    if(v in defines) v = defines[v][0].value;
                    i += 1;
                    string v2 = get().value;
                    if(v2 in defines) v = defines[v2][0].value;
                    i += 1;
                    if(canOutput()) _ifStack ~= (v == v2);
                }
                else if(get().cmd == TokCmd.cmd_exit) {
                    if(canOutput()) exit(0);
                    i += 1;
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