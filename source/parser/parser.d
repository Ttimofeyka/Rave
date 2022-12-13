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

static void removeAt(T)(ref T[] arr, size_t index)
{
    foreach (i, ref item; arr[index .. $ - 1])
        item = arr[i + 1];
    arr = arr[0 .. $ - 1];
    arr.assumeSafeAppend();
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
        pragma(inline,true);
        for(int i=0; i<nodes.length; i++) {
            nodes[i].check();
        }
    }

    override LLVMValueRef generate() {
        pragma(inline,true);
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
    string[] _templateNames;
    string[] _templateNamesF;
    bool[string] _imported;
    Type[string] _aliasTypes;
    string currentFile = "";
    Node[string] _aliasPredefines;
    int offset;

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
        import std.algorithm : canFind;
        
        if(peek().type == TokType.Identifier) {
            auto t = next();
            if(t.value == "void") return new TypeVoid();
            else if(t.value.into(_aliasTypes)) return _aliasTypes[t.value];
            else if(_templateNames.canFind(t.value)) return new TypeStruct(t.value);
            return getType(t.value);
        }
        else if(peek().type == TokType.MacroArgNum) {
            next();
            return new TypeMacroArg(to!int(tokens[_idx-1].value));
        }
        else if(peek().type == TokType.Builtin) {
            string name = peek().value;
            int loc = peek().line;
            next();
            Node[] args;
            next();
            while(peek().type != TokType.Lpar) {
                if(peek().type == TokType.MacroArgNum) {
                    args ~= new MacroGetArg(to!int(peek().value),loc);
                    next(); 
                }
                else args ~= parseExpr();
                if(peek().type == TokType.Comma) next();
            }
            next();
            return new TypeBuiltin(name,args);
        }
        else {
            error("Expected a typename on "~to!string(peek().line+1)~" line!");
        }
        return null;
    }

    TypeFuncArg[] getFuncTypeArgs() {
        TypeFuncArg[] buff;

        while(peek().type != TokType.Lpar) {
            Type t = parseType();
            string name = "";
            if(peek().type == TokType.Identifier) {name = peek().value; next();}
            buff ~= new TypeFuncArg(t,name);
            if(peek().type == TokType.Comma) next();
        }

        next();

        return buff.dup;
    }

    Type parseType() {
        auto t = parseTypeAtom();
        while(peek().type == TokType.Multiply || peek().type == TokType.Rarr || peek().type == TokType.Rpar || peek().type == TokType.Less) {
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
                TypeFuncArg[] types = getFuncTypeArgs().dup;
                t = new TypeFunc(t,types.dup);
            }
            else if(peek().type == TokType.Less) {
                // Template
                string name = t.toString();
                next();

                Type[] _types;

                while(peek().type != TokType.More) {
                    _types ~= parseType();
                    if(peek().type == TokType.Comma) next();
                }
                next();

                string _sTypes = "";
                for(int i=0; i<_types.length; i++) {
                    _sTypes ~= _types[i].toString();
                }

                t = new TypeStruct(name~"<"~_sTypes~">",_types);
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
        import std.algorithm : canFind;
        if(peek().type == TokType.Rpar) next(); // skip (
        Node[] args;
        while(peek().type != TokType.Lpar) {
            switch(peek().value) {
                case "int":
                case "short":
                case "long":
                case "cent":
                case "char":
                case "bool":
                    args ~= new NodeType(parseType(),peek().line);
                    break;
                default:
                    if(canFind(structs,peek().value) || canFind(_templateNames,peek().value)) args ~= new NodeType(parseType(),peek().line);
                    else {args ~= parseExpr();}
                    break;
            }
            if(peek().type == TokType.Comma) next();
        }
        next(); // skip )
        return args;
    }

    Node parseCall(Node func) {
        pragma(inline,true);
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

    long lambdas = 0;

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
            else if(t.value == "void" || t.value == "bool" || t.value == "char" || t.value == "short" || t.value == "int" || t.value == "long" || t.value == "cent") {
                // Lambda
                _idx -= 1;
                int loc = peek().line;
                Type tt = parseType();
                lambdas += 1;
                if(peek().type == TokType.ShortRet) {
                    next(); Node val = parseExpr();
                    return new NodeLambda(loc,tt.instanceof!TypeFunc,new NodeBlock([new NodeRet(val,loc,"lambda"~to!string(lambdas-1))]));
                }
                NodeBlock b = parseBlock("lambda"~to!string(lambdas));
                if(peek().type == TokType.ShortRet) {
                    next(); Node val = parseExpr();
                    b.nodes ~= new NodeRet(val,loc,"lambda"~to!string(lambdas-1));
                }
                //expect(TokType.Semicolon,peek().line);
                return new NodeLambda(loc,tt.instanceof!TypeFunc,b);
            }
            else if(peek().type == TokType.Less) {
                bool isBasicType(string s) {
                    return (s == "bool") || (s == "char") || (s == "short")
                    || (s == "void") || (s == "int") || (s == "cent") || (s == "long");
                }
                if(isBasicType(tokens[_idx+1].value) || tokens[_idx+2].type == TokType.Less || tokens[_idx+2].type == TokType.More || tokens[_idx+2].type == TokType.Multiply || tokens[_idx+2].type == TokType.Rarr) {
                    next();
                    string all = t.value~"<";

                    int countOfL = 0;
                    while(countOfL != -1) {
                        all ~= peek().value;

                        if(peek().type == TokType.Less) countOfL += 1;
                        else if(peek().type == TokType.More) countOfL -= 1;

                        next();
                    }
                    if(peek().type == TokType.More) next();
                    return parseCall(new NodeIden(all,peek().line));
                }
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
            Node[] values;
            while(peek().type != TokType.Larr) {
                values ~= parseExpr();
                if(peek().type == TokType.Comma) next();
            }
            next();
            return new NodeArray(t.line,values);
        }
        if(t.type == TokType.Builtin) {
            next();
            string name = t.value;

            Node[] args;
            while(peek().type != TokType.Lpar) {
                if(canFind(structs,peek().value) || peek().value == "int" || peek().value == "short" || peek().value == "long" ||
                   peek().value == "bool" || peek().value == "char" || peek().value == "cent" || canFind(_templateNames,peek().value)) {
                    if(tokens[_idx+1].type == TokType.Identifier || tokens[_idx+1].type == TokType.Rpar || tokens[_idx+1].type == TokType.ShortRet) {
                        args ~= parseLambda();
                    }
                    else args ~= new NodeType(parseType(),peek().line);
                }
                else args ~= parseExpr();
                if(peek().type == TokType.Comma) next();
            }
            next();
            
            return new NodeBuiltin(name,args.dup,t.line);
        }
        error("Expected a number, true/false, char, variable or expression. Got: "~to!string(t.type)~" on "~to!string(peek().line+1)~" line.");
        return null;
    }

    Node parsePrefix() {
        import std.algorithm : canFind;
        const TokType[] operators = [
            TokType.GetPtr,
            TokType.Multiply,
            TokType.Minus,
            TokType.Ne,
            TokType.Destructor,
        ];

        if(operators.canFind(peek().type)) {
            auto tok = next();
            auto tok2 = next();
            auto tok3 = next();
            if(tok3.value == "[" || tok3.value == ".") {
                _idx -= 2;
                return new NodeUnary(-1,tok.type,parseSuffix(parseAtom()));
            }
            else _idx -= 2;
            return new NodeUnary(tok.line, tok.type, parsePrefix());
        }
        return parseAtom();
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
                bool isPtr = (peek().type == TokType.GetPtr);
                if(isPtr) next();
                string field = expect(TokType.Identifier).value;
                if(peek().type == TokType.Rpar) {
                    base = parseCall(new NodeGet(base,field,(isPtr),peek().line));
                }
                else base = new NodeGet(base,field,(peek().type == TokType.Equ || isPtr),peek().line);
            }
        }

        return base;
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
            TokType.Rem: -50,
            TokType.Equ: -95,
            TokType.PluEqu: -97,
            TokType.MinEqu: -97,
            TokType.DivEqu: -97,
            TokType.MinEqu: -97,
            TokType.Equal: -80,
            TokType.Nequal: -80,
            TokType.Less: -70,
            TokType.More: -70,
            TokType.Or: -85,
            TokType.And: -85,
            TokType.BitLeft: -51,
            TokType.BitRight: -51,
            TokType.MoreEqual: -70,
            TokType.LessEqual: -70,
            TokType.BitXor: -51,
            TokType.BitNot: -51
        ];

        SList!Token operatorStack;
		SList!Node nodeStack;
		uint nodeStackSize = 0;
		uint operatorStackSize = 0;

		nodeStack.insertFront(parseBasic());
		nodeStackSize += 1;

        while(peek().type.into(operators))
		{
			if(operatorStack.empty) {
				operatorStack.insertFront(next());
				operatorStackSize += 1;
			}
			else {
				auto tok = next();
				auto t = tok.type;
				int prec = operators[t];
                //if(operatorStackSize > 0) writeln(operatorStack.front().type);
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

    Node parseFor(string f = "") {
        import std.algorithm : canFind;
        // Current token - for
        next();
        next(); // Skip (

        NodeVar var;
        NodeBinary[] condAndAfter;

        while(peek().type != TokType.Lpar) {
            if(peek().value == "bool" || peek().value == "short" || peek().value == "int" || peek().value == "long" || peek().value == "cent" || canFind(structs,peek().value) || canFind(_templateNames,peek().value)) {
                // Var
                Type t = parseType();
                string name = peek().value;
                Node value;
                next();
                if(peek().value == "=") {
                    next();
                    value = parseExpr();
                }
                var = new NodeVar(name,value,false,false,false,[],peek().line,t);
            }
            else {
                condAndAfter ~= parseExpr().instanceof!NodeBinary;
            }

            if(peek().value == ";") next();
        }
        next();

        return new NodeFor(var,condAndAfter,parseBlock(f),f,peek().line);
    }

    Node parseWith(string f = "") {
        import std.algorithm : canFind;

        // Current token - "with"
        next();
        
        Node _structure;
        // Current token - "("
        next();
        if(canFind(structs,peek().value) || canFind(_templateNames,peek().value)) { // Decl
            Type t = parseType();
            string name = peek().value;

            next();

            Node val = null;
            if(peek().value == "=") {
                next();
                val = parseExpr();
            }

            _structure = new NodeVar(name,val,false,false,false,[],peek().line,t);
        }
        else _structure = parseExpr();

        if(peek().value == ")") next();

        return new NodeWith(peek().line, _structure, parseBlock(f));
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
        else if(peek().type == TokType.Identifier && tokens[_idx+1].type == TokType.Less) {
            return parseDecl(f);
        }
        else if(peek().type == TokType.Command) {
            if(peek().value == "if") return parseIf(f,isStatic);
            if(peek().value == "while") return parseWhile(f);
            if(peek().value == "for") return parseFor(f);
            if(peek().value == "break") return parseBreak();
            if(peek().value == "with") return parseWith(f);
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
                if(iden == "volatile") return parseDecl(f);
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
                            case "volatile":
                                _idx -= 1;
                                return parseDecl(f);
                            default:
                                if(iden.into(_aliasTypes)) {
                                    _idx -= 1;
                                    return parseDecl(f);
                                }
                                break;
                        }
                        if(structs.canFind(iden) || _templateNames.canFind(iden) || _templateNamesF.canFind(iden)) {
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
                            case "volatile":
                                _idx -= 1;
                                return parseDecl(f);
                            default: break;
                        }
                        if(structs.canFind(iden) || _templateNames.canFind(iden) || _templateNamesF.canFind(iden)) {
                            _idx -= 1;
                            return parseDecl(f);
                        }
                        Node e = parseCall(new NodeIden(iden,peek().line));
                        if(peek().type == TokType.Semicolon) next();
                        return e;
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

    Node parseLambda() {
        int loc = peek().line;
        Type tt = parseType();
        lambdas += 1;
        string name = "";
        if(peek().type == TokType.Identifier) {
            name = peek().value;
            next();
        }
        if(peek().type == TokType.ShortRet) {
            next(); Node val = parseExpr();
            return new NodeLambda(loc,tt.instanceof!TypeFunc,new NodeBlock([new NodeRet(val,loc,"lambda"~to!string(lambdas-1))]),name);
        }
        NodeBlock b = parseBlock("lambda"~to!string(lambdas));
        if(peek().type == TokType.ShortRet) {
            next(); Node val = parseExpr();
            b.nodes ~= new NodeRet(val,loc,"lambda"~to!string(lambdas-1));
        }
        //expect(TokType.Semicolon,peek().line);
        return new NodeLambda(loc,tt.instanceof!TypeFunc,b,name);
    }

    Node parseDecl(string s = "") {
        DeclMod[] mods;
        int loc;
        string name;
        bool isExtern = false;
        bool isConst = false;
        bool isVolatile = false;

        if(peek().value == "volatile") {
            isVolatile = true;
            next();
        }

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
        if(peek().value == "operator") return parseOperatorOverload(type,s);
        if(peek().value == "~" && tokens[_idx+1].value == "with") {
            next(); // Current token = "with"
            name = "~with";
        }
        else name = peek().value;
        loc = peek().line;

        next();
        
        string[] _templates;

        if(peek().type == TokType.Less) {
            // Template there
            next();
            while(peek().type != TokType.More) {
                _templates ~= peek().value;
                _templateNamesF ~= peek().value;
                next();
                if(peek().type == TokType.Comma) next();
            }
            next();
        }

        if(peek().type == TokType.Rpar) {
            // Function with args
            FuncArgSet[] args = parseFuncArgSets();
            if(peek().type == TokType.Lpar) next();
            if(peek().type == TokType.Rbra) next();
            if(peek().type == TokType.ShortRet) {
                next();
                Node expr = parseExpr();
                if(peek().type != TokType.Lpar) expect(TokType.Semicolon);
                _templateNamesF = [];
                return new NodeFunc(name,args,new NodeBlock([new NodeRet(expr,loc,name)]),isExtern,mods,loc,type,_templates);
            }
            else if(peek().type == TokType.Semicolon) {
                next();
                _templateNamesF = [];
                return new NodeFunc(name,args,new NodeBlock([]),isExtern,mods,loc,type,_templates);
            }
            // {block}
            NodeBlock block = parseBlock(name);
            if(peek().type == TokType.ShortRet) {
                next();
                Node n = parseExpr();
                if(peek().type == TokType.Semicolon) next();

                if(type.instanceof!TypeVoid) block.nodes ~= n;
                else block.nodes ~= new NodeRet(n,peek().line,name);
            }
            _templateNamesF = [];
            return new NodeFunc(name,args,block,isExtern,mods,loc,type,_templates);
        }
        else if(peek().type == TokType.Rbra) {
            NodeBlock block = parseBlock(name);
            if(peek().type == TokType.ShortRet) {
                next();
                Node n = parseExpr();
                if(peek().type == TokType.Semicolon) next();

                if(type.instanceof!TypeVoid) block.nodes ~= n;
                else block.nodes ~= new NodeRet(n,peek().line,name);
            }
            _templateNamesF = [];
            return new NodeFunc(name,[],block,isExtern,mods,loc,type,_templates);
        }
        else if(peek().type == TokType.Semicolon) {
            // Var without value
            next();
            return new NodeVar(name,null,isExtern,isConst,(s==""),mods,loc,type,isVolatile);
        }
        else if(peek().type == TokType.ShortRet) {
            next();
            Node expr = parseExpr();
            if(peek().type != TokType.Lpar) expect(TokType.Semicolon);
            _templateNamesF = [];

            if(!type.instanceof!TypeVoid) return new NodeFunc(name,[],new NodeBlock([new NodeRet(expr,loc,name)]),isExtern,mods,loc,type,_templates);
            return new NodeFunc(name,[],new NodeBlock([expr]),isExtern,mods,loc,type,_templates);
        }
        else if(peek().type == TokType.Equ) {
            // Var with value
            next();
            auto e = parseExpr();
            if(peek().type != TokType.Lpar) expect(TokType.Semicolon);
            return new NodeVar(name,e,isExtern,isConst,(s==""),mods,loc,type,isVolatile);
        }
        NodeBlock _b = parseBlock(name);
        _templateNamesF = [];
        return new NodeFunc(name, [], _b, isExtern, mods, loc, type, _templates);
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
        string[] _files;

        while(peek().type == TokType.Less || peek().type == TokType.String) {
            if(peek().type == TokType.Less) {
                // Global
                _files ~= thisExePath()[0..$-4]~getGlobalFile()~".rave";
            }
            else {
                // Local
                _files ~= dirName(MainFile)~"/"~peek().value~".rave";
            }
            next();
        }
        if(peek().type == TokType.Semicolon) next();

        foreach(_file; _files) {
            if(_file != currentFile && !_file.into(_imported)) {
                Lexer l = new Lexer(readText(_file),offset);
                Parser p = new Parser(l.getTokens().dup,offset);
                p._imported = _imported.dup;
                p.MainFile = MainFile;
                p._aliasPredefines = _aliasPredefines.dup;
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
                    else if(NodeBlock b = nodes[i].instanceof!NodeBlock) {
                        for(int j=0; j<b.nodes.length; j++) {
                            if(NodeStruct s = b.nodes[j].instanceof!NodeStruct) s.isImported = true;
                        }
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
                foreach(key; byKey(p._aliasPredefines)) {
                    _aliasPredefines[key] = p._aliasPredefines[key];
                }
                structs ~= p.structs;
            } 
        }
        return new NodeNone();
    }

    Node parseStruct() {
        import std.algorithm : canFind;

        int loc = peek().line;
        next();
        string name = peek().value;
        next(); // current token = { or : or <

        string[] templateNames;
        if(peek().type == TokType.Less) {
            // Structure has a template
            next();
            while(peek().type != TokType.More) {
                templateNames ~= peek().value;
                _templateNames ~= peek().value;
                next();
                if(peek().type == TokType.Comma) next();
            }
            next();
        }

        string _exs = "";
        if(peek().type == TokType.ValSel) {
            next();
            _exs = peek().value;
            next();
        }
        expect(TokType.Rbra); // skip {
        Node[] nodes;
        Node currNode;
        while((currNode = parseTopLevel(name)) !is null) nodes ~= currNode;
        expect(TokType.Lbra); // skip }

        _templateNames = [];

        structs ~= name;
        return new NodeStruct(name,nodes,loc,_exs,templateNames);
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

    Node parseOperatorOverload(Type tt, string s = "") {
        next();
        string name = s~"("~peek().value~")";
        Token _t = peek();
        next();
        if(peek().type != TokType.Rpar) {
            Token _t2 = peek();
            name = s~"("~_t.value~_t2.value~")";
            next();
            if(peek().type != TokType.Rpar) {
                name = s~"("~_t.value~_t2.value~peek().value~")";
                next();
            }
        }

        FuncArgSet[] args;

        if(peek().type == TokType.Rpar) {
            next();

            while(peek().type != TokType.Lpar) {
                Type t = parseType();
                args ~= FuncArgSet(peek().value,t);
                next();
                if(peek().type == TokType.Comma) next();
            }
            next();
        }

        if(peek().type == TokType.ShortRet) {
            NodeBlock nb = new NodeBlock([]);
            next();
            nb.nodes ~= new NodeRet(parseExpr(),peek().line,name);
            if(peek().type == TokType.Semicolon) next();

            return new NodeFunc(name,args.dup,nb,false,[],peek().line,tt,[]);
        }
        else {
            // Rbra
            next();
            NodeBlock nb = parseBlock();
            if(peek().type == TokType.ShortRet) {
                next();
                nb.nodes ~= new NodeRet(parseExpr(),peek().line,name);
            }
            return new NodeFunc(name,args.dup,nb,false,[],peek().line,tt,[]);
        }
    }

    Node parseTopLevel(string s = "") {
        import std.algorithm : canFind;
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

        if(peek().value == "__debugp") {
            next();
            string name = peek().value;
            next();
            next();
            Node a = parseTopLevel();
            if(peek().type == TokType.Lbra) next();
            if(name.into(_aliasPredefines)) return a;
            return new NodeNone();
        }

        if(peek().value == "type") return parseAliasType();

        if(peek().value == "__aliasp") {
            next();
            string name = peek().value;
            next();
            if(peek().type == TokType.Equ) next();
            Node expr = parseExpr();
            next();
            _aliasPredefines[name] = expr;
            return new NodeNone();
        }

        if(peek().type == TokType.Builtin) {
            Token t = peek();
            string name = t.value;
            next(); next();

            Node[] args;
            while(peek().type != TokType.Lpar) {
                if(canFind(structs,peek().value) || peek().value == "int" || peek().value == "short" || peek().value == "long" ||
                   peek().value == "bool" || peek().value == "char" || peek().value == "cent" || canFind(_templateNames,peek().value)) {
                    if(tokens[_idx+1].type == TokType.Rpar || tokens[_idx+1].type == TokType.ShortRet) {
                        args ~= parseLambda();
                    }
                    else if(tokens[_idx+1].type == TokType.Identifier) {
                        // Func
                        args ~= parseDecl("");
                    }
                    else args ~= new NodeType(parseType(),peek().line);
                }
                else args ~= parseExpr();
                if(peek().type == TokType.Comma) next();
            }
            next();
            
            return new NodeBuiltin(name,args.dup,t.line);
        }
        
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
    
    this(Token[] t, int offset) {this.tokens = t; this.offset = offset;}
}