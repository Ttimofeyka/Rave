module parser.parser;

import std.string;
import std.array;
import std.conv : to;
import lexer.tokens;
import core.stdc.stdlib : exit;
import std.stdio : writeln;
import parser.types;
import parser.ast;
import std.container : SList;
import app : files;
import std.conv : parse;
import llvm.types;

T instanceof(T)(Object o) if(is(T == class)) {
	return cast(T) o;
}

struct DeclMod {
    string name;
    string value;
}

class MultiNode : Node {
    Node[] nodes;

    this(Node[] nodes) {
        this.nodes = nodes.dup;
    }

    override void check() {
        for(int i=0; i<nodes.length; i++) {
            nodes[i].check();
        }
    }

    override LLVMValueRef generate() {
        for(int i=0; i<nodes.length; i++) {
            nodes[i].generate();
        }
        return null;
    }
}

class Parser {
    Token[] tokens;
    int _idx = 0;
    Node[] _nodes;
    string[] structs;
    bool[string] _imported;
    Type[string] _aliasTypes;
    string currentFile = "";

    private void error(string msg) {
        pragma(inline,true);
		writeln("\033[0;31mError: " ~ msg ~ "\033[0;0m");
		exit(1);
	}

    private Token peek() {
        pragma(inline,true);
        return tokens[_idx];
    }

    private Token next() {
        pragma(inline,true);
        return tokens[_idx++];
    }

    private Token expect(TokType t, int line = -1) {
        pragma(inline,true);
        if(peek().type != t) {
            error("Expected "~to!string(t)~" on "~to!string(peek().line+1)~" line!");
        }
        return next();
    }

    private Type parseTypeAtom() {
        if(peek().type == TokType.Identifier) {
            auto t = next();
            if(t.value == "void") return new TypeVoid();
            else if(t.value.into(_aliasTypes)) return _aliasTypes[t.value];
            return getType(t.value);
        }
        else {
            error("Expected a typename on "~to!string(peek().line+1)~" line!");
        }
        return null;
    }

    Type[] getFuncTypeArgs() {
        Type[] buff;

        while(peek().type != TokType.Lpar) {
            buff ~= parseType();
            if(peek().type == TokType.Comma) next();
        }

        next();

        return buff.dup;
    }

    Type parseType() {
        auto t = parseTypeAtom();
        while(peek().type == TokType.Multiply || peek().type == TokType.Rarr || peek().type == TokType.Rpar) {
            if(peek().type == TokType.Multiply) {
                next();
                t = new TypePointer(t);
            }
            else if(peek().type == TokType.Rarr) {
                next();
                int num = 0;
                if(peek().type == TokType.Number) {
                    num = to!int(peek().value);
                }
                next();
                expect(TokType.Larr);
                t = new TypeArray(num,t);
            }
            else if(peek().type == TokType.Rpar) {
                next();
                Type[] types = getFuncTypeArgs().dup;
                t = new TypeFunc(t,types.dup);
            }
        }
        return t;
    }

    FuncArgSet[] parseFuncArgSets() {
        FuncArgSet[] args;

        if(peek().type != TokType.Rpar) return [];
        next();

        while(peek().type != TokType.Lpar) {
            if(peek().type == TokType.VarArg) {
                args ~= FuncArgSet("...",new TypeVarArg()); _idx += 1;
            }
            else {
                auto type = parseType();
                auto name = expect(TokType.Identifier).value;

                args ~= FuncArgSet(name,type);
            }
            if(peek().type == TokType.Lpar) {next(); break;}
            expect(TokType.Comma);
        }

        return args.dup;
    }

    Node[] parseFuncCallArgs() {
        next(); // skip (
        Node[] args;
        while(peek().type != TokType.Lpar) {
            args ~= parseExpr();
            if(peek().type == TokType.Comma) next();
        }
        next(); // skip )
        return args;
    }

    Node parseCall(Node func) {
        return new NodeCall(peek().line,func,parseFuncCallArgs());
    }

    Node[] parseIndexs() {
        Node[] indexs;

        while(peek().type == TokType.Rarr) {
            next();
            indexs ~= parseExpr();
            next();
        }

        return indexs.dup;
    }

    Node parseSuffix(Node base) {
        while(peek().type == TokType.Rpar
             || peek().type == TokType.Rarr
             || peek().type == TokType.Dot
        ) {
            if(peek().type == TokType.Rpar) base = parseCall(base);
            else if(peek().type == TokType.Rarr) {
                base = new NodeIndex(base,parseIndexs(),peek().line);
            }
            else if(peek().type == TokType.Dot) {
                next();
                string field = expect(TokType.Identifier).value;
                base = new NodeGet(base,field,peek().type == TokType.Equ,peek().line);
            }
        }

        return base;
    }

    Node parseAtom() {
        import std.algorithm : canFind;
        auto t = next();
        if(t.type == TokType.Number) return new NodeInt(to!ulong(t.value));
        if(t.type == TokType.FloatNumber) return new NodeFloat(to!double(t.value));
        if(t.type == TokType.HexNumber) return new NodeInt(parse!ulong(t.value,16));
        if(t.type == TokType.True) return new NodeBool(true);
        if(t.type == TokType.False) return new NodeBool(false);
        if(t.type == TokType.String) return new NodeString(t.value);
        if(t.type == TokType.Char) return new NodeChar(to!char(t.value));
        if(t.type == TokType.Identifier) {
            if(t.value == "cast") {
                next();
                Type ty = parseType();
                next(); // skip )
                Node expr = parseExpr();
                return new NodeCast(ty,expr,t.line);
            }
            else if(t.value == "typeof") {
                next();
                Node val = parseExpr();
                next(); //skip )
                return new NodeTypeof(val,t.line);
            }
            else if(t.value == "sizeof") {
                next();
                NodeType val = new NodeType(parseType(),t.line);
                next();
                return new NodeSizeof(val,t.line);
            }
            else if(t.value == "itop") {
                next();
                Node val = parseExpr();
                next();
                return new NodeItop(val,t.line);
            }
            else if(t.value == "ptoi") {
                next();
                Node val = parseExpr();
                next();
                return new NodePtoi(val,t.line);
            }
            return new NodeIden(t.value,peek().line);
        }
        if(t.type == TokType.MacroArgNum) {
            return new MacroGetArg(to!int(t.value), t.line);
        }
        if(t.type == TokType.Null) return new NodeNull();
        if(t.type == TokType.Rpar) {
            auto e = parseExpr();
            expect(TokType.Lpar);
            return e;
        }
        if(t.type == TokType.Rarr) {
            return null; // TODO: CONST ARRAY
        }
        error("Expected a number, true/false, char, variable or expression. Got: "~to!string(t.type)~" on "~to!string(peek().line+1)~" line.");
        return null;
    }

    Node parsePrefix() {
        import std.algorithm : canFind;
        const TokType[] operators = [
            TokType.Multiply,
            TokType.Minus,
            TokType.GetPtr,
            TokType.Ne
        ];

        if(operators.canFind(peek().type)) {
            auto tok = next();
            return new NodeUnary(tok.line, tok.type, parsePrefix());
        }
        return parseAtom();
    }

    Node parseBasic() {
        return parseSuffix(parsePrefix());
    }

    Node parseExpr() {
        const int[TokType] operators = [
            TokType.Plus: 0,
            TokType.Minus: 0,
            TokType.Multiply: 1,
            TokType.Divide: 1,
            TokType.Equ: -95,
            TokType.PluEqu: -97,
            TokType.MinEqu: -97,
            TokType.DivEqu: -97,
            TokType.MinEqu: -97,
            TokType.Equal: -80,
            TokType.Nequal: -80,
            TokType.Less: -70,
            TokType.More: -70,
            TokType.Or: -50,
            TokType.And: -50,
            TokType.BitLeft: -51,
            TokType.BitRight: -51
        ];

        SList!Token operatorStack;
		SList!Node nodeStack;
		uint nodeStackSize = 0;
		uint operatorStackSize = 0;

		nodeStack.insertFront(parseBasic());
		nodeStackSize += 1;

        while(peek().type in operators)
		{
			if(operatorStack.empty) {
				operatorStack.insertFront(next());
				operatorStackSize += 1;
			}
			else {
				auto tok = next();
				auto t = tok.type;
				int prec = operators[t];
				while(operatorStackSize > 0 && prec <= operators[operatorStack.front.type]) {
					// push the operator onto the nodeStack
					assert(nodeStackSize >= 2);

					auto lhs = nodeStack.front(); nodeStack.removeFront();
					auto rhs = nodeStack.front(); nodeStack.removeFront();

					nodeStack.insertFront(new NodeBinary(operatorStack.front.type,lhs,rhs,tok.line));
					nodeStackSize -= 1;

					operatorStack.removeFront();
					operatorStackSize -= 1;
				}

				operatorStack.insertFront(tok);
				operatorStackSize += 1;
			}

			nodeStack.insertFront(parseBasic());
			nodeStackSize += 1;
		}

        // push the remaining operator onto the nodeStack
		while(!operatorStack.empty()) {
			assert(nodeStackSize >= 2);

			auto rhs = nodeStack.front(); nodeStack.removeFront();
			auto lhs = nodeStack.front(); nodeStack.removeFront();

			nodeStack.insertFront(new NodeBinary(
				operatorStack.front.type, lhs, rhs,
				operatorStack.front.line));
			nodeStackSize -= 1;

			operatorStack.removeFront();
			operatorStackSize -= 1;
		}

        return nodeStack.front();
    }

    Node parseIf(string f = "", bool isStatic = false) {
        assert(peek().value == "if");
        int line = peek().line;
        next();
        expect(TokType.Rpar);
        auto cond = parseExpr();
        expect(TokType.Lpar);
        auto body_ = parseStmt(f);
        if(peek().value == "else") {
            next();
            auto othr = parseStmt();
            return new NodeIf(cond, body_, othr, line, f, isStatic);
        }
        return new NodeIf(cond, body_, null, line, f, isStatic);
    }

    Node parseWhile(string f = "") {
        int line = peek().line;
        next();

        Node condition;

        if(peek().type == TokType.Rbra) {
            condition = new NodeBool(true);
        }
        else {
            next();
            condition = parseExpr();
            expect(TokType.Lpar);
        }

        auto body_ = parseStmt(f);
        return new NodeWhile(condition, body_, line, f);
    }

    Node parseBreak() {
        next();
        Node _break = new NodeBreak(peek().line);
        expect(TokType.Semicolon);
        return _break;
    }

    Node parseStmt(string f = "") {
        import std.algorithm : canFind;
        bool isStatic = false;
        if(peek().value == "static") {
            isStatic = true;
            next();
        }
        if(peek().type == TokType.Rbra) {
            return parseBlock(f);
        }
        else if(peek().type == TokType.Semicolon) {
            next();
            return parseStmt(f);
        }
        else if(peek().type == TokType.Command) {
            if(peek().value == "if") return parseIf(f,isStatic);
            if(peek().value == "while") return parseWhile(f);
            if(peek().value == "break") return parseBreak();
            if(peek().value == "return") {
                auto tok = next();
                if(peek().type != TokType.Semicolon) {
                    auto e = parseExpr();
                    expect(TokType.Semicolon);
                    return new NodeRet(e,tok.line,f);
                }
                else {
                    next();
                    return new NodeRet(null,tok.line,f);
                }
            }
            if(peek().value == "extern") return parseDecl();
        }
        else if(peek().type == TokType.Identifier) {
                string iden = peek().value;
                if(iden == "debug") return parseDebug();
                _idx++;
                if(peek().type != TokType.Identifier) {
                    if(peek().type == TokType.Multiply)  {
                        _idx -= 1;
                        return parseDecl(f);
                    }
                    else if(peek().type == TokType.Rarr) {
                        switch(iden) {
                            case "void":
                            case "bool":
                            case "int":
                            case "short":
                            case "char":
                            case "cent":
                                _idx -= 1;
                                return parseDecl(f);
                            default:
                                if(iden.into(_aliasTypes)) {
                                    _idx -= 1;
                                    return parseDecl(f);
                                }
                                break;
                        }
                        if(structs.canFind(iden)) {
                            _idx -= 1;
                            return parseDecl(f);
                        }
                    }
                    else if(peek().type == TokType.Rpar) {
                        switch(iden) {
                            case "void":
                            case "bool":
                            case "int":
                            case "short":
                            case "char":
                            case "cent":
                                _idx -= 1;
                                return parseDecl(f);
                            default: break;
                        }
                        if(structs.canFind(iden)) {
                            _idx -= 1;
                            return parseDecl(f);
                        }
                    }
                    _idx -= 1;
                    auto e = parseExpr();
                    expect(TokType.Semicolon);
                    return e;
                }
                _idx -= 1;
                return parseDecl(f);
        }
        else if(peek().type == TokType.Eof) return null;
        auto e = parseExpr();
        expect(TokType.Semicolon);
        return e;
    }

    NodeBlock parseBlock(string f = "") {
        Node[] nodes;
        if(peek().type == TokType.Rbra) next();
        while(peek().type != TokType.Lbra && peek().type != TokType.Eof) nodes ~= parseStmt(f);
        if(peek().type != TokType.Eof) next();
        return new NodeBlock(nodes.dup);
    }

    Node parseAliasType() {
        int l = peek().line;
        next();
        string name = peek().value;
        next();
        expect(TokType.Equ);
        Type childType = parseType();
        if(peek().type == TokType.Semicolon) next();
        _aliasTypes[name] = childType;
        return new NodeNone();
    }

    Node parseMultiAlias(string s, DeclMod[] mods , int loc) {
        Node[] nodes;

        // Current token - alias
        next(); // Skip alias
        string n = (peek().value == "{" ? "" : peek().value);
        next(); if(peek().value == "{") next();

        int i = _idx;
        while(i<tokens.length) {
            if(peek().value == "}") break;
            string name = peek().value;
            if(n != "") name = n~"::"~name;
            next(); next(); // Skip name and =
            Node expr = parseExpr();
            if(peek().type == TokType.Semicolon) next();
            nodes ~= new NodeVar(name,expr,false,false,(s == ""), mods, loc, new TypeAlias());
        }
        if(peek().value == "}") next();
        return new MultiNode(nodes.dup);
    }

    Node parseDecl(string s = "") {
        DeclMod[] mods;
        int loc;
        string name;
        bool isExtern = false;
        bool isConst = false;

        if(peek().value == "extern") {
            isExtern = true;
            next();
        }

        if(peek().value == "const") {
            isConst = true;
            next();
        }

        if(peek().value == "alias") {
            if(tokens[_idx+1].value == "{" || tokens[_idx+2].value == "{") return parseMultiAlias(s,mods.dup,loc);
        }

        if(peek().type == TokType.Rpar) {
            // Decl have mods
            next();
            while(peek().type != TokType.Lpar) {
                auto nam = expect(TokType.Identifier).value;
                string value = "";
                if(peek().type == TokType.ValSel) {
                    next();
                    value = peek().value;
                    next();
                }
                mods ~= DeclMod(nam,value);
                if(peek().type == TokType.Comma) next();
                else if(peek().type == TokType.Lpar) break;
            }
            next(); // Skip lpar
        }

        auto type = parseType();
        loc = peek().line;
        name = peek().value;
        next();

        if(peek().type == TokType.Rpar) {
            // Function with args
            FuncArgSet[] args = parseFuncArgSets();
            if(peek().type == TokType.Lpar) next();
            if(peek().type == TokType.Rbra) next();
            if(peek().type == TokType.ShortRet) {
                next();
                Node expr = parseExpr();
                expect(TokType.Semicolon);
                return new NodeFunc(name,args,new NodeBlock([new NodeRet(expr,loc,name)]),isExtern,mods,loc,type);
            }
            else if(peek().type == TokType.Semicolon) {
                next();
                return new NodeFunc(name,args,new NodeBlock([]),isExtern,mods,loc,type);
            }
            // {block}
            NodeBlock block = parseBlock(name);
            if(peek().type == TokType.ShortRet) {
                next();
                Node n = parseExpr();
                if(peek().type == TokType.Semicolon) next();
                block.nodes ~= new NodeRet(n,peek().line,name);
            }
            return new NodeFunc(name,args,block,isExtern,mods,loc,type);
        }
        else if(peek().type == TokType.Rbra) {
            NodeBlock block = parseBlock(name);
            if(peek().type == TokType.ShortRet) {
                next();
                Node n = parseExpr();
                if(peek().type == TokType.Semicolon) next();
                block.nodes ~= new NodeRet(n,peek().line,name);
            }
            return new NodeFunc(name,[],block,isExtern,mods,loc,type);
        }
        else if(peek().type == TokType.Semicolon) {
            // Var without value
            next();
            return new NodeVar(name,null,isExtern,isConst,(s==""),mods,loc,type);
        }
        else if(peek().type == TokType.ShortRet) {
            next();
            Node expr = parseExpr();
            expect(TokType.Semicolon);
            return new NodeFunc(name,[],new NodeBlock([new NodeRet(expr,loc,name)]),isExtern,mods,loc,type);
        }
        else if(peek().type == TokType.Equ) {
            // Var with value
            next();
            auto e = parseExpr();
            expect(TokType.Semicolon);
            return new NodeVar(name,e,isExtern,isConst,(s==""),mods,loc,type);
        }
        return new NodeFunc(name,[],parseBlock(name), isExtern, mods, loc, type);
    }

    Node parseNamespace() {
        int loc = peek().line;
        next();
        string name = peek().value;
        next(); // current token = {
        expect(TokType.Rbra); // skip {
        Node[] nodes;
        Node currNode;
        while((currNode = parseTopLevel()) !is null) nodes ~= currNode;
        expect(TokType.Lbra); // skip }
        return new NodeNamespace(name,nodes,loc);
    }

    string getGlobalFile() {
        next();
        string buffer = "";

        while(peek().type != TokType.More) {
            buffer ~= peek().value;
            next();
        }

        return buffer;
    }

    string MainFile = "";

    Node parseImport() {
        import lexer.lexer, std.file, std.path, std.algorithm : canFind;
        next();
        string _file;

        if(peek().type == TokType.Less) {
            // Global
            _file = getGlobalFile()~".rave";
        }
        else {
            // Local
            _file = dirName(MainFile)~"/"~peek().value~".rave";
        }
        next();

        if(peek().type == TokType.Semicolon) next();

        if(_file == currentFile) return new NodeNone();

        if(!(_file !in _imported)) return new NodeNone();
        Lexer l = new Lexer(readText(_file));
        Parser p = new Parser(l.getTokens().dup);
        p._imported = _imported.dup;
        p.MainFile = MainFile;
        p.parseAll();
        Node[] nodes = p.getNodes();
        for(int i=0; i<nodes.length; i++) {
            if(NodeVar v = nodes[i].instanceof!NodeVar) {
                v.isExtern = true;
            }
            else if(NodeFunc f = nodes[i].instanceof!NodeFunc) {
                f.isExtern = true;
            }
            else if(NodeNamespace n = nodes[i].instanceof!NodeNamespace) {
                n.isImport = true;
            }
            else if(NodeStruct s = nodes[i].instanceof!NodeStruct) {
                s.isImported = true;
            }
        }
        this._nodes ~= nodes.dup;
        if(!files.canFind(_file)) files ~= _file;
        _imported[_file] = true;
        foreach(key; byKey(p._imported)) {
            _imported[key] = p._imported[key];
        }
        foreach(key; byKey(p._aliasTypes)) {
            _aliasTypes[key] = p._aliasTypes[key];
        }
        return new NodeNone();
    }

    Node parseStruct() {
        int loc = peek().line;
        next();
        string name = peek().value;
        next(); // current token = {
        expect(TokType.Rbra); // skip {
        Node[] nodes;
        Node currNode;
        while((currNode = parseTopLevel(name)) !is null) nodes ~= currNode;
        expect(TokType.Lbra); // skip }
        structs ~= name;
        return new NodeStruct(name,nodes,loc);
    }
    
    Node parseMacro() {
        next();
        string m = peek().value;
        int line = peek().line;
        next();
        string[] args;
        if(peek().type == TokType.Rpar) {
            next();
            while(peek().type != TokType.Lpar) {
                args ~= peek().value;
                next();
                if(peek().type == TokType.Comma) next();
            }
            next();
        }

        NodeBlock block = parseBlock(m);

        return new NodeMacro(block, m, args.dup, line);
    }

    Node parseUsing() {
        next();
        string namespace = peek().value;
        int loc = peek().line;
        next();
        expect(TokType.Semicolon,loc);
        return new NodeUsing(namespace,loc);
    }

    Node parseDebug(string f = "") {
        // Current value - "debug"
        next();
        string name = "";

        // debug(name) {} or debug name {}

        if(peek().type == TokType.Identifier) {
            name = peek().value; next();
        }
        else {
            next(); name = peek().value; next(); next();
        }

        NodeBlock b = parseBlock(f);

        return new NodeDebug(name,b);
    }

    Node parseTopLevel(string s = "") {
        while(peek().type == TokType.Semicolon) {
            next();
        }

        if(peek().type == TokType.Eof) return null;

        if(peek().value == "}") return null;
        
        if(peek().value == "namespace") return parseNamespace();

        if(peek().value == "struct") return parseStruct();

        if(peek().value == "macro") return parseMacro();

        if(peek().value == "using") return parseUsing();

        if(peek().value == "import") {
            return parseImport();
        }

        if(peek().value == "debug") return parseDebug();

        if(peek().value == "type") return parseAliasType();
        
        return parseDecl(s);
    }

    void parseAll() {
        while(peek().type != TokType.Eof) {
            auto n = parseTopLevel();
            if(n is null) break;
            if(!(n.instanceof!NodeNone)) _nodes ~= n;
        }
    }

    Node[] getNodes() {
        return this._nodes.dup;
    }
    
    this(Token[] t) {this.tokens = t;}
}