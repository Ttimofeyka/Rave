module parser;

import std.string;
import std.array;
import std.container : DList, SList;
import std.stdio, std.conv;
import ast;
import tokens;
import std.algorithm.iteration : map;
import std.algorithm : canFind;

class Parser {
	private TList _toks;
	private uint _idx = 0;
	private TypeDeclaration[] _decls;

	public this(TList toks) {
		this._toks = toks;
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
			return null;
		return _toks[_idx++];
	}

	private Token peek() {
		if(_idx == _toks.length)
			return null;

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

	private AtstNode parseType() {
		if(peek().type == TokType.tok_id) {
			auto t= next();
			if(t.value == "void")
				return new AtstNodeVoid();
			return new AtstNodeName(t.value);
		}
		else {
			errorExpected("Expected a typename.");
		}
		return null;
	}

	private VariableDeclaration parseVarDecl() {
		// <type> <id:name> [':' <qualified-id:attr> (',' qualified-id:attr)*]
		// Example? int abcd : volatile, std.profile.watch;
		
		auto type = parseType;
		auto nameToken = expectToken(TokType.tok_id);
		if(nameToken is null) nameToken = new Token("<error>");

		return VariableDeclaration(type, nameToken.value);
	}

	private FunctionArgument[] parseFuncArgumentDecls() {
		FunctionArgument[] args;
		
		auto lpar = expectToken(TokType.tok_lpar);
		if(lpar is null) return [];

		while(peek().type != TokType.tok_rpar)
		{
			auto decl = parseVarDecl();
			args ~= FunctionArgument(decl.name, decl.type);
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
	private FunctionDeclaration parseFuncDecl(bool function(Token) isEnd) {
		// [<type:ret-type>] <id:fun-name> ['(' <args> ')'] (';'|'{')
		
		// TODO: Allow for types that don't start with an id.
		// probably do the skipping thing if the peeked token
		// is an identifier, otherwise parse the type.

		// NOTE: Due do this syntax, types cannot be the form
		// of (<id> '(' ...) because that would make the grammar
		// ambiguous.

		auto firstToken = expectToken(TokType.tok_id);

		assert(firstToken !is null); // shouldn't be calling this function if this fails.
		if(isEnd(peek())) { // the decl has ended, the return type is void and there are no args.
			return FunctionDeclaration(FuncSignature(new AtstNodeVoid(), []), firstToken.value, []);
		}

		AtstNode retType;
		Token name = firstToken;
		
		// If the next token is not a '(' (which would mean that
		// the function has no return type and it's just the id)
		if(peek().type != TokType.tok_lpar) {
			// otherwise, if this is not the end, firstToken is the first token of the return type
			// so we need to decrement the _idx so the type parsing function doesn't fail.
			_idx -= 1;

			writeln("parsing return type, peek: ", tokTypeToStr(peek().type));
			retType = parseType();

			// The name will be the next token.
			name = expectToken(TokType.tok_id);
		} else {
			retType = new AtstNodeVoid();
		}

		if(name is null) {
			errorExpected("Expected the name of the function.");
			name = new Token("<error>");
		}
		
		if(peek().type == TokType.tok_lpar) { // Arguments
			auto args = parseFuncArgumentDecls();
			
			AtstNode[] argTypes = array(map!(a => a.type)(args));
			string[] argNames = array(map!(a => a.name)(args));

			return FunctionDeclaration(FuncSignature(retType, argTypes), name.value, argNames);
		}

		return FunctionDeclaration(FuncSignature(retType, []), name.value, []);
	}

	private TypeDeclarationStruct parseStructTypeDecl() {
		// TODO: Parse structs
		return new TypeDeclarationStruct();
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
		writeln("parseSuffix, peek: ", tokTypeToStr(peek().type));
		if(peek().type == TokType.tok_lpar) {
			return parseFuncCall(base);
		}

		return base;
	}

	private AstNode parseAtom() {
		auto t = next();
		if(t.type == TokType.tok_num) return new AstNodeInt(parse!uint(t.value));
		if(t.type == TokType.tok_id) return new AstNodeIden(t.value);
		else if(t.type == TokType.tok_lpar) {
			next();
			auto e = parseExpr();
			expectToken(TokType.tok_rpar);
			return e;
		}
		else errorExpected("Expected a number, a string or an expression in parentheses.");
		return null;
	}

	private AstNode parsePrefix() {
		const TokType[] OPERATORS = [TokType.tok_multiply, TokType.tok_bit_and];
		if(OPERATORS.canFind(peek().type)) { // pointer dereference
			auto tok = next().type;
			return new AstNodeUnary(parseAtom(), tok);
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
			TokType.tok_equ: -98,
			TokType.tok_shortplu: -99,
			TokType.tok_shortmin: -99,
			TokType.tok_shortmul: -99,
			TokType.tok_shortdiv: -99,
			TokType.tok_equal: -80,
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
			nodeStackSize += 2;
		}

		// push the remaining operator onto the nodeStack
		foreach(op; operatorStack) {
			assert(nodeStackSize >= 2);

			auto lhs = nodeStack.front(); nodeStack.removeFront();
			auto rhs = nodeStack.front(); nodeStack.removeFront();

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
		auto decl = parseFuncDecl(function(Token tok) { return tok.type == TokType.tok_2lbra; });
		return new AstNodeFunction(decl, parseBlock());
	}

	private AstNode parseExtern() {
		assert(next().cmd == TokCmd.cmd_extern);
		auto decl = parseFuncDecl(function(Token tok) { return tok.type == TokType.tok_semicolon; });
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
		else {
			errorExpected("Expected either a function, an extern, a type declaration or a semicolon.");
			next();
			return parseTopLevel(); // try again
		}
	}

	public AstNode[] parseProgram() {
		AstNode[] nodes;
		while(peek() !is null) {
			nodes ~= parseTopLevel();
		}

		return nodes;
	}
}
