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
import parser.mparser;
import parser.ast;
import std.string;

class Preprocessor {
    TList tokens;
    TList[string] defines;
    TList newtokens;
    bool hadErrors = false;
    bool[string] _includeFiles;
    TList[string] _macros;
    string[][string] _macrosDefinesNames;
    int idxx = 0;

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
        pragma(inline,true);
        return (i<tokens.length()) ? tokens[i+n] : new Token(SourceLocation(-1,-1,""),"");
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
        return (t == 1) ? user_warn(output[0..$-1]) : output[0..$-1]; // Error && Just output
    }

    private void insertFile(string pattern, bool isInc) {
        if(pattern[0] == ':') {
            pattern = buildPath(stdlibIncPath, pattern[1..$]);
        }
        if(pattern.isDir) {
            foreach(fname; dirEntries(pattern, SpanMode.breadth)) {
                // writeln("-> ", fname.name);
                if(fname.isDir) continue;
                insertFile(fname.name, isInc);
            }
        }
        else {
            if(isInc) {
                if(pattern !in _includeFiles) {
                    _includeFiles[pattern] = true;
                    auto l = new Lexer(pattern, readText(pattern));
                    auto preproc = new Preprocessor(l.getTokens(), stdlibIncPath, this.defines, this._macros, this._includeFiles, this._macrosDefinesNames);
                    foreach(key; byKey(preproc._macros)) {
                        this._macros[key] = preproc._macros[key];
                    }
                    foreach(key; byKey(preproc._includeFiles)) {
                        this._includeFiles[key] = preproc._includeFiles[key];
                    }
                    foreach(key; byKey(preproc._macrosDefinesNames)) {
                        this._macrosDefinesNames[key] = preproc._macrosDefinesNames[key];
                    }
                    foreach(token; preproc.getTokens().tokens) {
                        newtokens.insertBack(copyToken(token));
                    }
                }
            }
            else {
                auto l = new Lexer(pattern, readText(pattern));
                auto preproc = new Preprocessor(l.getTokens(), stdlibIncPath, this.defines, this._macros, this._includeFiles, this._macrosDefinesNames);
                foreach(token; preproc.getTokens().tokens) {
                    newtokens.insertBack(copyToken(token));
                }
            }
        }
    }

    private void prMacro() {
        string mname = get().value;
        i += 1;

        // Next - arguments(defines of values)
        i += 1; // skip (
        while(get().type != TokType.tok_rpar) {
            _macrosDefinesNames[mname] ~= get().value;
            i += 1;
            if(get().type == TokType.tok_comma) i += 1;
            else if(get().type == TokType.tok_multiargs) {
                // =) Multiargs
                _macrosDefinesNames[mname] ~= ("_RaveAA"~get(-1).value);
                i += 1;
            }
        }
        i += 1; // skip )
        // Next - tokens before @endm
        TList toks = new TList();
        while(true) {
            if(get().type == TokType.tok_at) {
                // next - @endm
                i += 1;
                continue;
            }
            if(get().cmd != TokCmd.cmd_endm) {
                toks.insertBack(get());
                i += 1;
            }
            else break;
        }
        i += 1; // skip @endm
        if(canOutput()) _macros[mname] = toks;
    }

    private TList getBeforeComma() {
        TList t = new TList();
        while(get().type != TokType.tok_comma && get().type != TokType.tok_rpar) {
            t.insertBack(get());
            i += 1;
        }
        return t;
    }

    private TList getBeforeEndOfExpr() {
        TList t = new TList();
        idxx += 1;
        while(get().type != TokType.tok_rpar) {
            t.insertBack(get());
            if(get().type == TokType.tok_comma) idxx += 1;
            i += 1;
        }
        return t;
    }

    private void addT(Token t) {
        if(t.type == TokType.tok_id && t.value in defines) {
            for(int z=0; z<defines[t.value].length; z++) {
                addT(defines[t.value][z]);
            }
        }
        else newtokens.insertBack(t);
    }

    this(TList tokens, string stdlibIncPath, TList[string] defines, TList[string] macros, bool[string] iF, string[][string] mN) {
        this.tokens = tokens;
        this.defines = defines;
        this.newtokens = new TList();
        this.stdlibIncPath = stdlibIncPath;
        this._macros = macros;
        this._macrosDefinesNames = mN;
        this._includeFiles = iF;

        int currPr = 0;

        while(i < tokens.length) {
            if(get().type == TokType.tok_at) {
                i += 1;
                if(get().type != TokType.tok_cmd) {
                    error("Expected a command after '@'! Got: " ~ get().value ~ "\nAt: " ~ get().loc.toString());
                }
                if(get().cmd == TokCmd.cmd_define) {
                    i += 1;
                    if(get().type != TokType.tok_id) {
                        error("Expected an identifier after \"@def\"!");
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
                        error("Expected a string after the \"@inc\" keyword!");
                        continue;
                    }
                    string name = get().value.replace("\"", "");
                    i += 1;
                    // name = absolutePath(name);
                    // if(exists(name) && isDir(name)) insertFile(buildPath(name, "*"));
                   defines["_FILE"] = new TList();
                   defines["_FILE"].insertBack(new Token(SourceLocation(0,0,""), name~".rave"));
                   if(canOutput()) insertFile(name~".rave",true);
                   defines["_FILE"].tokens = defines["_MFILE"].tokens.dup;
                }
                else if(get().cmd == TokCmd.cmd_insert) {
                    i += 1;
                    if(get().type != TokType.tok_string) {
                        error("Expected a string after the \"@inc\" keyword!");
                        continue;
                    }
                    string name = get().value.replace("\"", "");
                    i += 1;
                    // name = absolutePath(name);
                    // if(exists(name) && isDir(name)) insertFile(buildPath(name, "*"));
                   defines["_FILE"] = new TList();
                   defines["_FILE"].insertBack(new Token(SourceLocation(0,0,""), name~".rave"));
                   if(canOutput()) insertFile(name~".rave",false);
                   defines["_FILE"].tokens = defines["_MFILE"].tokens.dup;
                }
                else if(get().cmd == TokCmd.cmd_macro) {
                    i += 1;
                    if(!canOutput()) {
                        while(get().cmd != TokCmd.cmd_endm) {i+=1;}
                        i += 1;
                    }
                    else prMacro();
                }
                else if(get().cmd == TokCmd.cmd_import) {
                    i += 1;
                    if(get().type != TokType.tok_string) {
                        error("Expected a string after the \"@imp\" keyword!");
                        continue;
                    }
                    string name = get().value.replace("\"","");
                    i += 1;
                    // Lexing
                    Lexer l = new Lexer(name,readText(name));
                    
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

                    _ifStack ~= !_ifStack[$-1];
                }
                else if(get().cmd == TokCmd.cmd_protected) {
                    string randomname = defines["_FILE"][0].value;
                    randomname ~= to!string(currPr);
                    currPr += 1;

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

                    if(canOutput()) this.defines.remove(name);
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
                    i += 1;
                    int exitcode = to!int(get().value);
                    if(canOutput()) exit(exitcode);
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
                else if(get().value in _macros) {
                    string mname = get().value;
                    i += 2; // skip (
                    int currMVar = 0;
                    int[] toclear;
                    while(get().type != TokType.tok_rpar) {
                        if(_macrosDefinesNames[mname][currMVar].indexOf("_RaveAA") != -1) {
                            // After, all - this multiargs
                            while(get().value != "(") i -= 1;
                            i += 1;
                            TList margs = getBeforeEndOfExpr();
                            defines[_macrosDefinesNames[mname][currMVar].replace("_RaveAA","")] = margs;
                            toclear ~= currMVar;
                            break;
                        }
                        else {
                            defines[_macrosDefinesNames[mname][currMVar]] = getBeforeComma();
                            currMVar += 1;
                            toclear ~= currMVar;
                            if(get().type == TokType.tok_rpar) break;
                            else i += 1;
                        }
                    }
                    defines[mname~"_argslen"] = new TList();
                    defines[mname~"_argslen"].insertBack(new Token(SourceLocation(0,0,""),to!string(idxx)));
                    for(int z=0; z<_macros[mname].length; z++) {
                        if(canOutput()) addT(_macros[mname][z]);
                    }
                    i += 1; // skip )
                    for(int b=0; b<toclear.length; b++) {
                        try {defines.remove(_macrosDefinesNames[mname][toclear[b]]);} catch(Error e) {}
                    }
                    _macrosDefinesNames.clear();
                    defines.remove(mname~"_argslen");
                }
                else {
                    if(canOutput()) addT(get());
                    i += 1;
                }
            }
            else {
                if(canOutput()) addT(get());
                i += 1;
            }
        }
    }

    TList getTokens() {
        return newtokens;
    }
}