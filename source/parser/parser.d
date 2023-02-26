/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

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
import std.bigint;

string[] types = ["void","bool", "char", "uchar", "wchar", "uwchar", "short", "ushort", "int", "uint", "long", "ulong", "cent", "ucent"];

int[TokType] operators;

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
    Node _genValue;
}

long hexToLong(string s) {
    import core.stdc.stdlib;
	long v = strtoul(toStringz(s), null, 16);
	return v;
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
    string _file;

    private void error(string msg) {
        pragma(inline,true);
		writeln("\033[0;31mError in '"~_file~"' file: " ~ msg ~ "\033[0;0m");
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
            switch(t.value) {
                case "void": return new TypeVoid();
                case "auto": return new TypeAuto();
                case "const":
                    if(peek().type == TokType.Rpar) next();
                        Type _t = parseType();
                    if(peek().type == TokType.Lpar) next();
                    return new TypeConst(_t);
                default:
                    if(t.value.into(_aliasTypes)) return _aliasTypes[t.value];
                    else if(_templateNames.canFind(t.value)) return new TypeStruct(t.value);
                    return getType(t.value);
            }
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
            NodeBlock block;
            if(peek().type == TokType.Rbra) block = parseBlock("");
            else block = new NodeBlock([]);
            return new TypeBuiltin(name,args,block);
        }
        else {
            error("Expected a typename on "~to!string(peek().line)~" line!");
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

    Type parseType(bool cannotBeTemplate = false) {
        auto t = parseTypeAtom();
        while(peek().type == TokType.Multiply || peek().type == TokType.Rarr || peek().type == TokType.Rpar || (peek().type == TokType.Less && !cannotBeTemplate)) {
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
            else if(peek().type == TokType.Less && !cannotBeTemplate) {
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
                    _sTypes ~= _types[i].toString()~",";
                }
                _sTypes = _sTypes[0..$-1];

                t = new TypeStruct(name~"<"~_sTypes~">",_types);

                if(peek().type == TokType.Rpar) {
                    // Call
                    t = new TypeCall(t.instanceof!TypeStruct.name,parseFuncCallArgs());
                }
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

    bool isDefinedLambda(bool retidx = true) {
        int idx = _idx; // Type
        if(tokens[_idx+1].type != TokType.Rpar) return false;
        _idx += 1;

        int countOfRpars = 1;
        while(tokens[_idx].type != TokType.Lpar) {
            _idx += 1;
            if(peek().type == TokType.Lpar) {
                countOfRpars -= 1;
                if(countOfRpars == 0) break;
                _idx += 1;
            }
            else if(peek().type == TokType.Rpar) {
                countOfRpars += 1;
            }
        }
        next();
        if(peek().type == TokType.Rbra || peek().type == TokType.ShortRet) {
            if(retidx) _idx = idx;
            return true;
        }
        if(retidx) _idx = idx;
        return false;
    }

    bool commaAfterCall() {
        int idx = _idx;

        // Peek() == (
        next();
        int countOfRpars = 1;

        while(countOfRpars != 0) {
            if(peek().type == TokType.Rpar) countOfRpars += 1;
            else if(peek().type == TokType.Lpar) countOfRpars -= 1;
            next();
        }
        bool result = false;
        if(peek().type == TokType.Comma) result = true;
        _idx = idx;
        return result;
    }

    bool isSlice() {
        // peek() == TokType.Rarr
        int idx = _idx;
        next();

        int countOfRarr = 1;
        while(countOfRarr != 0) {
            if(peek().type == TokType.Rarr) countOfRarr += 1;
            else if(peek().type == TokType.Larr) countOfRarr -= 1;
            else if(peek().type == TokType.SliceOper || peek().type == TokType.SlicePtrOper) {
                _idx = idx;
                return true;
            }
            next();
        }
        _idx = idx;
        return false;
    }

    Node[] parseFuncCallArgs() {
        import std.algorithm : canFind;
        if(peek().type == TokType.Rpar) next(); // skip (
        Node[] args;
        while(peek().type != TokType.Lpar) {
            if(peek().type == TokType.Identifier) {
                switch(peek().value) {
                    case "int":
                    case "short":
                    case "long":
                    case "cent":
                    case "char":
                    case "bool":
                    case "uint":
                    case "ushort":
                    case "ulong":
                    case "ucent":
                    case "uchar":
                    case "wchar":
                    case "uwchar":
                        if(isDefinedLambda()) {
                            NodeLambda nl = parseLambda().instanceof!NodeLambda;
                            args ~= nl;
                            break;
                        }
                        args ~= new NodeType(parseType(),peek().line);
                        break;
                    default:
                        if(canFind(structs,peek().value) || canFind(_templateNames,peek().value)) {
                            if(isDefinedLambda()) {
                                NodeLambda nl = parseLambda().instanceof!NodeLambda;
                                args ~= nl;
                                break;
                            }
                            else if(tokens[_idx+1].type != TokType.Rpar) args ~= new NodeType(parseType(),peek().line);
                            else args ~= parseExpr();
                        }
                        else {
                            args ~= parseExpr();
                        }
                        break;
                }
            }
            else args ~= parseExpr();
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

    Node parseAtom(string f) {
        import std.algorithm : canFind;
        auto t = next();
        if(t.type == TokType.Number) {
            if(peek().value.toLower() == "u") {
                next();
                NodeInt _int = new NodeInt(BigInt(t.value));
                _int.isUnsigned = true;
                return _int;
            }
            else if(peek().value.toLower() == "l") {
                next();
                NodeInt _int = new NodeInt(BigInt(t.value));
                _int.isMustBeLong = true;
                return _int;
            }
            return new NodeInt(BigInt(t.value));
        }
        if(t.type == TokType.FloatNumber) {
            if(peek().value.toLower() == "d") {
                next();
                return new NodeFloat(to!double(t.value),true);
            }
            return new NodeFloat(to!double(t.value));
        }
        if(t.type == TokType.HexNumber) return new NodeInt(BigInt(t.value));
        if(t.type == TokType.True) return new NodeBool(true);
        if(t.type == TokType.False) return new NodeBool(false);
        if(t.type == TokType.String) {
            if(peek().type == TokType.Identifier && peek().value == "w") {
                next();
                return new NodeString(tokens[_idx-2].value,true);
            }
            return new NodeString(t.value,false);
        }
        if(t.type == TokType.Char) {
            if(peek().type == TokType.Identifier && peek().value == "w") {
                next();
                NodeChar ch = new NodeChar(to!char(tokens[_idx-2].value));
                ch.isWide = true;
                return ch;
            }
            return new NodeChar(to!char(t.value));
        }
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
                Type type = parseType();
                next();
                Node val = parseExpr();
                next();
                return new NodeItop(val,type,t.line);
            }
            else if(t.value == "ptoi") {
                next();
                Node val = parseExpr();
                next();
                return new NodePtoi(val,t.line);
            }
            else if(t.value == "asm") {
                next();
                Type tt = new TypeVoid();
                if(peek().type != TokType.String) {
                    tt = parseType();
                    if(peek().type == TokType.Comma) next();
                }
                string s = peek().value;
                next();
                string adds = "";
                if(peek().type == TokType.Comma) {
                    next();
                    adds = peek().value;
                    next();
                }
                Node[] args;
                if(peek().type == TokType.Comma) {
                    next();
                    while(peek().type != TokType.Lpar) {
                        args ~= parseExpr();
                        if(peek().type != TokType.Lpar) next();
                    }
                }
                if(peek().type == TokType.Lpar) next();
                return new NodeAsm(s,true,tt,adds,args);
            }
            else if(canFind(types,t.value)) {
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
                    return canFind(types,s);
                }
                if(tokens[_idx+1].type == TokType.More || isBasicType(tokens[_idx+1].value) || tokens[_idx+2].type == TokType.Less || tokens[_idx+2].type == TokType.More || tokens[_idx+2].type == TokType.Comma || tokens[_idx+2].type == TokType.Multiply || tokens[_idx+2].type == TokType.Rarr) {
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
                if(canFind(structs,peek().value) || canFind(types,peek().value) || canFind(_templateNames,peek().value) || tokens[_idx+1].value == "*") {
                    Type pType = parseType(true);
                    if(peek().type != TokType.Comma && peek().type != TokType.Lpar) {
                        TokType operator = peek().type;
                        Node n;
                        if(tokens[_idx+1].type == TokType.Builtin) n = parseAtom(f);
                        else {
                            next();
                            n = new NodeType(parseType(true),peek().line);
                        }
                        args ~= new NodeBinary(operator,new NodeType(pType,peek().line),n,peek().line);
                    }
                    else args ~= new NodeType(pType,peek().line);
                }
                else args ~= parseExpr();
                if(peek().type == TokType.Comma) next();
            }
            next();
            NodeBlock block;
            if(peek().type == TokType.Rbra) {
                block = parseBlock(f);
            }
            else block = new NodeBlock([]);
            if(peek().type == TokType.Identifier && tokens[_idx-1].type != TokType.Lbra && tokens[_idx-1].type != TokType.Semicolon) {
                string nam = peek().value;
                Node val = null;
                next();
                if(peek().type == TokType.Equ) {
                    next();
                    val = parseExpr(f);
                }
                return new NodeVar(nam,val,false,false,false,[],peek().line,new TypeBuiltin(name,args.dup,block));
            }
            return new NodeBuiltin(name,args.dup,t.line,block);
        }
        error("Expected a number, true/false, char, variable or expression. Got: "~to!string(t.type)~" on "~to!string(peek().line+1)~" line.");
        return null;
    }

    Node parsePrefix(string f) {
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
            next();
            auto tok3 = next();
            if(tok3.value == "[" || tok3.value == ".") {
                _idx -= 2;
                return new NodeUnary(-1,tok.type,parseSuffix(parseAtom(f),f));
            }
            else _idx -= 2;
            return new NodeUnary(tok.line, tok.type, parsePrefix(f));
        }
        return parseAtom(f);
    }

    Node parseSlice(string f, Node value) {
        // peek() == TokType.Rarr
        Node start;
        Node end;
        int loc = peek().line;

        next();
        start = parseExpr(f);
        bool isConst = true;
        if(peek().type == TokType.SlicePtrOper) isConst = false;
        next();
        end = parseExpr(f);
        if(peek().type == TokType.Larr) next();

        return new NodeSlice(loc, start, end, value, isConst);
    }

    bool isTemplate() {
        int idx = _idx;
        next();

        if(peek().type == TokType.Number || peek().type == TokType.String || peek().type == TokType.HexNumber || peek().type == TokType.FloatNumber) {
            _idx = idx;
            return false;
        }

        next();
        if(peek().type == TokType.Multiply || peek().type == TokType.Comma || peek().type == TokType.Rarr || peek().type == TokType.More || peek().type == TokType.Less) {
            if(peek().type == TokType.Multiply) {
                next();
                if(peek().type == TokType.Multiply || peek().type == TokType.Rarr || peek().type == TokType.Less || peek().type == TokType.More || peek().type == TokType.Comma) {
                    _idx = idx;
                    return true;
                }
                _idx = idx;
                return false;
            }
            else if(peek().type == TokType.Comma || peek().type == TokType.More || peek().type == TokType.Less) {
                _idx = idx;
                return true;
            }
            else if(peek().type == TokType.Rarr) {
                while(peek().type == TokType.Rarr) {
                    next();
                    if(peek().type != TokType.Number) { // TODO: Alias support
                        _idx = idx;
                        return false;
                    }
                    next();
                    next();
                }
                if(peek().type == TokType.Multiply || peek().type == TokType.Less || peek().type == TokType.More || peek().type == TokType.Comma) {
                    _idx = idx;
                    return true;
                }
                _idx = idx;
                return false;
            }
        }
        _idx = idx;
        return false;
    }

    Node parseSuffix(Node base, string f) {
        while(peek().type == TokType.Rpar
             || peek().type == TokType.Rarr
             || peek().type == TokType.Dot
        ) {
            if(peek().type == TokType.Rpar) {
                base = parseCall(base);
            }
            else if(peek().type == TokType.Rarr) {
                if(isSlice()) {
                    base = parseSlice(f,base);
                }
                else base = new NodeIndex(base,parseIndexs(),peek().line);
            }
            else if(peek().type == TokType.Dot) {
                next();
                bool isPtr = (peek().type == TokType.GetPtr);
                if(isPtr) next();
                string field = expect(TokType.Identifier).value;
                if(peek().type == TokType.Rpar) {
                    base = parseCall(new NodeGet(base,field,(isPtr),peek().line));
                }
                else if(peek().type == TokType.Less) {
                    if(isTemplate()) {
                        Type[] types;
                        string _stypes = "<";
                        next();
                        while(peek().type != TokType.More) {
                            types ~= parseType();
                            _stypes ~= types[types.length-1].toString()~",";
                            if(peek().type == TokType.Comma) next();
                        }
                        _stypes = _stypes[0..$-1]~">";
                        next();
                        if(peek().type == TokType.Rpar) {
                            base = parseCall(new NodeGet(base,field~_stypes,(isPtr),peek().line));
                        }
                        else assert(0);
                    }
                }
                else base = new NodeGet(base,field,(peek().type == TokType.Equ || isPtr),peek().line);
            }
        }
        return base;
    }

    Node parseBasic(string f) {
        return parseSuffix(parsePrefix(f),f);
    }

    Node parseExpr(string f = "") {
        SList!Token operatorStack;
		SList!Node nodeStack;
		uint nodeStackSize = 0;
		uint operatorStackSize = 0;

		nodeStack.insertFront(parseBasic(f));
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

			nodeStack.insertFront(parseBasic(f));
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

        Node[] before;
        NodeBinary cond;
        Node[] after;

        int curr = 0;

        while(peek().type != TokType.Lpar) {
            if(peek().type == TokType.Semicolon) {
                curr += 1;
                next();
            }
            if(peek().type == TokType.Comma) next();

            if(curr == 0) {
                if(tokens[_idx+1].value == "=") {
                    // Not a decl, put value into variable
                    before ~= parseExpr();
                }
                else {
                    // Decl
                    Type _t = parseType();
                    string name = peek().value;
                    next();
                    if(peek().type == TokType.Equ) {
                        next();
                        Node val = parseExpr();
                        before ~= new NodeVar(name,val,false,false,false,[],peek().line,_t);
                        if(peek().type != TokType.Semicolon && peek().type != TokType.Comma) curr += 1;
                    }
                    else before ~= new NodeVar(name,null,false,false,false,[],peek().line,_t);
                }
            }
            else if(curr == 1) {
                cond = parseExpr().instanceof!NodeBinary;
            }
            else {
                after ~= parseExpr();
            }
        }
        next();

        return new NodeFor(before,cond,after,parseBlock(f),f,peek().line);
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

    Node parseAsm(string f) {
        assert(0);
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
        else if(peek().type == TokType.Identifier && (tokens[_idx+1].type == TokType.Less)) {
            return parseDecl(f);
        }
        else if(peek().value == "asm") {
            next(); next();
            Type tt = new TypeVoid();
            if(peek().type != TokType.String) {
                tt = parseType();
                if(peek().type == TokType.Comma) next();
            }
            string s = peek().value;
            next();
            string adds = "";
            if(peek().type == TokType.Comma) {
                next();
                adds = peek().value;
                next();
            }
            Node[] args;
            if(peek().type == TokType.Comma) {
                next();
                while(peek().type != TokType.Lpar) {
                    args ~= parseExpr();
                    if(peek().type != TokType.Lpar) next();
                }
            }
            if(peek().type == TokType.Lpar) next();
            if(peek().type == TokType.Semicolon) next();
            return new NodeAsm(s,true,tt,adds,args);
        }
        else if(peek().value == "try") {
            int l = peek().line;
            next();
            NodeBlock nb = parseBlock(f);
            if(peek().type == TokType.Lbra) next();
            return new NodeTry(l,nb);
        }
        else if(peek().type == TokType.Command) {
            if(peek().value == "if") return parseIf(f,isStatic);
            if(peek().value == "while") return parseWhile(f);
            if(peek().value == "for") return parseFor(f);
            if(peek().value == "break") return parseBreak();
            if(peek().value == "with") return parseWith(f);
            if(peek().value == "asm") return parseAsm(f);
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
                if(tokens[_idx+1].type == TokType.Rarr && tokens[_idx+4].type != TokType.Equ && tokens[_idx+4].type != TokType.Lpar && tokens[_idx+4].type != TokType.Rpar) {
                    if(tokens[_idx+2].type == TokType.Number && tokens[_idx+3].type == TokType.Larr) return parseDecl(f);
                }
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
                            case "const":
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
                            case "const":
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
                    if(peek().type == TokType.Builtin) return e;
                    if(peek().value != "}") expect(TokType.Semicolon);
                    else next();
                    return e;
                }
                _idx -= 1;
                return parseDecl(f);
        }
        else if(peek().type == TokType.Eof) return null;
        else if(peek().type == TokType.Rpar) {
            DeclMod[] mods;
            next();
            while(peek().type != TokType.Lpar) {
                if(peek().type == TokType.Builtin) {
                    NodeBuiltin nb = parseBuiltin().instanceof!NodeBuiltin;
                    mods ~= DeclMod("@"~nb.name,"",nb);
                    if(peek().type == TokType.Comma) next();
                    continue;
                }
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
            return parseDecl(f,mods.dup);
        }
        auto e = parseExpr();
        if(peek().type == TokType.Builtin) return e;
        if(peek().value == "}") next();
        else expect(TokType.Semicolon);
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
        return new NodeAliasType(l,name,childType);
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

    Node parseBuiltin(string s = "") {
        import std.algorithm : canFind;

        string name = peek().value;
        next(); next();
        Node[] args;
        while(peek().type != TokType.Lpar) {
            if(canFind(structs,peek().value) || canFind(types,peek().value) || canFind(_templateNames,peek().value)) {
                if(tokens[_idx+1].type == TokType.Identifier || tokens[_idx+1].type == TokType.Rpar || tokens[_idx+1].type == TokType.ShortRet) {
                    args ~= parseLambda();
                }
                else args ~= new NodeType(parseType(),peek().line);
            }
            else {
                args ~= parseExpr();
            }
            if(peek().type == TokType.Comma) next();
        }
        next();
        NodeBlock block;
        if(peek().type == TokType.Rbra) {
            block = parseBlock(s);
        }
        else block = new NodeBlock([]);

        return new NodeBuiltin(name,args.dup,peek().line,block);
    }

    Node parseDecl(string s = "", DeclMod[] _mods = []) {
        DeclMod[] mods = _mods;
        int loc;
        string name;
        bool isExtern = false;
        bool isVolatile = false;

        if(peek().value == "volatile") {
            isVolatile = true;
            next();
        }

        if(peek().value == "extern") {
            isExtern = true;
            next();
        }

        if(peek().value == "alias") {
            if(tokens[_idx+1].value == "{" || tokens[_idx+2].value == "{") return parseMultiAlias(s,mods.dup,loc);
        }

        if(peek().type == TokType.Rpar) {
            // Decl have mods
            next();
            while(peek().type != TokType.Lpar) {
                if(peek().type == TokType.Builtin) {
                    NodeBuiltin nb = parseBuiltin().instanceof!NodeBuiltin;
                    mods ~= DeclMod("@"~nb.name,"",nb);
                    if(peek().type == TokType.Comma) next();
                    continue;
                }
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

        int IDX = _idx;
        auto type = parseType();
        if(TypeCall tc = type.instanceof!TypeCall) {
            _idx = IDX;
            Node n = parseAtom(s);
            if(peek().type == TokType.Semicolon) next();
            return n;
        }
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
            return new NodeVar(name,null,isExtern,type.instanceof!TypeConst ? true : false,(s==""),mods,loc,type,isVolatile);
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
            return new NodeVar(name,e,isExtern,type.instanceof!TypeConst ? true : false,(s==""),mods,loc,type,isVolatile);
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

        string[] funcs;

        if(peek().type == TokType.Semicolon) next();
        else if(peek().type == TokType.ValSel) {
            next();
            funcs ~= peek().value;
            next();
            if(peek().type == TokType.Semicolon) next();
        }

        return new NodeImport(_files.dup,peek().line, funcs.dup);
    }

    Node parseStruct(DeclMod[] mods = []) {
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
        return new NodeStruct(name,nodes,loc,_exs,templateNames,mods.dup);
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

    Node parseMixin() {
        next(); // Skip mixin command
        string name = peek().value;
        int loc = peek().line;
        next();

        string[] templateNames;
        string[] names;
        if(peek().type == TokType.Less) {
            // Template names
            next();
            while(peek().type != TokType.More) {
                templateNames ~= peek().value;
                next();
                if(peek().type == TokType.Comma) next();
            }
            next();
        }

        if(peek().type == TokType.Rpar) {
            // Names for arguments
            next();
            while(peek().type != TokType.Lpar) {
                names ~= peek().value;
                next();
                if(peek().type == TokType.Comma) next();
            }
        }

        return new NodeMixin(name, loc, parseBlock(), templateNames, names);
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

        if(peek().value == "import") return parseImport();

        if(peek().value == "mixin") return parseMixin();

        if(peek().value == "debug") return parseDebug();

        if(peek().value == "__debugp") {
            next();
            string name = peek().value;
            next();
            next();
            Node a = parseTopLevel(s);
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
            Node expr = parseExpr(s);
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
                if(canFind(structs,peek().value) || canFind(types,peek().value) || canFind(_templateNames,peek().value)) {
                    if(tokens[_idx+1].type == TokType.Rpar || tokens[_idx+1].type == TokType.ShortRet) {
                        args ~= parseLambda();
                    }
                    else if(tokens[_idx+1].type == TokType.Identifier) {
                        args ~= parseDecl("");
                    }
                    else args ~= new NodeType(parseType(),peek().line);
                }
                else {
                    args ~= parseExpr(s);
                }
                if(peek().type == TokType.Comma) next();
            }
            next();
            NodeBlock block = new NodeBlock([]);
            if(peek().type == TokType.Rbra) {
                next();
                while(peek().type != TokType.Lbra) {
                    block.nodes ~= parseTopLevel(s);
                }
                next();
            }
            NodeBuiltin nb = new NodeBuiltin(name,args.dup,t.line,block);
            nb.isTopLevel = true;
            return nb;
        }

        if(peek().type == TokType.Rpar) {
            DeclMod[] mods;
            next();
            while(peek().type != TokType.Lpar) {
                if(peek().type == TokType.Builtin) {
                    NodeBuiltin nb = parseBuiltin().instanceof!NodeBuiltin;
                    mods ~= DeclMod("@"~nb.name,"",nb);
                    if(peek().type == TokType.Comma) next();
                    continue;
                }
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
            if(peek().value != "struct") return parseDecl(s,mods.dup);
            return parseStruct(mods.dup);
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

    this(Token[] t, int offset, string _file) {this.tokens = t; this.offset = offset; this._file = _file;}
}
