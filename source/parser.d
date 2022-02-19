module parser;

import std.string;
import std.array;
import std.container : SList;
import std.stdio, std.conv;
import ast;
import tokens;
import std.algorithm.iteration : map;
import std.algorithm : canFind;

// TODO: Add `cast(type) value` create AstNodeCast.
//       see Parser.parsePrefix()`
class Parser {
	private TList _toks;
	private uint _idx = 0;
	private TypeDeclaration[] _decls;

	public this(TList toks) {
		this._toks = toks;
	}

	public TypeDeclaration[] getDecls() {
		return _decls;
	}

	// TODO: Source location data.
	private void error(string msg) {
		writeln("\033[0;31mError: " ~ msg ~ "\033[0;0m");
	}

	private void errorExpected(string msg) {
		error(msg ~ " Got: " ~ tokTypeToStr(next().type));
	}

	private Token next() {
		if(_idx == _toks.length)
			return new Token("");
		return _toks[_idx++];
	}

	private Token peek() {
		if(_idx == _toks.length)
			return new Token("");
		return _toks[_idx];
	}

	private Token expectToken(TokType type) {
		auto t = peek();
		if(t.type != type) {
			errorExpected("Expected " ~ tokTypeToStr(type) ~ ".");
			return null;
		}
		next();
		return t;
	}

	private AtstNode parseTypeAtom() {
		if(peek().type == TokType.tok_id) {
			auto t = next();
			if(t.value == "void")
				return new AtstNodeVoid();
			return new AtstNodeName(t.value);
		}
		else {
			errorExpected("Expected a typename.");
		}
		return null;
	}

	private AtstNode parseType() {
		auto t = parseTypeAtom();
		while(peek().type == TokType.tok_multiply || peek().type == TokType.tok_lbra) {
			/**/ if(peek().type == TokType.tok_multiply) {
				next();
				t = new AtstNodePointer(t);
			}
			else if(peek().type == TokType.tok_lbra) {
				next();
				uint num = 0;
				if(peek().type == TokType.tok_num) {
					num = to!uint(next().value);
				}
				expectToken(TokType.tok_rbra);
				t = new AtstNodeArray(t, num);
			}
		}

		return t;
	}

	private FunctionArgument[] parseFuncArgumentDecls() {
		FunctionArgument[] args;
		
		auto lpar = expectToken(TokType.tok_lpar);
		if(lpar is null) return [];

		while(peek().type != TokType.tok_rpar)
		{
			auto name = expectToken(TokType.tok_id);
			// Allow type inference in argument types?
			expectToken(TokType.tok_type);
			auto type = parseType();
			args ~= FunctionArgument(name.value, type);
			if(peek().type == TokType.tok_rpar) break;
			expectToken(TokType.tok_comma);
		}

		next(); // skip the right parenthesis.
		return args;
	}

	/** 
	 * Parse a generic function declaration with any end token
	 * Params:
	 *   isEnd = Returns whether the passed token is the end of the declaration,
	 * Returns: Function declaration with all of the necessary information in it.
	 */
	private FunctionDeclaration parseFuncDecl(bool function(Token) isEnd, bool allowRetTypeInference) {
		// <id:fun-name> ['(' <args> ')'] [':' [<type:rettype>]] end

		auto name = expectToken(TokType.tok_id);

		if(isEnd(peek())) { // the decl has ended, the return type is void and there are no args.
			return FunctionDeclaration(FuncSignature(new AtstNodeVoid(), []), name.value, []);
		}

		AtstNode retType = new AtstNodeVoid();
		FunctionArgument[] args = [];
		
		if(peek().type == TokType.tok_lpar) args = parseFuncArgumentDecls();

		if(peek().type == TokType.tok_type) {
			next();
			if(isEnd(peek()) && allowRetTypeInference) retType = new AtstNodeUnknown();
			else retType = parseType();
		}

		AtstNode[] argTypes = array(map!(a => a.type)(args));
		string[] argNames = array(map!(a => a.name)(args));

		return FunctionDeclaration(FuncSignature(retType, argTypes), name.value, argNames);
	}

	private TypeDeclarationStruct parseStructTypeDecl() {
		auto st = new TypeDeclarationStruct();
		next(); // skip 'struct' token

		st.name = expectToken(TokType.tok_id).value;

		expectToken(TokType.tok_2lbra);

		while(peek().type != TokType.tok_2rbra) {
			auto name = expectToken(TokType.tok_id).value;
			
			AtstNode type;
			FunctionArgument[] args;
			AstNode value;
			bool isMethod;

			if(peek().type == TokType.tok_lpar) {
				// function with void return type and args.
				isMethod = true;
				type = new AtstNodeVoid();
				args = parseFuncArgumentDecls();
				value = parseBlock();
			}
			else if(peek().type == TokType.tok_equ) {
				// error! field cannot be void type.
				error("Field cannot be of void type! Maybe you forgot ':'?");
				next();
				isMethod = false;
				type = new AtstNodeUnknown();
				args = [];
				value = parseExpr();
				expectToken(TokType.tok_semicolon);
			}
			else if(peek().type == TokType.tok_semicolon) {
				// error! field cannot be void type.
				error("Field cannot be of void type! Maybe you forgot ':'?");
				next();
				isMethod = false;
				type = new AtstNodeVoid();
				args = [];
				value = null;
			}
			else if(peek().type == TokType.tok_type) {
				// field or function with return type.
				next();
				if(peek().type == TokType.tok_equ || peek().type == TokType.tok_2lbra) {
					type = new AtstNodeUnknown();
				} else {
					type = parseType();
				}

				if(peek().type == TokType.tok_equ) {
					next();
					isMethod = false;
					value = parseExpr();
				}
				else if(peek().type == TokType.tok_2lbra) {
					isMethod = true;
					value = parseBlock();
				}
				// can't happen?
				// else if(peek().type == TokType.tok_semicolon && type is null) {
				// 	error("Cannot infer type for a field with a default value");
				// 	next();
				// }
				else {
					isMethod = false;
					expectToken(TokType.tok_semicolon);
				}

				args = [];
				value = null;
			}
			else {
				// function with void return type and no args.
				isMethod = true;
				type = new AtstNodeVoid();
				args = [];
				value = parseBlock();
			}

			if(canFind!((decl, name) => decl.name == name)(st.fieldDecls, name)
			|| canFind!((decl, name) => decl.name == name)(st.methodDecls, name)) {
				error("Field or function with the same name already exists: " ~ name);
			}
			else {
				if(!isMethod) {
					st.fieldDecls ~= VariableDeclaration(type, name);
					if(value !is null) st.fieldValues[name] = value;
				}
				else {
					AtstNode[] argTypes = array(map!(a => a.type)(args));
					string[] argNames = array(map!(a => a.name)(args));
					st.methodDecls ~= FunctionDeclaration(FuncSignature(type, argTypes), name, argNames);
					if(value !is null) st.methodValues[name] = value;
				}
			}
		}

		expectToken(TokType.tok_2rbra);
		return st;
	}

	private TypeDeclaration parseTypeDecl() {
		if(peek().type == TokType.tok_cmd) {
			if(peek().cmd == TokCmd.cmd_struct) {
				return parseStructTypeDecl();
			}
		}

		errorExpected("Expected a type declaration");
		return new TypeDeclaration();
	}

	private AstNode[] parseFuncArguments() {
		AstNode[] args;
		
		auto lpar = expectToken(TokType.tok_lpar);
		if(lpar is null) return [];

		while(peek().type != TokType.tok_rpar)
		{
			args ~= parseExpr();
			if(peek().type == TokType.tok_rpar) break;
			expectToken(TokType.tok_comma);
		}

		next(); // skip the right parenthesis.
		return args;
	}

	private AstNode parseFuncCall(AstNode func) {
		return new AstNodeFuncCall(func, parseFuncArguments());
	}

	private AstNode parseSuffix(AstNode base) {
		while(peek().type == TokType.tok_lpar
		   || peek().type == TokType.tok_lbra
		   || peek().type == TokType.tok_struct_get
		   || peek().type == TokType.tok_dot
		   || peek().type == TokType.tok_plu_plu
		   || peek().type == TokType.tok_min_min)
		{
			if(peek().type == TokType.tok_lpar) {
				base = parseFuncCall(base);
			}
			else if(peek().type == TokType.tok_lbra) {
				next();
				auto idx = parseExpr();
				expectToken(TokType.tok_rbra);
				base = new AstNodeIndex(base, idx);
			}
			else if(peek().type == TokType.tok_struct_get
			     || peek().type == TokType.tok_dot)
			{
				auto t = next().type;
				string field = expectToken(TokType.tok_id).value;
				base = new AstNodeGet(base, field, t == TokType.tok_struct_get);
			}
			else if(peek().type == TokType.tok_plu_plu) {
				next();
				base = new AstNodeUnary(base, TokType.tok_r_plu_plu);
			}
			else if(peek().type == TokType.tok_min_min) {
				next();
				base = new AstNodeUnary(base, TokType.tok_r_min_min);
			}
		}

		return base;
	}

	private AstNode parseAtom() {
		auto t = next();
		if(t.type == TokType.tok_num) {
			if(canFind(t.value, '.'))
				return new AstNodeFloat(parse!float(t.value));
			else
				return new AstNodeInt(parse!ulong(t.value));
		}
		if(t.type == TokType.tok_string) return new AstNodeString(t.value[1..$]);
		if(t.type == TokType.tok_char) return new AstNodeChar(t.value[1..$][0]);
		if(t.type == TokType.tok_id) return new AstNodeIden(t.value);
		if(t.type == TokType.tok_lpar) {
			auto e = parseExpr();
			expectToken(TokType.tok_rpar);
			return e;
		}
		
		writeln(t.value);
		error("Expected a variable, a number, a string, a char or an expression in parentheses. Got: "
			~ tokTypeToStr(t.type));
		return null;
	}

	private AstNode parsePrefix() {
		const TokType[] OPERATORS = [
			TokType.tok_multiply,
			TokType.tok_bit_and,
			TokType.tok_plu_plu,
			TokType.tok_min_min
		];

		if(OPERATORS.canFind(peek().type)) {
			auto tok = next().type;
			return new AstNodeUnary(parsePrefix(), tok);
		}
		return parseAtom();
	}

	private AstNode parseBasic() {
		return parseSuffix(parsePrefix());
	}

	private AstNode parseInfix() {
		const int[TokType] OPERATORS = [
			TokType.tok_plus: 0,
			TokType.tok_minus: 0,
			TokType.tok_multiply: 1,
			TokType.tok_divide: 1,
			TokType.tok_equ: -95,
			TokType.tok_shortplu: -97,
			TokType.tok_shortmin: -97,
			TokType.tok_shortmul: -97,
			TokType.tok_shortdiv: -97,
			TokType.tok_bit_and_eq: -97,
			TokType.tok_bit_or_eq: -97,
			TokType.tok_and_eq: -97,
			TokType.tok_or_eq: -97,
			TokType.tok_more_more_eq: -97,
			TokType.tok_less_less_eq: -97,
			TokType.tok_equal: -80,
			TokType.tok_nequal: -80,
			TokType.tok_less: -70,
			TokType.tok_more: -70,
			TokType.tok_or: -50,
			TokType.tok_and: -50,
			TokType.tok_bit_and: -20,
			TokType.tok_bit_or: -22,
			TokType.tok_bit_ls: -25,
			TokType.tok_bit_rs: -25,
			TokType.tok_bit_xor: -30,
		];

		SList!TokType operatorStack;
		SList!AstNode nodeStack;
		uint nodeStackSize = 0;

		nodeStack.insertFront(parseBasic());
		nodeStackSize += 1;

		while(peek().type in OPERATORS)
		{
			if(operatorStack.empty) {
				operatorStack.insertFront(next().type);
			}
			else {
				auto t = next().type;
				int prec = OPERATORS[t];
				while(prec <= OPERATORS[operatorStack.front()]) {
					// push the operator onto the nodeStack
					assert(nodeStackSize >= 2);

					auto lhs = nodeStack.front(); nodeStack.removeFront();
					auto rhs = nodeStack.front(); nodeStack.removeFront();

					nodeStack.insertFront(new AstNodeBinary(lhs, rhs, operatorStack.front()));
					nodeStackSize -= 1;

					operatorStack.removeFront();
				}

				operatorStack.insertFront(t);
			}

			nodeStack.insertFront(parseBasic());
			nodeStackSize += 1;
		}

		// push the remaining operator onto the nodeStack
		foreach(op; operatorStack) {
			assert(nodeStackSize >= 2);

			auto rhs = nodeStack.front(); nodeStack.removeFront();
			auto lhs = nodeStack.front(); nodeStack.removeFront();

			nodeStack.insertFront(new AstNodeBinary(lhs, rhs, operatorStack.front()));
			nodeStackSize -= 1;

			operatorStack.removeFront();
		}
		
		return nodeStack.front();
	}

	private AstNode parseExpr() {
		return parseInfix();
	}

	private AstNode parseIf() {
		assert(peek().cmd == TokCmd.cmd_if);
		next();
		expectToken(TokType.tok_lpar);
		auto cond = parseExpr();
		expectToken(TokType.tok_rpar);
		auto body_ = parseStmt();
		if(peek().type == TokType.tok_cmd && peek().cmd == TokCmd.cmd_else) {
			next();
			auto othr = parseStmt();
			return new AstNodeIf(cond, body_, othr);
		}
		return new AstNodeIf(cond, body_, null);
	}

	private AstNode parseWhile() {
		assert(peek().cmd == TokCmd.cmd_while);
		next();
		expectToken(TokType.tok_lpar);
		auto cond = parseExpr();
		expectToken(TokType.tok_rpar);
		auto body_ = parseStmt();
		return new AstNodeWhile(cond, body_);
	}

	private AstNode parseStmt() {
		if(peek().type == TokType.tok_2lbra) {
			return parseBlock();
		}
		else if(peek().type == TokType.tok_semicolon) {
			next();
			return parseStmt();
		}
		else if(peek().type == TokType.tok_cmd) {
			/**/ if(peek().cmd == TokCmd.cmd_if) {
				return parseIf();
			}
			else if(peek().cmd == TokCmd.cmd_while) {
				return parseWhile();
			}
			else if(peek().cmd == TokCmd.cmd_ret) {
				next();
				auto e = parseExpr();
				expectToken(TokType.tok_semicolon);
				return new AstNodeReturn(e);
			}
		}
		else if(peek().type == TokType.tok_id) {
			// might be
			//   <name:id> ':' <type:id> [ '=' <expr> ] ';'
			auto name = next().value;
			if(peek().type != TokType.tok_type) {
				_idx -= 1; // oops, this is not a declaration!
				auto e = parseExpr();
				expectToken(TokType.tok_semicolon);
				return e;
			}
			next(); // skip ':' if it exists'

			AtstNode type;
			if(peek().type == TokType.tok_equ || peek().type == TokType.tok_semicolon) {
				type = new AtstNodeUnknown();
			}
			else {
				type = parseType();
			}
			
			auto decl = new AstNodeDecl(name, type, null);
			if(peek().type == TokType.tok_equ) {
				next();
				decl.value = parseExpr();
			}
			expectToken(TokType.tok_semicolon);
			return decl;
		}
		
		auto e = parseExpr();
		expectToken(TokType.tok_semicolon);
		return e;
	}

	private AstNodeBlock parseBlock() {
		AstNode[] nodes;
		assert(next().type == TokType.tok_2lbra);
		while(peek().type != TokType.tok_2rbra) {
			nodes ~= parseStmt();
		}
		next();
		return new AstNodeBlock(nodes);
	}

	private AstNode parseFunc() {
		auto decl = parseFuncDecl(function(Token tok) { return tok.type == TokType.tok_2lbra; }, true);
		return new AstNodeFunction(decl, parseBlock());
	}

	private AstNode parseExtern() {
		assert(next().cmd == TokCmd.cmd_extern);
		auto decl = parseFuncDecl(function(Token tok) { return tok.type == TokType.tok_semicolon; }, false);
		return new AstNodeExtern(decl);
	}

	private bool isTypeDecl() {
		return peek().type == TokType.tok_cmd && peek().cmd == TokCmd.cmd_struct;
	}

	private AstNode parseTopLevel() {
		// top-level ::= <func-decl> (';' | <block>)
		//             | extern <func-decl> ';'
		//             | <var-decl>
		//             | <type-decl>
		//             | ';'

		if(peek().type == TokType.tok_id) {
			return parseFunc();
		}
		else if(peek().type == TokType.tok_cmd
		     && peek().cmd == TokCmd.cmd_extern) {
			return parseExtern();
		}
		else if(peek().type == TokType.tok_semicolon) {
			next(); // Ignore the semicolon, try again
			return parseTopLevel();
		}
		else if(isTypeDecl()) {
			_decls ~= parseTypeDecl();
			return parseTopLevel(); // we ignore type declarations.
		}
		else if(peek().type == TokType.tok_eof) {
			return null;
		}
		else {
			errorExpected("Expected either a function, an extern, a type declaration or a semicolon.");
			next();
			return parseTopLevel(); // try again
		}
	}

	public AstNode[] parseProgram() {
		AstNode[] nodes;
		while(peek().type != TokType.tok_eof) {
			auto n = parseTopLevel();
			if(n is null) break;
			nodes ~= n;
		}

		return nodes;
	}
}
