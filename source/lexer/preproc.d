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
import parser.util;
import std.path;
import std.algorithm;

struct Macro {
    string[] macro_args;
    TList value;
}

class Preprocessor {
    int _idx = 0;
    TList _list;
    TList _newlist;

    Macro[string] macros;
    TList[string] defines;

    string[] _incFiles;

    string sourceDirName;

    bool[] _ifStack;

    bool canOut() {
        pragma(inline,true);
        return (_ifStack.length>0) ? (!canFind(_ifStack,false)) : true;
    }

    Token peek() {
        pragma(inline,true);
        return _list[_idx];
    }

    void addTList(TList t) {
        for(int i=0; i<t.length; i++) {
            addToken(t[i]);
        }
    }

    void addToken(Token t) {
        if(t.value in defines) {
            for(int i=0; i<defines[t.value].length; i++) {
                addToken(defines[t.value][i]);
            }
        }
        else if(t.value in macros) {
            Macro m = macros[t.value];
            _idx += 2; // skip name and (
            TList[int] v;
            int currv = 0;
            v[currv] = new TList();
            while(peek().type != TokType.tok_rpar) {
                v[currv].insertBack(peek());
                _idx += 1;
                if(peek().type == TokType.tok_comma) {
                    _idx += 1;
                    currv += 1;
                    v[currv] = new TList();
                }
            }
            for(int i=0; i<v.length; i++) {
                defines[m.macro_args[i]] = v[i];
            }
            Preprocessor p = new Preprocessor(m.value,macros,defines,sourceDirName,_incFiles);
            this.defines = p.defines.dup;
            this.macros = p.macros.dup;
            this._incFiles = p._incFiles.dup;
            addTList(p.getTokens());
            for(int i=0; i<v.length; i++) {
                defines.remove(m.macro_args[i]);
            }
        }
        else _newlist.insertBack(t);
    }

    void addDefine() {
        _idx += 1;
        string defname = peek().value;
        _idx += 1;
        TList v = new TList();
        while(peek().type != TokType.tok_cmd && peek().cmd != TokCmd.cmd_end) {
            v.insertBack(peek());
            _idx += 1;
        }
        defines[defname] = v;
    }

    void addMacro() {
        _idx += 1;
        string macroname = peek().value;
        _idx += 1;
        string[] macro_args;
        _idx += 1;
        while(peek().type != TokType.tok_rpar) {
            macro_args ~= peek().value;
            _idx += 1;
            if(peek().type == TokType.tok_comma) _idx += 1;
        }
        _idx += 1;
        TList value = new TList();
        while(peek().cmd != TokCmd.cmd_endm) {
            value.insertBack(peek());
            _idx += 1;
        }
        macros[macroname] = Macro(macro_args.dup,value);
    }

    string getCorrectPath() {
        string p = "";
        if(peek().type == TokType.tok_less) {
            _idx += 1;
            while(peek().type != TokType.tok_more) {
                p ~= peek().value;
                _idx += 1;
            }
            _idx += 1;
            return p;
        }
        p = peek().value;
        _idx += 1;
        return sourceDirName~"/"~(p[1..$-1]);
    }

    void _ins() {
        _idx += 1;
        string p = getCorrectPath()~".rave";
        if(canOut()) {
            Lexer l = new Lexer(p,readText(p));
            Preprocessor pr = new Preprocessor(l.getTokens(),macros,defines,sourceDirName,_incFiles);
            this.defines = pr.defines.dup;
            this.macros = pr.macros.dup;
            this._incFiles = pr._incFiles.dup;
            addTList(pr.getTokens());
        }
    }

    void _inc() {
        _idx += 1;
        string p = getCorrectPath()~".rave";
        if(canOut() && !canFind(_incFiles,p)) {
            _incFiles ~= p;
            Lexer l = new Lexer(p,readText(p));
            Preprocessor pr = new Preprocessor(l.getTokens(),macros,defines,sourceDirName, _incFiles);
            this.defines = pr.defines.dup;
            this.macros = pr.macros.dup;
            this._incFiles = pr._incFiles.dup;
            addTList(pr.getTokens());
        }
    }

    void _imp() {
        _idx += 1;
        string p = getCorrectPath()~".rave";
        if(canOut() && !canFind(_incFiles,p)) {
            _incFiles ~= p;
            Lexer l = new Lexer(p,readText(p));
            Preprocessor pr = new Preprocessor(l.getTokens(),macros,defines,sourceDirName, _incFiles);
            this.defines = pr.defines.dup;
            this.macros = pr.macros.dup;
            this._incFiles = pr._incFiles.dup;
            Parser prsr = new Parser(pr.getTokens());
            int i = 0;
            AstNode[] nodes = prsr.parseProgram();
            AstNode[] newnodes;
            while(i < nodes.length) {
                if(AstNodeFunction f = nodes[i].instanceof!AstNodeFunction) {
                    f.decl.isExtern = true;
                    i += 1;
                    newnodes ~= f;
                }
                else if(AstNodeDecl d = nodes[i].instanceof!AstNodeDecl) {
                    d.decl.isExtern = true;
                    i += 1;
                    newnodes ~= d;
                }
                else {
                    i += 1;
                    newnodes ~= nodes[i];
                }
            }
        }
    }

    void _ifdef() {
        _idx += 1;
        if(canOut()) _ifStack ~= !(peek().value !in defines);
        _idx += 1;
    }

    void _ifndef() {
        _idx += 1;
        if(canOut()) _ifStack ~= !(peek().value !in defines);
        _idx += 1;
    }

    void _do() {
        if(peek().type == TokType.tok_cmd) {
            if(peek().cmd == TokCmd.cmd_define) {
                addDefine();
                _idx += 1;
                return;
            }
            else if(peek().cmd == TokCmd.cmd_macro) {
                addMacro();
                _idx += 1;
                return;
            }
            else if(peek().cmd == TokCmd.cmd_insert) {
                _ins();
                return;
            }
            else if(peek().cmd == TokCmd.cmd_include) {
                _inc();
                return;
            }
            else if(peek().cmd == TokCmd.cmd_import) {
                _imp();
                return;
            }
            else if(peek().cmd == TokCmd.cmd_ifdef) {
                _ifdef();
                return;
            }
            else if(peek().cmd == TokCmd.cmd_ifndef) {
                _ifndef();
                return;
            }
            else if(peek().cmd == TokCmd.cmd_undefine) {
                _idx += 1;
                defines.remove(peek().value);
                _idx += 1;
                return;
            }
            else if(peek().cmd == TokCmd.cmd_exit) {
                _idx += 1;
                exit(to!int(peek().value));
            }
            else if(peek().cmd == TokCmd.cmd_end) {
                if(canOut() && _ifStack.length>0) _ifStack = _ifStack[0..$-1];
                _idx += 1;
                return;
            }
        }
        addToken(peek());
        _idx += 1;
    }

    this(TList _list, string sourceDirName) {
        this._list = _list;
        this._newlist = new TList();
        this.sourceDirName = sourceDirName;
        while(_idx<_list.length) _do();
    }

    this(TList _list, Macro[string] macros, TList[string] defines, string sourceDirName, string[] _incFiles) {
        this.macros = macros;
        this.defines = defines;
        this._newlist = new TList();
        this.sourceDirName = sourceDirName;
        this._incFiles = _incFiles;
        this._list = _list;
        while(_idx<_list.length) _do();
    }

    TList getTokens() {
        return this._newlist;
    }
}