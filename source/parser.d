module parser;

import std.string;
import std.array;
import std.container : DList;
import std.stdio;
import ast;
import tokens;

class Parser {
	private TList _toks;
	private uint _idx = 0;
	private TypeDeclaration[] _decls;

	public this(TList toks) {
		this._toks = toks;
	}

	// TODO: Source location data.
	private void error(string msg) {
		writeln("\033[0;31mError: " ~ msg);
	}

	private void errorExpected(string msg) {
		error(msg ~ " Got: " ~ tokTypeToUserFriendlyString(next().type));
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
			errorExpected("Expected " ~ tokTypeToUserFriendlyString(type) ~ ".");
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

	private FunctionArgument[] parseFuncArguments() {
		// FunctionArgument[] args;
		
		// if(peek(p).type != TokType.tok_lbra) {
		// 	errorExpected(p, "Expected an opening parenthesis.");
		// 	return [];
		// }

		// next();

		// while(peek(p).type != TokType.tok_rbra)
		// {
		// 	args ~= parseExpr(p);
		// 	if(p_peek(p).type == tt_close_paren) break;
		// 	if(p_peek(p).type != tt_cma)
		// 		return parser_error(p, "expected a comma"), l;
		// 	p_del(p);
		// }

		// // if loop finishes, this error is not needed.
		// // if(p_peek(p).type != tt_close_paren)
		// // 		return parser_error(p, "expected a closing parenthesis"), l;
		// p_del(p);
		// return l;
		return [];
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
		
		auto lbra = expectToken(TokType.tok_lbra);
		if(lbra is null) return [];

		while(peek().type != TokType.tok_rbra)
		{
			auto decl = parseVarDecl();
			args ~= FunctionArgument(decl.name, decl.type);
			if(peek().type == TokType.tok_rbra) break;
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
		auto firstToken = expectToken(TokType.tok_id);
		assert(firstToken !is null); // shouldn't be calling this function if this fails.
		if(isEnd(peek())) { // the decl has ended, the return type is void and there are no args.
			return FunctionDeclaration(FuncSignature(new AtstNodeVoid(), []), firstToken.value, []);
		}

		// otherwise, if this is not the end, firstToken is the first token of the return type
		// so we need to decrement the _idx so the type parsing function doesn't fail.
		_idx -= 1;

		auto retType = parseType();
		auto name = expectToken(TokType.tok_id);
		if(name is null) {
			errorExpected("Expected the name of the function.");
			name = new Token("<error>");
		}
		
		if(peek().type == TokType.tok_lbra) { // Arguments
			auto args = parseFuncArgumentDecls();
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

	private AstNode parseStmt() {
		// if(peek().type == TokType.tok_)
		return null;
	}

	private AstNodeBlock parseBlock() {
		AstNode[] nodes;
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
		return null;
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
