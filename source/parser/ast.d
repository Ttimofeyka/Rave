module parser.ast;

import std.stdio, std.conv;
import std.algorithm.iteration : map;
import std.algorithm : canFind;
import parser.generator.gen, lexer.tokens;
import std.conv : to;
import core.stdc.stdlib : exit;
import parser.typesystem;
import parser.util;
import parser.analyzer;
import std.string;
import std.array;
import parser.generator.llvm_abs;
import parser.util;
import std.typecons;
import core.stdc.stdlib : malloc;
import parser.mparser;
import parser.atst;
import llvm;

SourceLocation basic = SourceLocation(0,0,"");

struct FuncSignature {
	AtstNode ret;
	AtstNode[] args;

	this(AtstNode ret, AtstNode[] args) {
		this.ret = ret;
		this.args = args;
	}
	
	debug {
		string toString() const {
			return ret.toString() ~ " <- (" ~ join(map!(a => a.toString())(args), ", ") ~ ")";
		}
	}
}

struct TypecheckResult {
	bool success;
	// TODO? see AstNode.typechecK
}
// Abstract syntax tree Node.
class AstNode {
	SourceLocation where;

	LLVMValueRef gen(AnalyzerScope s) {
		LLVMValueRef a;
		return a;
	}

	// TODO: Propagate to child classes.
	// TypecheckResult typecheck(SemanticAnalyzerContext ctx) {
	// 	return TypecheckResult(false);
	// }

	Type getType(AnalyzerScope s) {
		return new TypeUnknown();
	}

	void analyze(AnalyzerScope s, Type neededType) {
		writeln("Abstract AstNode.analyze called! this: ",
			this.classinfo.name);
		assert(0);
	}

	TList getTokens(string caller) { TList toret = new TList(); toret.insertBack(new Token(basic,"LOL")); return toret; }

	debug {
		void writeTabs(int indent) {
			for(int i = 0; i < indent; ++i)
				write("  ");
		}

		void debugPrint(int indent) {
			writeTabs(indent);
			writeln("AstNode not implemented.");
		}
	}
}

struct DeclMod {
	string name;
	string[] args;
}

class FunctionDeclaration {
	FuncSignature signature;
	string name;
	string[] argNames;
	bool isStatic;
	bool isExtern;
	DeclMod[] decl_mods;
	string doc;

	this(
		FuncSignature signature,
		string name,
		string[] argNames,
		bool isStatic,
		bool isExtern,
		DeclMod[] decl_mods,
		string doc,
	) {
		this.signature = signature;
		this.name = name;
		this.argNames = argNames;
		this.isStatic = isStatic;
		this.isExtern = isExtern;
		this.decl_mods = decl_mods;
		this.doc = doc;
	}

	TypeFunction get(AnalyzerScope ctx) {
		return new TypeFunction(signature.ret.get(ctx), signature.args.map!(a => a.get(ctx)).array);
	}

	debug {
		override string toString() const {
			string s = "";
			
			if(isStatic) s ~= "static ";
			if(isExtern) s ~= "extern ";

			s ~= name ~ "(";

			if(signature.args.length > 0) {
				s ~= argNames[0] ~ ": " ~ signature.args[0].toString();
				for(int i = 1; i < signature.args.length; ++i) {
					s ~= ", " ~ argNames[i] ~ ": " ~ signature.args[i].toString();
				}
			}
			s ~= "): " ~ signature.ret.toString();
			return s;
		}
	}
}

struct FunctionArgument {
	string name;
	AtstNode type;
}

class VariableDeclaration {
	AtstNode type;
	string name;
	bool isStatic;
	bool isExtern;
	DeclMod[] decl_mods;
	string doc;
	
	this(
		AtstNode type,
		string name,
		bool isStatic,
		bool isExtern,
		DeclMod[] decl_mods,
		string doc,
	) {
		this.type = type;
		this.name = name;
		this.isStatic = isStatic;
		this.isExtern = isExtern;
		this.decl_mods = decl_mods;
		this.doc = doc;
	}

	debug {
		override string toString() const {
			string s = "";
			if(isStatic) s ~= "static ";
			if(isExtern) s ~= "extern ";
			return s ~ name ~ ": ";
		}
	}
}

class Declaration {
	string name;

	/** 
	 * Analyzes and generates AST from a declaration node.
	 * Params:
	 *   s = the analyzer scope.
	 * Returns: a list of nodes that replace the declaration, empty if none.
	 */
	abstract AstNode[] analyze(AnalyzerScope s);
	abstract LLVMValueRef gen(AnalyzerScope s);
}

struct EnumEntry {
	string name;
	ulong value;
}

class DeclarationEnum : Declaration {
	EnumEntry[] entries;

	override string toString() const {
		string s = "enum " ~ name ~ " {";
		if(entries.length > 0) s ~= '\n';
		foreach(entry; entries) {
			s ~= "    " ~ entry.name ~ " = " ~ to!string(entry.value) ~ "\n";
		}
		return s ~ "}";
	}

	override AstNode[] analyze(AnalyzerScope s) {
		foreach(EnumEntry entry; entries) {
			s.vars[entry.name] = new ScopeVarIntConstant(
				new TypeBasic(BasicType.t_int),
				entry.value
			);
		}
		return [];
	}

	override LLVMValueRef gen(AnalyzerScope s) {
		return null;
	}
}

class DeclarationStruct : Declaration {
	string name;

	VariableDeclaration[] fieldDecls;
	FunctionDeclaration[] methodDecls;

	AstNode[string] fieldValues;
	AstNode[string] methodValues;

	override AstNode[] analyze(AnalyzerScope s) {
		auto fieldTypes = assocArray(fieldDecls.map!(d => tuple(d.name, d.type.get(s))));
		auto methodTypes = assocArray(methodDecls.map!(d => tuple(d.name, d.get(s))));

		auto structType = new TypeStruct(fieldTypes, methodTypes);

		foreach(TypeFunction ft; structType.methods) {
			ft.args = structType ~ ft.args;
		}

		s.ctx.typeContext.setType(name, structType);

		AstNode[] funcList;
		foreach(FunctionDeclaration method; methodDecls)
		{
			method.signature.args = new AtstNodeName(name) ~ method.signature.args;
			method.argNames = "this" ~ method.argNames;

			funcList ~= new AstNodeFunction(
				SourceLocation(0, 0, ""),
				method,
				method.name !in methodValues ? null : methodValues[method.name]
			);
		}
		return funcList;
	}

	override string toString() const {
		string s = "struct " ~ name ~ " {";
		if(fieldDecls.length + methodDecls.length > 0) {
			s ~= '\n';
		}
		foreach(field; fieldDecls) {
			s ~= "    " ~ field.name ~ ": " ~ field.type.toString();
			if(field.name in fieldValues) {
				s ~= " = ..."; // ~ fieldValues[field.name].debugPrint();
			}
			s ~= ";\n";
		}
		foreach(method; methodDecls) {
			s ~= "    " ~ method.toString();
			
			if(method.name in methodValues) {
				s ~= " { ... }\n"; // ~ fieldValues[field.name].debugPrint();
			} else {
				s ~= ";\n";
			}
		}
		return s ~ "}";
	}

	override LLVMValueRef gen(AnalyzerScope s) {
		return null;
	}
}

class AstNodeStruct : AstNode {
	AstNodeDecl[] variables;
	AstNodeFunction[] methods;
	string name;

	this(string name, AstNodeDecl[] v, AstNodeFunction[] m) {
		this.variables = v.dup;
		this.methods = m.dup;
		this.name = name;
	}

	override void analyze(AnalyzerScope s, Type neededType) {
		
	}

	override LLVMValueRef gen(AnalyzerScope s) {
		GenerationContext ctx = s.genctx;
		LLVMTypeRef t = LLVMStructCreateNamed(ctx.context, toStringz(name));
		ctx.gstructs.ss[name] = t;
		LLVMTypeRef* types = cast(LLVMTypeRef*)malloc(LLVMTypeRef.sizeof * variables.length);
		for(int i=0; i<variables.length; i++) {
			ctx.gstructs.addV(cast(uint)i, variables[i].decl.name, name);
			ctx.gstructs.addVar(variables[i],name);
			types[i] = ctx.getLLVMType(variables[i].decl.type, s);
		}
		//ctx.gstructs.addS(LLVMVectorType(LLVMPointerType(LLVMInt8Type(),0), cast(uint)variables.length), name);
		LLVMStructSetBody(t,types,cast(uint)variables.length,false);
		ctx.gstructs.ss[name] = t;
		return null;
	}

	override string toString() const {
		return "AstNodeStruct";
	}
}

struct ReturnStmt {
	AstNodeReturn node;
	AnalyzerScope scope_;
}

struct ReturnGenStmt {
	LLVMBasicBlockRef where;
	LLVMValueRef value;
}

class AstNodeFunction : AstNode {
	FunctionDeclaration decl;
	TypeFunction type;
	AstNode body_;
	ReturnStmt[] returns;

	LLVMBasicBlockRef exitBlock;
	LLVMBuilderRef builder;
	ReturnGenStmt[] genReturns;
	Type retT;

	LLVMTypeRef* paramTypes = null;

	this(SourceLocation loc, FunctionDeclaration decl, AstNode body_) {
		this.where = loc;
		this.decl = decl;
		this.body_ = body_;
	}

	override void analyze(AnalyzerScope s, Type) {
		for(int i=0; i<decl.signature.args.length; i++) {
			s.genctx.gfuncs.funcs_args[i] = decl.signature.args[i];
		}
		auto retType = this.decl.signature.ret.get(s);
		Type[] argTypes = array(map!((a) => a.get(s))(this.decl.signature.args));
		this.type = new TypeFunction(retType, argTypes);

		if(this.body_ is null) {
			if(retType.instanceof!TypeUnknown) {
				s.ctx.addError("Cannot infer the return type of a function with no body.",
					this.where);
				return;
			}

			// if(this.decl.isExtern) {
			// 	s.vars[this.decl.name] = actualDecl;
			// 	return;
			// }
			// else {
				// s.ctx.addError("Forward declarations are not yet implemented.",
				// 	this.where);
				// Forward declaration... TODO
			s.vars[this.decl.name] = new ScopeVar(type, decl.isStatic, decl.isExtern);
			return;
			// }
		}

		auto ss = new AnalyzerScope(s.ctx, s.genctx, s);
		ss.ctx.currentFunc = this;

		for(int i = 0; i < this.decl.argNames.length; ++i) {
			ss.vars[this.decl.argNames[i]] = new ScopeVar(argTypes[i]);
		}

		ss.neededReturnType = retType;
		this.body_.analyze(ss, new TypeUnknown());

		if(this.returns.length == 0 && retType.instanceof!TypeUnknown)
			retType = new TypeVoid();

		foreach(ret; this.returns) {
			Type t = new TypeVoid();
			if(ret.node.value !is null)
				t = ret.node.value.getType(ret.scope_);
			
			if(retType.instanceof!TypeUnknown)
				retType = t;
			else if(!retType.assignable(t))
			{
				/*s.ctx.addError("First mismatching return is of type '" ~ type.toString()
					~ "', while all previous returns are of type '" ~ retType.toString() ~ "'",
					ret.node.where);
				s.ctx.addError("Return types don't match in function '" ~ decl.name ~ "'.", this.where);
				break;*/
			}
		}

		if(!ss.hadReturn) {
			if(retType.instanceof!TypeUnknown)
				retType = new TypeVoid();

			if(!retType.instanceof!TypeVoid) {
				s.ctx.addError("In function '" ~ this.decl.name ~
					"': not all code paths return a value!", this.where);
			}
		}
		else if(retType.instanceof!TypeUnknown) {
			retType = ss.returnType;
		}

		// TODO: Check if types are not inferred!
		this.type = new TypeFunction(retType, argTypes);
		retT = this.type.ret;
		s.vars[this.decl.name] = new ScopeVar(type, decl.isStatic, decl.isExtern);
	}

	private LLVMTypeRef* getParamsTypes(AnalyzerScope s) {
		GenerationContext ctx = s.genctx;
		if(paramTypes is null) {
			paramTypes = stackMemory.createBuffer!LLVMTypeRef(
				decl.signature.args.length
			);

			for(int i = 0; i < decl.signature.args.length; i++) {
				paramTypes[i] = ctx.getLLVMType(decl.signature.args[i],s);
				//char* ptype = LLVMPrintTypeToString(paramTypes[i]);
				//writeln(fromStringz(ptype));
				ctx.gargs.set(i, paramTypes[i], decl.argNames[i]);
			}
		}

		return paramTypes;
	}

	override LLVMValueRef gen(AnalyzerScope s) {
		bool vararg = false;
		GenerationContext ctx = s.genctx;
		if(decl.name in ctx.gfuncs.funcs) {
			ctx.err("Function \""~decl.name~"\" already exists!",where);
		}
		builder = LLVMCreateBuilder();

		AstNodeBlock funcBody = cast(AstNodeBlock)body_;

		LLVMTypeRef retType = ctx.getLLVMType(decl.signature.ret,s);

		for(int i=0; i<decl.signature.args.length; i++) {
			if(AtstNodeName n = decl.signature.args[i].instanceof!AtstNodeName) {
				if(n.name == "args") {
					ctx.gfuncs.funcs_varargs[decl.name] = i;
					break;
				}
			}
		}

		string linkname = null;

		bool shouldMangle = true;
		foreach(mod; decl.decl_mods) { // Function pre-options
			if(mod.name == "C") shouldMangle = false; // Mangle C-mode
			else if(mod.name == "dll") {
				if(mod.args.length != 1) 
					s.ctx.addError("Function modifier 'dll' needs exactly 1 parameter.",
						this.where);
			}
			else if(mod.name == "lib") {
				if(mod.args.length != 1) 
					s.ctx.addError("Function modifier 'lib' needs exactly 1 parameter.",
						this.where);
			}
			else if(mod.name == "linkname") {
				linkname = mod.args[0];
			}
			else if(mod.name == "vararg") vararg = true;
			else {
				s.ctx.addError("Unknown modifier: '" ~ mod.name ~ "'", this.where);
			}
		}

		LLVMTypeRef funcType = LLVMFunctionType(
		    retType,
			getParamsTypes(s),
		    cast(uint)decl.signature.args.length,
		    vararg
	    );

		LLVMValueRef func;
		if(linkname == null) func = LLVMAddFunction(
			ctx.mod,
			toStringz(
				shouldMangle
				? ctx.mangleQualifiedName([decl.name], true)
				: decl.name),
			funcType
		);
		else func = LLVMAddFunction(
			ctx.mod,
			toStringz(
				shouldMangle
				? ctx.mangleQualifiedName([decl.name], true)
				: linkname),
			funcType
		);

		//LLVMSetFunctionCallConv(func, LLVMPTXKernelCallConv);
		

		ctx.gfuncs.add(decl.name, func, retType, type);
		ctx.currfunc = func;

		ctx.gstack.gen_init(ctx);

		if(!decl.isExtern) {
			LLVMBasicBlockRef entryBlock = LLVMAppendBasicBlock(func, toStringz("entry"));
			exitBlock = LLVMAppendBasicBlock(func, toStringz("exit"));
			
			LLVMPositionBuilderAtEnd(builder, entryBlock);
			ctx.currbuilder = builder;

			if(decl.name == ctx.entryFunction) {
				LLVMValueRef* args;
				for(int i = 0; i < ctx.presets.length; i++) {
					LLVMBuildCall(
						ctx.currbuilder,
						ctx.presets[i],
						args,
						0,
						toStringz("")
					);
				}
			}

			funcBody.gen(s);

			//writeln(decl.signature.ret.toString());
			if(decl.signature.ret.instanceof!AtstNodeVoid) {
				LLVMBuildBr(
					ctx.currbuilder,
					exitBlock
				);
			}

			LLVMMoveBasicBlockAfter(exitBlock, LLVMGetLastBasicBlock(func));
			LLVMPositionBuilderAtEnd(builder, exitBlock);
			if(!type.ret.instanceof!TypeVoid) {
				LLVMValueRef[] retValues = array(map!(x => x.value)(genReturns));
				LLVMBasicBlockRef[] retBlocks = array(map!(x => x.where)(genReturns));

				if(retBlocks.length == 1 || retBlocks[0] == null) {
					LLVMBuildRet(builder, retValues[0]);
				}
				else {
					auto phi = LLVMBuildPhi(builder, retType, "retval");
					LLVMAddIncoming(
						phi,
						retValues.ptr,
						retBlocks.ptr,
						cast(uint)genReturns.length
					);

					LLVMBuildRet(builder, phi);
				}
			}
			else LLVMBuildRetVoid(builder);
			
			ctx.currbuilder = null;
			ctx.gstack.clean();
			ctx.gargs.clean();
		}

		ctx.currfunc = null;

		//char* modString = LLVMPrintModuleToString(ctx.mod);
		//writeln("Module Code: ", fromStringz(modString));
		LLVMVerifyFunction(func, 0);
		
		return func;
	}

	override TList getTokens(string caller) {
		TList toret = new TList();
		if(body_ !is null) {
			AstNodeBlock funcBody = cast(AstNodeBlock)body_;
			toret.insertBack(new Token(basic,decl.name,TokType.tok_id));
			if(decl.argNames.length > 0) {
				toret.insertBack(new Token(basic,"(",TokType.tok_lpar));
				for(int i=0; i<decl.signature.args.length; i++) {
					toret.insertBack(new Token(basic,decl.argNames[i],TokType.tok_id));
					toret.insertBack(new Token(basic,":",TokType.tok_type));
					toret.insertBack(new Token(basic,decl.signature.args[i].toString(),TokType.tok_id));
					if((i+1) < decl.signature.args.length) toret.insertBack(new Token(basic,",",TokType.tok_comma));
				}
				toret.insertBack(new Token(basic,")",TokType.tok_rpar));
			}
			toret.insertBack(new Token(basic,":",TokType.tok_type));
			toret.insertBack(new Token(basic,decl.signature.ret.toString()));
			toret.insertBack(new Token(basic,"{",TokType.tok_2lbra));
			toret.insertBack(funcBody.getTokens("AstNodeFunction"));
			toret.insertBack(new Token(basic,"}",TokType.tok_2rbra));
			return toret;
		}
		else {
			toret.insertBack(new Token(basic,"extern"));
			toret.insertBack(new Token(basic,decl.name,TokType.tok_id));
			toret.insertBack(new Token(basic,"(",TokType.tok_lpar));
			if(decl.argNames.length > 0) {
				for(int i=0; i<decl.signature.args.length; i++) {
					toret.insertBack(new Token(basic,decl.argNames[i],TokType.tok_id));
					toret.insertBack(new Token(basic,":",TokType.tok_type));
					toret.insertBack(new Token(basic,decl.signature.args[i].toString(),TokType.tok_id));
					if((i+1) < decl.signature.args.length) toret.insertBack(new Token(basic,",",TokType.tok_comma));
				}
			}
			toret.insertBack(new Token(basic,")",TokType.tok_rpar));
			toret.insertBack(new Token(basic,":",TokType.tok_type));
			toret.insertBack(new Token(basic,decl.signature.ret.toString()));
			toret.insertBack(new Token(basic,";",TokType.tok_semicolon));
			return toret;
		}
	}

	debug {
		override void debugPrint(int indent) {
			writeTabs(indent);
			writeln("Function Declaration: ",
				type is null ? decl.toString() : type.toString(decl.name, decl.argNames)
			);
			if(body_ !is null) body_.debugPrint(indent + 1);
		}
	}
}

class AstNodeArray : AstNode {
	// [0,1,2,etc.], etc.
	AstNode[] values;

	this(AstNode[] values) {
		this.values = values.dup;
	}

	override void analyze(AnalyzerScope s, Type neededType) {}
	override LLVMValueRef gen(AnalyzerScope s) {
		GenerationContext ctx = s.genctx;
		LLVMValueRef* constvalues = cast(LLVMValueRef*)malloc(LLVMValueRef.sizeof * values.length);
		LLVMValueRef fvalgen = values[0].gen(s);
		constvalues[0] = fvalgen;
		for(int i=1; i<values.length; i++) {
			LLVMValueRef valgen = values[i].gen(s);
			if(LLVMTypeOf(valgen) != LLVMTypeOf(fvalgen)) {
				constvalues[i] = castTo(ctx, valgen, LLVMTypeOf(fvalgen));
			}
			else constvalues[i] = valgen;
		}
		return LLVMConstArray(LLVMArrayType(LLVMTypeOf(fvalgen),cast(uint)values.length),constvalues,cast(uint)values.length);
	}
}

// token naming sucks
// enum BinaryOpType {
// 	binary_op_add, // addition
// 	binary_op_sub, // substraction
// 	binary_op_mul, // multiplication
// 	binary_op_div, // division
// 	binary_op_and, // boolean and
// 	binary_op_cor, // boolean or
// 	binary_op_xor, // exclusive or
// 	binary_op_grt, // greater than
// 	binary_op_lst, // less than
// 	binary_op_geq, // greater than or equal
// 	binary_op_leq, // less than or equal
// 	binary_op_eql, // equal to
// 	binary_op_neq, // not equal to
// }

class AstNodeGet : AstNode { // ->, .
	AstNode value;
	string field;
	bool isSugar; // true if '->' false if '.'
	string base;
	bool isEqu = false;
	
	// (after analysis) true if the get is for
	// accessing things from a namespace
	bool isNamespace;

	Type type;
	
	this(string b, SourceLocation where, AstNode value, string field, bool isSugar) {
		this.where = where;
		this.value = value;
		this.field = field;
		this.isSugar = isSugar;
		this.base = b;
	}

	override Type getType(AnalyzerScope s) {
		return type;
	}

	override void analyze(AnalyzerScope s, Type neededType) {
		this.value.analyze(s, new TypeUnknown());
		
		Type t = this.value.getType(s);
		if(isSugar) {
			if(!t.instanceof!TypePointer) {
				//s.ctx.addError("Operator '->' expects a pointer type on the left hand side. "
					//~ "Got '" ~ t.toString() ~ "' instead.", this.where);
				return;
			}
			t = (cast(TypePointer)t).to;
		}

		if(t.instanceof!TypeType && !isSugar) {
			TypeType tt = cast(TypeType)t;
			isNamespace = true;
			if(!tt.type.instanceof!TypeStruct) {
				s.ctx.addError("Operator '.' expects a structure type on the left hand side. "
					~ "Got '" ~ t.toString() ~ "' instead.", this.where);
				return;
			}

			TypeStruct st = cast(TypeStruct)tt.type;

			if(field !in st.fields && field !in st.methods) {
				s.ctx.addError("No field or method named '" ~ field
					~ "' on type '" ~ st.toString() ~ "'.", this.where);
				return;
			}

			// TODO: static fields
			// if(Type v = field in st.fields) {
			// 	v.;
			// }

			if(field in st.fields) type = st.fields[field];
			else type = st.methods[field];
			return;
		}

		if(t.instanceof!TypeStruct) {
			TypeStruct st = cast(TypeStruct)t;

			if(field !in st.fields && field !in st.methods) {
				if(ScopeVar v = s.get(field)) {
					if(neededType.assignable(v.type)) {
						type = v.type;
						return;
					}
				}
				s.ctx.addError("No such function in scope: '" ~ field ~ "'.", this.where);
				return;
			}

			if(field in st.fields) type = st.fields[field];
			else type = st.methods[field];

			return;
		}
		else if(neededType.instanceof!TypeFunctionLike) {
			// Search for global functions with field name only if
			// the outer scope requests a function-like value.
			if(ScopeVar v = s.get(field)) {
				if(neededType.assignable(v.type)) {
					type = v.type;
					return;
				}
			}
			s.ctx.addError("No such function in scope: '" ~ field ~ "'.", this.where);
		}

		/*if(isSugar)
			s.ctx.addError("Operator '->' expects a pointer to a structure on the left hand side. "
				~ "Got '" ~ t.toString() ~ "' instead.", this.where);
		else
			s.ctx.addError("Operator '.' expects a structure on the left hand side. "
				~ "Got '" ~ t.toString() ~ "' instead.", this.where);*/
	}

	override LLVMValueRef gen(AnalyzerScope s) {
		GenerationContext ctx = s.genctx;
		if(base !in ctx.gstructs.structs) {
			// Its var-struct or error?
			if(AstNodeGet v = value.instanceof!AstNodeGet) {
				writeln(fromStringz(LLVMPrintTypeToString(LLVMTypeOf(v.gen(s)))));
				exit(-1);
			}
			writeln("Didn't find structure '",base,"'!");
			exit(-1);
		}
		LLVMValueRef loaded = LLVMBuildLoad(
			ctx.currbuilder,
			ctx.gstack[base],
			toStringz("load")
		);
		if(LLVMGetTypeKind(LLVMTypeOf(loaded)) == LLVMPointerTypeKind) {
			// struct*
			if(isEqu) return LLVMBuildStructGEP(
				ctx.currbuilder,
				loaded,
				ctx.gstructs.getV(field, ctx.gstructs.structs[base]),
				toStringz("struct_gep")
			);
			return LLVMBuildLoad(
				ctx.currbuilder,
				LLVMBuildStructGEP(
					ctx.currbuilder,
					loaded,
					ctx.gstructs.getV(field, ctx.gstructs.structs[base]),
					toStringz("struct_gep")
				),
				toStringz("load")
			);
		}
		else {if(isEqu) return LLVMBuildStructGEP(
			ctx.currbuilder,
			ctx.gstack[base],
			ctx.gstructs.getV(field, ctx.gstructs.structs[base]),
			toStringz("struct_gep")
		);
		return LLVMBuildLoad(
			ctx.currbuilder,
			LLVMBuildStructGEP(
				ctx.currbuilder,
				ctx.gstack[base],
				ctx.gstructs.getV(field, ctx.gstructs.structs[base]),
				toStringz("struct_gep")
			),
			toStringz("load")
		);}
	}

	debug {
		override void debugPrint(int indent) {
			writeTabs(indent);
			writeln("Get(" ~ (isSugar?"->":".") ~ ") ", this.field);
			value.debugPrint(indent + 1);
		}
	}
}

class AstNodeBool : AstNode {
	bool value;

	override void analyze(AnalyzerScope s, Type neededType) {}
	override TList getTokens(string caller) {
		TList toret = new TList();
		toret.insertBack(new Token(basic,to!string(value)));
		return toret;
	}
	this(bool value) {this.value = value;}
	override LLVMValueRef gen(AnalyzerScope s) {
		return LLVMConstInt(
			LLVMInt1Type(),
			cast(ulong)value,
			true
		);
	}
}

class AstNodeBinary : AstNode { // Binary operations
	AstNode lhs, rhs;
	TokType type;
	Type valueType;

	this(SourceLocation where, AstNode lhs, AstNode rhs, TokType type) {
		this.where = where; 
		this.lhs = lhs;
		this.rhs = rhs;
		this.type = type;
	}

	override LLVMValueRef gen(AnalyzerScope s) {
		GenerationContext ctx = s.genctx;
		switch(type) {
			case TokType.tok_equ:
				auto rhsgg = rhs.gen(s);
				LLVMValueRef rhsg;
				if(auto iden = lhs.instanceof!AstNodeIden) {
					if(LLVMTypeOf(rhsgg) == LLVMPointerType(LLVMInt8Type(),0)) {
							rhsg = LLVMBuildPointerCast(
								ctx.currbuilder,
								rhsgg,
								LLVMTypeOf(LLVMBuildLoad(ctx.currbuilder,ctx.gstack[iden.name],toStringz("l"))),
								toStringz("ptrtoptr")
							);
					}
					else rhsg = rhsgg;
					return LLVMBuildStore(
						ctx.currbuilder,
						rhsg,
						ctx.gstack[iden.name]
					);
				}
				else if(AstNodeIndex index = lhs.instanceof!AstNodeIndex) {
					LLVMValueRef iindex;
					auto iiden = index.base.gen(s);
					if(LLVMGetTypeKind(LLVMTypeOf(iiden)) == LLVMVectorTypeKind) {
						// Array or structure
						if(AstNodeIden as_iden = index.base.instanceof!AstNodeIden) {
							if(as_iden.name in ctx.gstructs.structs) {
								// It's struct
								if(AstNodeString ss = index.index.instanceof!AstNodeString) {
									// ...["..."]
									if(AstNodeIden id = index.base.instanceof!AstNodeIden) {
										LLVMValueRef brhs = rhs.gen(s);
										auto loaded = LLVMBuildLoad(
											ctx.currbuilder,
											ctx.gstack[id.name],
											toStringz("load")
										);
										if(LLVMGetTypeKind(LLVMTypeOf(brhs)) == LLVMIntegerTypeKind) {
											brhs = intToPtr(ctx, brhs, LLVMPointerType(LLVMInt8Type(),0));
										}
										auto insert = LLVMBuildInsertElement(
											ctx.currbuilder,
											loaded,
											brhs,
											LLVMConstInt(LLVMInt32Type(),
											 	cast(ulong)ctx.gstructs.getV(
													 ss.value[0..$-1], 
													 ctx.gstructs.structs[id.name]
												), 
												false
											),
											toStringz("struct_set")
										);

										LLVMBuildStore(
											ctx.currbuilder,
											insert,
											ctx.gstack[id.name]
										);

										return ctx.gstack[id.name];
									}
									else iindex = index.index.gen(s);
								}
								else iindex = index.index.gen(s);
								LLVMValueRef ptr;
								if(LLVMGetTypeKind(LLVMTypeOf(rhs.gen(s))) == LLVMIntegerTypeKind) {
									ptr = intToPtr(ctx, rhs.gen(s), LLVMPointerType(LLVMInt8Type(),0));
								}
								else if(LLVMGetTypeKind(LLVMTypeOf(rhs.gen(s))) == LLVMPointerTypeKind) {
									ptr = LLVMBuildBitCast(
										ctx.currbuilder,
										rhs.gen(s),
										LLVMPointerType(LLVMInt8Type(),0),
										toStringz("bitcast")
									);
								}
								return LLVMBuildInsertElement(
									ctx.currbuilder,
									iiden,
									ptr,
									iindex,
									toStringz("set_struct_el")
								);
							}
						}
						auto ins = LLVMBuildInsertElement(
							ctx.currbuilder,
							iiden,
							rhs.gen(s),
							index.index.gen(s),
							toStringz("set_arr_el")
						);
						if(AstNodeIden id = index.base.instanceof!AstNodeIden) {
							return LLVMBuildStore(
								ctx.currbuilder,
								ins,
								ctx.gstack[id.name]
							);
						}
						return ins;
					}
					else { // Pointer
						return LLVMBuildStore(
							ctx.currbuilder,
							rhs.gen(s),
							index.genForSet(s)
						);
					}
				}
				else if(AstNodeGet get = lhs.instanceof!AstNodeGet) {
					get.isEqu = true;
					auto store = LLVMBuildStore(
						ctx.currbuilder,
						rhsgg,
						get.gen(s)
					);
					return store;
				}
				return null;
			case TokType.tok_shortplu:
			case TokType.tok_shortmin:
			case TokType.tok_shortmul:
				AstNodeIden iden = cast(AstNodeIden)lhs;
				if(s.genctx.gstack.isLocal(iden.name)) {
					LLVMValueRef tmp;
					if(type == TokType.tok_shortplu) {
						tmp = operAdd(
							ctx,
							lhs.gen(s),
							rhs.gen(s)
						);
					}
					else if (type == TokType.tok_shortmin) {
						tmp = operSub(
							ctx,
							lhs.gen(s),
							rhs.gen(s)
						);
					}
					else if (type == TokType.tok_shortmul) {
						tmp = operMul(
							ctx,
							lhs.gen(s),
							rhs.gen(s)
						);
					}
					
					LLVMBuildStore(
						s.genctx.currbuilder,
						tmp,
						s.genctx.gstack[iden.name]
					);

					return s.genctx.gstack[iden.name];
				}
				else {
					LLVMValueRef tmp;
					LLVMValueRef loaded = LLVMBuildLoad(
						ctx.currbuilder,
						ctx.gstack[iden.name],
						toStringz("load_global")
					);
					if(type == TokType.tok_shortplu) {
						tmp = LLVMBuildAdd(
							ctx.currbuilder,
							loaded,
							rhs.gen(s),
							toStringz("tmp")
						);
					}
					else if(type == TokType.tok_shortmin) {
						tmp = LLVMBuildSub(
							ctx.currbuilder,
							loaded,
							rhs.gen(s),
							toStringz("tmp")
						);
					}
					else if(type == TokType.tok_shortmul) {
						tmp = ctx.operMul(
							loaded,
							rhs.gen(s)
						);
					}

					LLVMBuildStore(
						s.genctx.currbuilder,
						tmp,
						s.genctx.gstack[iden.name]
					);
					return s.genctx.gstack[iden.name];
				}
				assert(0);
			case TokType.tok_plus:
			case TokType.tok_minus:
			case TokType.tok_multiply:
			case TokType.tok_divide:
				LLVMValueRef result;
				switch(type) {
					case TokType.tok_plus:
						result = operAdd(
							ctx,
							lhs.gen(s),
							rhs.gen(s)
						); break;
					case TokType.tok_minus:
						result = operSub(
							ctx,
							lhs.gen(s),
							rhs.gen(s)
						); break;
					case TokType.tok_multiply:
						result = operMul(
							ctx,
							lhs.gen(s),
							rhs.gen(s)
						); break;
					case TokType.tok_divide:
						result = operDiv(
							ctx,
							lhs.gen(s),
							rhs.gen(s)
						); break;
					default: break;
				}
				return result;
			case TokType.tok_bit_ls:
			case TokType.tok_bit_rs:
			case TokType.tok_bit_and:
			case TokType.tok_bit_or:
			case TokType.tok_bit_xor:
				LLVMValueRef bit_result;
				if(type == TokType.tok_bit_ls) {
					bit_result = LLVMBuildShl(
						ctx.currbuilder,
						lhs.gen(s),
						rhs.gen(s),
						toStringz("bit_ls")
					);
				}
				else if(type == TokType.tok_bit_rs)  {
					bit_result = LLVMBuildAShr(
						ctx.currbuilder,
						lhs.gen(s),
						rhs.gen(s),
						toStringz("bit_rs")
					);
				}
				else if(type == TokType.tok_bit_and) {
					bit_result = LLVMBuildAnd(
						ctx.currbuilder,
						lhs.gen(s),
						rhs.gen(s),
						toStringz("bit_and")
					);
				}
				else if(type == TokType.tok_bit_or) {
					bit_result = LLVMBuildOr(
						ctx.currbuilder,
						lhs.gen(s),
						rhs.gen(s),
						toStringz("bit_or")
					);
				}
				else if(type == TokType.tok_bit_xor) {
					bit_result = LLVMBuildXor(
						ctx.currbuilder,
						lhs.gen(s),
						rhs.gen(s),
						toStringz("bit_xor")
					);
				}
				return bit_result;
			case TokType.tok_equal:
			case TokType.tok_nequal:
				auto lhs_g = lhs.gen(s);
				auto rhs_g = rhs.gen(s);
				if(LLVMTypeOf(lhs_g) != LLVMTypeOf(rhs_g)) {
					rhs_g = castNum(ctx, rhs_g, LLVMTypeOf(lhs_g));
				}
				if(type == TokType.tok_equal) return cmpNum(
					ctx,
					lhs_g,
					rhs_g
				);
				return ncmpNum(
					ctx,
					lhs_g,
					rhs_g
				);
			case TokType.tok_and:
			case TokType.tok_or:
				if(type == TokType.tok_and) {
					return LLVMBuildAnd(
						s.genctx.currbuilder,
						lhs.gen(s),
						rhs.gen(s),
						toStringz("and_2cmp")
					);
				}
				return LLVMBuildOr(
					s.genctx.currbuilder,
					lhs.gen(s),
					rhs.gen(s),
					toStringz("or_2cmp")
				);
			case TokType.tok_less:
				LLVMValueRef[] trn = trueNums(ctx,lhs.gen(s),rhs.gen(s));
				return LLVMBuildICmp(
					ctx.currbuilder,
					LLVMIntSGT,
					trn[0],
					trn[1],
					toStringz("less")
				);
			case TokType.tok_more:
				LLVMValueRef[] trn = trueNums(ctx,lhs.gen(s),rhs.gen(s));
				return LLVMBuildICmp(
					ctx.currbuilder,
					LLVMIntSLT,
					trn[0],
					trn[1],
					toStringz("more")
				);
			case TokType.tok_less_equal:
				LLVMValueRef[] trn = trueNums(ctx,lhs.gen(s),rhs.gen(s));
				return LLVMBuildICmp(
					ctx.currbuilder,
					LLVMIntSLE,
					trn[0],
					trn[1],
					toStringz("lessequal")
				);
			case TokType.tok_more_equal:
				LLVMValueRef[] trn = trueNums(ctx,lhs.gen(s),rhs.gen(s));
				return LLVMBuildICmp(
					ctx.currbuilder,
					LLVMIntSGE,
					trn[0],
					trn[1],
					toStringz("moreequal")
				);
			case TokType.tok_proc:
				LLVMValueRef[] trn = trueNums(ctx,lhs.gen(s),rhs.gen(s));
				return LLVMBuildSRem(
					ctx.currbuilder,
					trn[0],
					trn[1],
					toStringz("srem")
				);
			default:
				break;
		}
		assert(0);
	}

	override Type getType(AnalyzerScope s) {
		return valueType;
	}

	private void checkTypes(AnalyzerScope s, Type neededType) {
		
	}

	override TList getTokens(string caller) {
		TList toret = new TList();
		toret.insertBack(lhs.getTokens("AstNodeBinary"));
		toret.insertBack(new Token(basic,"",type));
		toret.insertBack(rhs.getTokens("AstNodeBinary"));
		if(caller == "AstNodeBlock") toret.insertBack(new Token(basic,";",TokType.tok_semicolon));
		return toret;
	}

	override void analyze(AnalyzerScope s, Type neededType) {
		if(this.type == TokType.tok_equ) {
			lhs.analyze(s, new TypeUnknown()); // TODO: Check if it's assignable
			rhs.analyze(s, lhs.getType(s));
			return;
		}

		// if(_isArithmetic()) {
		// 	if(!instanceof!TypeBasic(neededType)) {
		// 		s.ctx.addError("Non-basic type " ~ neededType.toString() ~
		// 			" was required for an arithmetic binary operation by the outer scope.");
		// 	}
		// 	this.valueType = neededType;
		// 	return;
		// }
		
		lhs.analyze(s, neededType);
		rhs.analyze(s, neededType);
		
		if(neededType.instanceof!TypeUnknown)
			neededType = lhs.getType(s); // TODO: Find best suitable type!
			
		
		// writeln("Checking types...");
		checkTypes(s, neededType);
		// writeln("this.getType() -> ", this.getType(s));
	}

	private bool _isArithmetic() {
		return canFind([
			TokType.tok_plus,
			TokType.tok_minus,
			TokType.tok_multiply,
			TokType.tok_divide,
		], type);
	}

	debug {
		override void debugPrint(int indent) {
			writeTabs(indent);
			writeln("Binary ", to!string(type));
			lhs.debugPrint(indent + 1);
			rhs.debugPrint(indent + 1);
		}
	}
}

class AstNodeAddr : AstNode {
	string id;
	
	this(string id) {this.id = id;}

	override void analyze(AnalyzerScope s, Type neededType) {

	}

	override LLVMValueRef gen(AnalyzerScope s) {
		GenerationContext ctx = s.genctx;
		LLVMTypeRef lt = LLVMTypeOf(ctx.gstack[id]);
		if(lt == LLVMPointerType(LLVMInt1Type(),0)
		 ||lt == LLVMPointerType(LLVMInt8Type(),0)
		 ||lt == LLVMPointerType(LLVMInt16Type(),0)
		 ||lt == LLVMPointerType(LLVMInt32Type(),0)
		 ||lt == LLVMPointerType(LLVMInt64Type(),0)) return LLVMBuildPtrToInt(ctx.currbuilder,ctx.gstack[id],LLVMTypeOf(ctx.gstack[id]),toStringz("addr"));
		return LLVMBuildBitCast(ctx.currbuilder,ctx.gstack[id],LLVMTypeOf(LLVMBuildLoad(ctx.currbuilder,ctx.gstack[id],toStringz("l"))),toStringz("link"));
	}
}

class AstNodeUnary : AstNode { // Unary operations
	AstNode node;
	TokType type;

	this(SourceLocation where, AstNode node, TokType type) {
		this.where = where;
		this.node = node;
		this.type = type;
	}

	override void analyze(AnalyzerScope s, Type neededType) {

	}

	override LLVMValueRef gen(AnalyzerScope s) {
		GenerationContext ctx = s.genctx;
		if(type == TokType.tok_bit_and) {
			auto val = node.gen(s);
			
			if(LLVMGetTypeKind(LLVMTypeOf(val)) == LLVMVectorTypeKind) {
				auto extval = LLVMBuildExtractElement(
					ctx.currbuilder,
					val,
					LLVMConstInt(LLVMInt32Type(),0,false),
					toStringz("extval")
				);

				return LLVMBuildIntToPtr(
					ctx.currbuilder,
					extval,
					LLVMPointerType(LLVMTypeOf(extval),0),
					toStringz("itop")
				);
			}
			else if(LLVMGetTypeKind(LLVMTypeOf(val)) == LLVMArrayTypeKind) {
				AstNodeIden id = node.instanceof!AstNodeIden;
				val = ctx.gstack[id.name];
				return LLVMBuildInBoundsGEP(
					ctx.currbuilder,
					val,
					[LLVMConstInt(LLVMInt32Type(),0,false),LLVMConstInt(LLVMInt32Type(),0,false)].ptr,
					2,
					toStringz("getel")
				);
			}
			else if(LLVMGetTypeKind(LLVMTypeOf(val)) == LLVMIntegerTypeKind) {
				return LLVMBuildIntToPtr(
					ctx.currbuilder,
					val,
					LLVMPointerType(LLVMTypeOf(val),0),
					toStringz("inttoptr")
				);
			}
			else if(LLVMGetTypeKind(LLVMTypeOf(val)) == LLVMStructTypeKind) {
				auto alloca = LLVMBuildAlloca(
					ctx.currbuilder,
					LLVMPointerType(LLVMTypeOf(val),0),
					toStringz("alloca")
				);
				if(AstNodeIden id = node.instanceof!AstNodeIden) {
					LLVMBuildStore(
						ctx.currbuilder,
						ctx.gstack[id.name],
						alloca
					);
				}
				return LLVMBuildLoad(
					ctx.currbuilder,
					alloca,
					toStringz("load")
				);
			}
			return null;
		}
		else if(type == TokType.tok_minus) {
			auto nodegen = node.gen(s);
			if(LLVMTypeOf(nodegen) == LLVMFloatType()) return LLVMBuildFAdd(
				ctx.currbuilder,
				LLVMBuildNot(
					ctx.currbuilder,
					nodegen,
					toStringz("not")
				),
				LLVMConstReal(LLVMFloatType(),cast(double)1),
				toStringz("unary")
			);
			return LLVMBuildAdd(
				ctx.currbuilder,
				LLVMBuildNot(
					ctx.currbuilder,
					nodegen,
					toStringz("not")
				),
				LLVMConstInt(LLVMTypeOf(nodegen), cast(ulong)1, false),
				toStringz("unary")
			);
			
		}
		else if(type == TokType.tok_not) {
			return LLVMBuildNot(
				ctx.currbuilder,
				node.gen(s),
				toStringz("unary")
			);
		}
		else if(type == TokType.tok_multiply) {
			auto nodeg = node.gen(s);

			if(LLVMGetTypeKind(LLVMTypeOf(nodeg)) == LLVMPointerTypeKind) {
				// Return value of first element
				return LLVMBuildLoad(
					ctx.currbuilder,
					LLVMBuildGEP(
						ctx.currbuilder,
						nodeg,
						[LLVMConstInt(LLVMInt32Type(),0,false)].ptr,
						1,
						toStringz("get_el_bi")
					),
					toStringz("load")
				);
			}
			else if(LLVMGetTypeKind(LLVMTypeOf(nodeg)) == LLVMArrayTypeKind) {
				// Return value of first element
				return LLVMBuildLoad(ctx.currbuilder, LLVMBuildInBoundsGEP(
					ctx.currbuilder,
					nodeg,
					[LLVMConstInt(LLVMInt32Type(),0,false),LLVMConstInt(LLVMInt32Type(),0,false)].ptr,
					2,
					toStringz("getel")
				), toStringz("load"));
			}
			else {
				writeln("Error: using unary operator '*' without array or pointer!");
				exit(-1);
			}
		}
		else if(type == TokType.tok_plu_plu) {
			return operAdd(
				ctx,
				node.gen(s),
				LLVMConstInt(LLVMInt32Type(),1,false)
			);
		}
		else if(type == TokType.tok_hash) {
			if(AstNodeIden id = node.instanceof!AstNodeIden) {
				LLVMValueRef vid = id.gen(s);
				return LLVMBuildPtrToInt(ctx.currbuilder, vid, LLVMTypeOf(vid), toStringz("link"));
			}
		}
		return null;
	}

	debug {
		override void debugPrint(int indent) {
			writeTabs(indent);
			writeln("Unary ", to!string(type));
			node.debugPrint(indent + 1);
		}
	}
}

class AstNodeIf : AstNode {
	AstNode cond;
	AstNode body_;
	AstNode else_;
	AstNodeFunction parent;

	this(AstNode cond, AstNode body_, AstNode else_) {
		this.cond = cond;
		this.body_ = body_;
		this.else_ = else_;
	}

	override void analyze(AnalyzerScope s, Type neededType) {
		this.parent = s.ctx.currentFunc;
		// auto boolType = new TypeBasic(BasicType.t_bool);
		this.cond.analyze(s, new TypeUnknown());
		if(!new TypeBasic(BasicType.t_bool).assignable(this.cond.getType(s))) {
			/*s.ctx.addError("If requires a boolean for it's condition, but '"
				~ this.cond.getType(s).toString() ~ "' is not convertible to bool.",
				this.where);*/
		}

		this.body_.analyze(s, new TypeUnknown());

		if(this.else_ !is null) {
			auto prevHadReturn = s.hadReturn;
			this.else_.analyze(s, new TypeUnknown());
			s.hadReturn = prevHadReturn && s.hadReturn;
		}
		else s.hadReturn = false;
	}

	override LLVMValueRef gen(AnalyzerScope s) {
		GenerationContext ctx = s.genctx;

		LLVMBasicBlockRef thenBlock =
			LLVMAppendBasicBlock(
				ctx.currfunc,
				toStringz("then"));
				
		LLVMBasicBlockRef elseBlock =
			LLVMAppendBasicBlock(
				ctx.currfunc,
				toStringz("else"));

		LLVMBasicBlockRef endBlock =
			LLVMAppendBasicBlock(
				ctx.currfunc,
				toStringz("end"));

		LLVMBuildCondBr(
			ctx.currbuilder,
			cond.gen(s),
			thenBlock,
			elseBlock
		);
		
		{
			LLVMPositionBuilderAtEnd(s.genctx.currbuilder, thenBlock);
			ctx.currbb = thenBlock;

			AnalyzerScope ss = new AnalyzerScope(s.ctx, s.genctx, s);
			if(body_ !is null) body_.gen(ss);

			if(!ss.hadReturn) LLVMBuildBr(ss.genctx.currbuilder, endBlock);
		}
		{
			LLVMPositionBuilderAtEnd(s.genctx.currbuilder, elseBlock);
			ctx.currbb = elseBlock;

			AnalyzerScope ss = new AnalyzerScope(s.ctx, s.genctx, s);
			if(else_ !is null) else_.gen(ss);

			if(!ss.hadReturn) LLVMBuildBr(ss.genctx.currbuilder, endBlock);
		}

		LLVMPositionBuilderAtEnd(s.genctx.currbuilder, endBlock);
		ctx.currbb = endBlock;

		return null;
	}

	override TList getTokens(string caller) {
		TList toret = new TList();
		toret.insertBack(new Token(basic,"if"));
		toret.insertBack(new Token(basic,"(",TokType.tok_lpar));
		toret.insertBack(cond.getTokens("AstNodeIf"));
		toret.insertBack(new Token(basic,")",TokType.tok_rpar));
		toret.insertBack(new Token(basic,"{"));
		toret.insertBack(body_.getTokens("AstNodeIf"));
		toret.insertBack(new Token(basic,"}"));
		return toret;
	}
	
	debug {
		override void debugPrint(int indent) {
			writeTabs(indent);
			writeln("If:");
			cond.debugPrint(indent + 1);
			writeTabs(indent);
			writeln("^Then:");
			body_.debugPrint(indent + 1);
			if(else_ !is null) {
				writeTabs(indent);
				writeln("^Else:");
				body_.debugPrint(indent + 1);
			}
		}
	}
}

class AstNodeWhile : AstNode {
	AstNode cond;
	AstNode body_;
	AstNodeFunction parent;

	this(AstNode cond, AstNode body_) {
		this.cond = cond;
		this.body_ = body_;
	}

	override void analyze(AnalyzerScope s, Type neededType) {
		this.parent = s.ctx.currentFunc;
		this.body_.analyze(s, neededType);
	}

	override LLVMValueRef gen(AnalyzerScope s) {
		auto ctx = s.genctx;
		LLVMBasicBlockRef _while;
		LLVMBasicBlockRef _after;

		ctx.basicblocks_count += 1;
		_while = LLVMAppendBasicBlock(s.genctx.gfuncs[parent.decl.name],toStringz("_while"~to!string(ctx.basicblocks_count)));
		ctx.basicblocks_count += 1;
		_after = LLVMAppendBasicBlock(s.genctx.gfuncs[parent.decl.name],toStringz("_after"~to!string(ctx.basicblocks_count)));

		ctx.exitbb = _after;
		ctx.thenbb = _while;
		ctx.whileexitbb = _after;

		LLVMValueRef cond_as_cmp = cond.gen(s);
		LLVMBuildCondBr(ctx.currbuilder,cond_as_cmp,_while,_after);
		
		LLVMPositionBuilderAtEnd(s.genctx.currbuilder,_while);
		ctx.currbb = _while;

		if(body_.instanceof!(AstNodeBlock)) {
			AstNodeBlock block = cast(AstNodeBlock)body_;
			for(int i=0; i<block.nodes.length; i++) {
				block.nodes[i].gen(s);
			}
		}
		else body_.gen(s);

		if(!ctx.break_inserted) LLVMBuildCondBr(ctx.currbuilder,cond.gen(s), _while, _after);
		ctx.break_inserted = false;

		LLVMPositionBuilderAtEnd(ctx.currbuilder, _after);
		ctx.currbb = _after;

		return null;
	}

	override TList getTokens(string caller) {
		TList toret = new TList();
		toret.insertBack(new Token(basic,"while"));
		toret.insertBack(new Token(basic,"(",TokType.tok_lpar));
		toret.insertBack(cond.getTokens("AstNodeWhile"));
		toret.insertBack(new Token(basic,")",TokType.tok_rpar));
		toret.insertBack(new Token(basic,"{"));
		toret.insertBack(body_.getTokens("AstNodeWhile"));
		toret.insertBack(new Token(basic,"}"));
		return toret;
	}

	debug {
		override void debugPrint(int indent) {
			writeTabs(indent);
			writeln("While:");
			cond.debugPrint(indent + 1);
			writeTabs(indent);
			writeln("^Body:");
			body_.debugPrint(indent + 1);
		}
	}
}

class AstNodeAsm : AstNode {
	string value;

	this(string value) {
		this.value = value[1..$-1];
	}

	override void analyze(AnalyzerScope s, Type neededType) {}
	override LLVMValueRef gen(AnalyzerScope s) {
		auto ctx = s.genctx;
		LLVMTypeRef functy = LLVMFunctionType(LLVMVoidType(), null,cast(uint)0, false);
		LLVMValueRef asmc = LLVMGetInlineAsm(
			functy,
			cast(char*)toStringz(value),
			cast(size_t)value.length,
			cast(char*)toStringz(""),
			cast(size_t)0,
			true,
			false,
			LLVMInlineAsmDialectIntel
		);
		return LLVMBuildCall2(ctx.currbuilder, functy, asmc, null, cast(uint)0, toStringz(""));
	}
}

class AstNodeBlock : AstNode {
	AstNode[] nodes;

	this(AstNode[] nodes) {
		this.nodes = nodes;
	}

	override void analyze(AnalyzerScope s, Type neededType) {
		auto newScope = new AnalyzerScope(s.ctx, s.genctx, s);
		newScope.returnType = s.returnType;
		newScope.neededReturnType = s.neededReturnType;

		foreach(node; this.nodes) {
			node.analyze(newScope, neededType);
			if(newScope.hadReturn) break;
		}

		s.returnType = newScope.returnType;
		s.hadReturn = s.hadReturn || newScope.hadReturn;
	}

	override LLVMValueRef gen(AnalyzerScope s) {
		auto newScope = new AnalyzerScope(s.ctx, s.genctx, s);
		foreach(node; nodes) {
			node.gen(newScope);
			if(newScope.hadReturn) break;
		}
		s.hadReturn = s.hadReturn || newScope.hadReturn;
		return cast(LLVMValueRef)0;
	}

	override TList getTokens(string caller) {
		TList toret = new TList();
		for(int i=0; i<nodes.length; i++) {
			toret.insertBack(nodes[i].getTokens("AstNodeBlock"));
		}
		return toret;
	}

	debug {
		override void debugPrint(int indent) {
			writeTabs(indent);
			writeln("Block: ", nodes.length, " statements");
			foreach(AstNode node; nodes) {
				node.debugPrint(indent + 1);
			}
		}
	}
}

class AstNodeReturn : AstNode {
	AstNodeFunction parent;
	AstNode value;
	LLVMBasicBlockRef retbb;

	this(SourceLocation where, AstNode value, AstNodeFunction parent) { 
		this.value = value;
		this.parent = parent;
		this.where = where;
	}

	override void analyze(AnalyzerScope s, Type neededType) {
		this.parent = s.ctx.currentFunc;
		
		Type t = new TypeVoid();

		if(this.value !is null) {
			this.value.analyze(s, s.neededReturnType);
			t = this.value.getType(s);
		}

		// writeln(t);
		if(s.neededReturnType.instanceof!TypeUnknown) {
			s.hadReturn = true;
			s.returnType = t;
		}
		else if(!s.neededReturnType.assignable(t)) {
			if(!value.instanceof!AstNodeIndex && !value.instanceof!AstNodeUnary) {
				/*s.ctx.addError("Wrong return type: expected '" ~ s.neededReturnType.toString()
					~ "', got: '" ~ t.toString() ~ '\'', this.where);*/
				s.hadReturn = true;
			}
			else s.hadReturn = true;
		}
		else {
			s.hadReturn = true;
			s.returnType = t;
		}

		parent.returns ~= ReturnStmt(this, s);
	}

	override LLVMValueRef gen(AnalyzerScope s) {
		GenerationContext ctx = s.genctx;
		LLVMValueRef v;

		if(value !is null) {
			LLVMValueRef val;
			if(AstNodeIndex ind = value.instanceof!AstNodeIndex) {
				val = LLVMBuildPtrToInt(
					ctx.currbuilder,
					value.gen(s),
					ctx.getLLVMType(parent.type.ret, s),
					toStringz("ptrtoint")
				);
			}
			else val = value.gen(s);
			parent.genReturns ~= ReturnGenStmt(ctx.currbb, val);
			v = LLVMBuildBr(ctx.currbuilder, parent.exitBlock);
		}
		else {
			v = LLVMBuildBr(ctx.currbuilder, parent.exitBlock);
		}

		s.hadReturn = true;
		return v;
	}

	override TList getTokens(string caller) {
		TList toret = new TList();
		toret.insertBack(new Token(basic,"ret"));
		toret.insertBack(value.getTokens("AstNodeReturn"));
		toret.insertBack(new Token(basic,";",TokType.tok_semicolon));
		return toret;
	}

	debug {
		override void debugPrint(int indent) {
			writeTabs(indent);
			writeln("Return: ", value is null ? "void" : "");
			if(value !is null) {
				value.debugPrint(indent  + 1);
			}
		}
	}
}

class AstNodeIndex : AstNode {
	AstNode base;
	AstNode index;
	Type needed;

	this(AstNode base, AstNode index) {
		this.base = base;
		this.index = index;
	}

	override void analyze(AnalyzerScope s, Type neededType) {
		needed = neededType;
	}

	LLVMValueRef genForSet(AnalyzerScope s) {
		auto ctx = s.genctx;

		auto baseg = base.gen(s);
		if(LLVMGetTypeKind(LLVMTypeOf(baseg)) == LLVMPointerTypeKind) {
			return LLVMBuildGEP(
				ctx.currbuilder,
				baseg,
				[index.gen(s)].ptr,
				cast(uint)1,
				toStringz("getelbyix_gfs")
			);
		}
		else if(LLVMGetTypeKind(LLVMTypeOf(baseg)) == LLVMArrayTypeKind) {
			return LLVMBuildGEP(
				ctx.currbuilder,
				baseg,
				[LLVMConstInt(LLVMInt32Type(),cast(ulong)0,true),index.gen(s)].ptr,
				2,
				toStringz("get_el_by_index_genforset")
			);
		}
		else if(LLVMGetTypeKind(LLVMTypeOf(baseg)) == LLVMStructTypeKind) {
			AstNodeString ind = index.instanceof!AstNodeString;
			string v = ind.value[0..$-1];
			AstNodeIden id = base.instanceof!AstNodeIden;
			return LLVMBuildStructGEP(
				ctx.currbuilder,
				baseg,
				cast(uint)ctx.gstructs.getV(
					v, 
					ctx.gstructs.structs[id.name]
				),
				toStringz("sgep")
			);
		}

		/*return LLVMBuildExtractElement(
			ctx.currbuilder,
			baseg,
			index.gen(s),
			toStringz("get_el_by_index_genforset2")
		);*/
		if(AstNodeIden id = base.instanceof!AstNodeIden) {
			baseg = ctx.gstack[id.name];
		}
		return LLVMBuildInBoundsGEP(
			ctx.currbuilder,
			baseg,
			[LLVMConstInt(LLVMInt32Type(),0,false),index.gen(s)].ptr,
			2,
			toStringz("getel")
		);
	}

	override LLVMValueRef gen(AnalyzerScope s) {
		GenerationContext ctx = s.genctx;

		auto baseg = base.gen(s);
		if(LLVMGetTypeKind(LLVMTypeOf(baseg)) == LLVMPointerTypeKind) {
			return LLVMBuildLoad(ctx.currbuilder, LLVMBuildGEP(
				ctx.currbuilder,
				baseg,
				[index.gen(s)].ptr,
				1,
				toStringz("get_el_by_index_astnodeindex")
			), toStringz("temp"));
		}
		if(AstNodeString ss = index.instanceof!AstNodeString) {
			if(AstNodeIden id = base.instanceof!AstNodeIden) {
				auto loaded = LLVMBuildLoad(
					ctx.currbuilder,
					ctx.gstack[id.name],
					toStringz("load")
				);
				return LLVMBuildExtractElement(
					ctx.currbuilder,
					loaded,
					LLVMConstInt(
						LLVMInt32Type(), 
						cast(ulong)ctx.gstructs.getV(
							ss.value[0..$-1], 
							ctx.gstructs.structs[id.name]
						), 
						false
					),
					toStringz("get_el_by_index_struct")
				);
			}
		}
		/*return LLVMBuildExtractElement(
			ctx.currbuilder,
			baseg,
			index.gen(s),
			toStringz("get_el_by_index_astnodeindex2")
		);*/
		if(AstNodeIden id = base.instanceof!AstNodeIden) {
			baseg = ctx.gstack[id.name];
		}
		return LLVMBuildLoad(ctx.currbuilder, LLVMBuildInBoundsGEP(
				ctx.currbuilder,
				baseg,
				[LLVMConstInt(LLVMInt32Type(),0,false),index.gen(s)].ptr,
				2,
				toStringz("getel")
		), toStringz("load"));
	}

	debug {
		override void debugPrint(int indent) {
			writeTabs(indent);
			writeln("Index:");
			base.debugPrint(indent + 1);
			writeTabs(indent);
			writeln("^By:");
			index.debugPrint(indent + 1);
		}
	}
}

class AstNodeIden : AstNode {
	string name;

	Type type;

	this(SourceLocation where, string name) {
		this.where = where;
		this.name = name;
	}

	override LLVMValueRef gen(AnalyzerScope s) {
		auto ctx = s.genctx;
		// _Ravef4exit, not exit! ctx.mangleQualifiedName([name], true) -> string
		// TODO: Check if the global variable is referring to a function
		if(s.genctx.gstack.setVar) {
			s.genctx.gstack.setVar = false;
			return ctx.gstack[name];
		}
		else {
			if(s.genctx.gstack.isGlobal(name)) {
				// Global var
				if(s.genctx.currbuilder != null) {
					return s.genctx.getValueByPtr(name);
				}
				else if(s.genctx.gstack.isLocal(name)) return s.genctx.gstack[name];
			}
			else if(s.genctx.gargs.isArg(name)) {
				return LLVMGetParam(s.genctx.currfunc, cast(uint)s.genctx.gargs[name]);
			}
			else {
				// Local var
				return s.genctx.getValueByPtr(name);
			}
		}
		assert(0);
	}

	override Type getType(AnalyzerScope s) {
		return type;
	}

	override void analyze(AnalyzerScope s, Type neededType) {
		auto v = s.get(name);
		if(v !is null && neededType !is null) {
			type = v.type;
		}
		else {
			auto t = s.ctx.typeContext.getType(name);
			if(t !is null) type = new TypeType(t);
			else {
				//s.ctx.addError("Unknown identifier: \""~name~"\"",this.where);
			}
		}
	}

	override TList getTokens(string caller) {
		TList toret = new TList();
		toret.insertBack(new Token(basic,name,TokType.tok_id));
		return toret;
	}

	debug {
		override void debugPrint(int indent) {
			writeTabs(indent);
			writeln("Identifier: ", name);
		}
	}
}

class AstNodeNamespace : AstNode {
	string[] names;
	AstNode[] nodes;

	this(string name, AstNode[] nodes) {
		this.names ~= name;
		this.nodes = nodes.dup;
	}

	override void analyze(AnalyzerScope s, Type neededType) {
		for(int i=0; i<nodes.length; i++) {
			AstNode currnode = nodes[i];

			if(AstNodeDecl decl = currnode.instanceof!AstNodeDecl) {
				decl.analyze(s,neededType);
			}
			else if(AstNodeFunction func = currnode.instanceof!AstNodeFunction) {
				func.analyze(s,neededType);
			}
			else if(AstNodeNamespace sp = currnode.instanceof!AstNodeNamespace) {
				sp.analyze(s,neededType);
			}
			else if(AstNodeStruct st = currnode.instanceof!AstNodeStruct) {
				st.analyze(s,neededType);
			}
		}
	}
	override LLVMValueRef gen(AnalyzerScope s) {
		GenerationContext ctx = s.genctx;

		for(int i=0; i<nodes.length; i++) {
			AstNode currnode = nodes[i];

			if(AstNodeDecl decl = currnode.instanceof!AstNodeDecl) {
				string oldname = decl.decl.name;
				decl.decl.name = namespacesToGenName(names,decl.decl.name,"var");
				decl.gen(s);
				string newname = namespacesToVarName(names,oldname);
				ctx.gstack.rename(decl.decl.name,newname);
			}
			else if(AstNodeFunction func = currnode.instanceof!AstNodeFunction) {
				string oldname = func.decl.name;
				func.decl.name = namespacesToGenName(names,oldname,"func");
				func.gen(s);
				string newname = namespacesToVarName(names,oldname);
				ctx.gfuncs.rename(func.decl.name,newname);
			}
			else if(AstNodeNamespace sp = currnode.instanceof!AstNodeNamespace) {
				sp.names = names ~ sp.names;
				sp.gen(s);
			}
			else if(AstNodeStruct st = currnode.instanceof!AstNodeStruct) {
				string oldname = st.name;
				st.name = namespacesToVarName(names,oldname);
				st.gen(s);
			}
		}

		return null;
	}
}

class AstNodeUsing : AstNode {
	string namespace;

	this(string namespace) {
		this.namespace = namespace;
	}

	override void analyze(AnalyzerScope s, Type neededType) {}
	override LLVMValueRef gen(AnalyzerScope s) {
		// Example: "using std;"
		GenerationContext ctx = s.genctx;
		
		for(int i=0; i<keys(ctx.gstack.newnames).length; i++) {
			if(firstNamespaceEqual(namespace,ctx.gstack.newnames.keys()[i])) {
				ctx.gstack.newnames[removeFirstNamespace(keys(ctx.gstack.newnames)[i])] = ctx.gstack.newnames[keys(ctx.gstack.newnames)[i]];
				ctx.gstack.newnames.remove(keys(ctx.gstack.newnames)[i]);
			}
		}

		for(int i=0; i<keys(ctx.gfuncs.newfuncs).length; i++) {
			if(firstNamespaceEqual(namespace,ctx.gfuncs.newfuncs.keys()[i])) {
				ctx.gfuncs.newfuncs[removeFirstNamespace(keys(ctx.gfuncs.newfuncs)[i])] = ctx.gfuncs.newfuncs[keys(ctx.gfuncs.newfuncs)[i]];
				ctx.gfuncs.newfuncs.remove(keys(ctx.gfuncs.newfuncs)[i]);
			}
		}

		return null;
	}
}

class AstNodeDecl : AstNode {
	VariableDeclaration decl;
	Type actualType;
	AstNode value; // can be null!
	bool isInImport = false;

	this(VariableDeclaration decl, AstNode value) {
		this.decl = decl;
		this.value = value;
	}

	override void analyze(AnalyzerScope s, Type neededType) {
		actualType = this.decl.type.get(s);
		if(this.value !is null) {
			this.value.analyze(s, actualType);
			if(instanceof!TypeUnknown(actualType))
				actualType = this.value.getType(s);
		}
		s.vars[decl.name] = new ScopeVar(actualType);
	}

	override LLVMValueRef gen(AnalyzerScope s) {
		GenerationContext ctx = s.genctx;
		if(ctx.gstack.isGlobal(decl.name)) {
			s.ctx.addError("The variable " ~ decl.name ~ " has been declared multiple times!",this.where);
		}
		if(s.genctx.currbuilder == null) {
			// Global var
			LLVMValueRef global;
			if(AstNodeInt i = value.instanceof!AstNodeInt) {
				global = ctx.createGlobal(
					ctx.getLLVMType(decl.type,s),
					value.gen(s),
					decl.name,
					decl.isExtern
				);
			}
			else if(AstNodeString str = value.instanceof!AstNodeString) {
				global = ctx.createGlobal(
					ctx.getLLVMType(decl.type,s),
					value.gen(s),
					decl.name,
					decl.isExtern
				);
			}
			else if(AstNodeChar ch = value.instanceof!AstNodeChar) {
				global = ctx.createGlobal(
					ctx.getLLVMType(decl.type,s),
					value.gen(s),
					decl.name,
					decl.isExtern
				);
			}
			else if(AstNodeFloat fl = value.instanceof!AstNodeFloat) {
				global = ctx.createGlobal(
					ctx.getLLVMType(decl.type,s),
					value.gen(s),
					decl.name,
					decl.isExtern
				);
			}
			else if(AstNodeBool bl = value.instanceof!AstNodeBool) {
				global = ctx.createGlobal(
					ctx.getLLVMType(decl.type,s),
					value.gen(s),
					decl.name,
					decl.isExtern
				);
			}
			else {
			global = ctx.createGlobal(
				ctx.getLLVMType(decl.type,s),
				LLVMConstNull(ctx.getLLVMType(decl.type,s)),
				decl.name,
				decl.isExtern
			);

			LLVMTypeRef ret_type = LLVMFunctionType(
		    	LLVMVoidType(),
		    	null,
		    	0,
		    	false
	    	);

			string stru = to!string(fromStringz(LLVMPrintTypeToString(ctx.getLLVMType(decl.type,s))));

			if(indexOf(stru,"%") == -1 && value is null) return global;

			LLVMValueRef func = LLVMAddFunction(
				ctx.mod,
				toStringz("_Ravei"~to!string((decl.name~"_init").length)~decl.name~"_init"),
				ret_type
			);

			ctx.currfunc = func;

			ctx.gstack.gen_init(ctx);

			if(!decl.isExtern) {
				auto entry = LLVMAppendBasicBlock(func,toStringz("entry"));
				ctx.currbuilder = LLVMCreateBuilder();
				LLVMPositionBuilderAtEnd(ctx.currbuilder, entry);

			if(indexOf(stru,"%") != -1) {
				if(indexOf(stru,"type") == -1) {
					string stru_2 = stru.replace("%","").replace("*","");
					ctx.gstructs.structs[decl.name] = stru_2;
				}
			}

			if(AtstNodeName n = decl.type.instanceof!AtstNodeName) {
				if(n.name in ctx.gstructs.ss) {
					ctx.gstructs.structs[decl.name] = n.name;

					for(int i=0; i<ctx.gstructs.variables[n.name].length; i++) {
						if(ctx.gstructs.variables[n.name][i].value !is null) LLVMBuildStore(
							ctx.currbuilder,
							ctx.gstructs.variables[n.name][i].value.gen(s),
							LLVMBuildStructGEP(
								ctx.currbuilder,
								global,
								ctx.gstructs.getV(ctx.gstructs.variables[n.name][i].decl.name, n.name),
								toStringz("struct_gep")
							)
						);
						else {
							if(AtstNodeName ty = ctx.gstructs.variables[n.name][i].decl.type.instanceof!AtstNodeName) {
								if(ty.name in ctx.gstructs.ss) { // It's struct!
									auto alloc = LLVMBuildAlloca(
												ctx.currbuilder,
												ctx.gstructs.getS(ty.name),
												toStringz("temp_struct")
											);
									auto strucg = LLVMBuildStructGEP(
												ctx.currbuilder,
												global,
												ctx.gstructs.getV(ctx.gstructs.variables[n.name][i].decl.name, n.name),
												toStringz("struct_gep")
											);
									LLVMValueRef allocc;
									if(LLVMTypeOf(alloc) == LLVMTypeOf(strucg)) {
										allocc = LLVMBuildLoad(
											ctx.currbuilder,
											alloc,
											toStringz("alloc")
										);
									}
									else allocc = alloc;
									LLVMBuildStore(
											ctx.currbuilder,
											allocc,
											strucg
									);
								}
							}
						}
					}
				}
			}

			if(value !is null) LLVMBuildStore(
				ctx.currbuilder,
				value.gen(s),
				global
			);
			LLVMBuildRetVoid(ctx.currbuilder);
			}

			ctx.currfunc = null;

			ctx.presets.add(func);

			ctx.currbuilder = null;
			}

			return global;
		}
		else {
			// Local var
			string stru = to!string(fromStringz(LLVMPrintTypeToString(ctx.getLLVMType(decl.type,s))));
			if(indexOf(stru,"%") != -1) {
				if(indexOf(stru,"type") == -1) {
					string stru_2 = stru.replace("%","").replace("*","");
					ctx.gstructs.structs[decl.name] = stru_2;
				}
			}
			if(AtstNodeName n = decl.type.instanceof!AtstNodeName) {
				if(n.name in ctx.gstructs.ss) {
					// Structure
					ctx.gstructs.structs[decl.name] = n.name;
					auto cl = ctx.createLocal(
						ctx.getLLVMType(decl.type, s),
						null,
						decl.name
					);

					if(n.name in ctx.gstructs.variables) {
					for(int i=0; i<ctx.gstructs.variables[n.name].length; i++) {
						if(ctx.gstructs.variables[n.name][i].value !is null) LLVMBuildStore(
							ctx.currbuilder,
							ctx.gstructs.variables[n.name][i].value.gen(s),
							LLVMBuildStructGEP(
								ctx.currbuilder,
								cl,
								ctx.gstructs.getV(ctx.gstructs.variables[n.name][i].decl.name, n.name),
								toStringz("struct_gep")
							)
						);
						else {
							if(AtstNodeName ty = ctx.gstructs.variables[n.name][i].decl.type.instanceof!AtstNodeName) {
								if(ty.name in ctx.gstructs.ss) { // It's struct!
									auto alloc = LLVMBuildAlloca(
												ctx.currbuilder,
												ctx.gstructs.getS(ty.name),
												toStringz("temp_struct")
											);
									auto strucg = LLVMBuildStructGEP(
												ctx.currbuilder,
												cl,
												ctx.gstructs.getV(ctx.gstructs.variables[n.name][i].decl.name, n.name),
												toStringz("struct_gep")
											);
									LLVMValueRef allocc;
									if(LLVMTypeOf(alloc) == LLVMTypeOf(strucg)) {
										allocc = LLVMBuildLoad(
											ctx.currbuilder,
											alloc,
											toStringz("alloc")
										);
									}
									else allocc = alloc;
									LLVMBuildStore(
											ctx.currbuilder,
											allocc,
											strucg
									);
								}
							}
						}
					}}

					return cl;
				}
			}
			if(!decl.isExtern) {
				if(value is null) return ctx.createLocal(
					ctx.getLLVMType(decl.type, s),
					null,
					decl.name
				);
				return ctx.createLocal(
					ctx.getLLVMType(decl.type, s),
					value.gen(s),
					decl.name
				);
			}
			auto gl = LLVMAddGlobal(ctx.mod, ctx.getLLVMType(decl.type,s), toStringz(decl.name));
			LLVMSetLinkage(gl, LLVMExternalLinkage);
			ctx.gstack.addGlobal(gl, decl.name);
			return gl;
		}
	}

	override TList getTokens(string caller) {
		TList toret = new TList();
		if(!isInImport) {
			toret.insertBack(new Token(basic,decl.name,TokType.tok_id));
			toret.insertBack(new Token(basic,":",TokType.tok_type));
			toret.insertBack(new Token(basic,decl.type.toString(),TokType.tok_id));
			if(value !is null) {
				toret.insertBack(new Token(basic,"=",TokType.tok_equ));
				toret.insertBack(value.getTokens("AstNodeDecl"));
			}
			toret.insertBack(new Token(basic,";",TokType.tok_semicolon));
			return toret;
		}
		toret.insertBack(new Token(basic,"extern"));
		toret.insertBack(new Token(basic,decl.name,TokType.tok_id));
		toret.insertBack(new Token(basic,":",TokType.tok_type));
		toret.insertBack(new Token(basic,decl.type.toString(),TokType.tok_id));
		toret.insertBack(new Token(basic,";",TokType.tok_semicolon));
		return toret;
	}

	debug {
		override void debugPrint(int indent) {
			writeTabs(indent);
			writeln("Variable Declaration: ", decl.toString(), actualType);
			if(value !is null) {
				writeTabs(indent);
				writeln("^Default Value:");
				value.debugPrint(indent + 1);
			}
		}
	}
}

class AstNodeLabel : AstNode {
	string name;

	override LLVMValueRef gen(AnalyzerScope s) {
		auto ctx = s.genctx;
		LLVMValueRef a;
		return a;
	}

	debug {
		override void debugPrint(int indent) {
			writeTabs(indent);
			writeln("Label: ", name);
		}
	}
}

class AstNodeBreak : AstNode {
	string label; /* optional */

	override void analyze(AnalyzerScope s, Type neededType) {}

	override LLVMValueRef gen(AnalyzerScope s) {
		GenerationContext ctx = s.genctx;
		ctx.break_inserted = true;
		return LLVMBuildBr(ctx.currbuilder, ctx.whileexitbb);
	}

	debug {
		override void debugPrint(int indent) {
			writeTabs(indent);
			writeln("Break");
		}
	}
}

class AstNodeContinue : AstNode {
	string label; /* optional */

	override LLVMValueRef gen(AnalyzerScope s) {
		auto ctx = s.genctx;
		return LLVMBuildBr(
			ctx.currbuilder,
			ctx.thenbb
		);
	}

	debug {
		override void debugPrint(int indent) {
			writeTabs(indent);
			writeln("Continue");
		}
	}
}

class AstNodeSizeof : AstNode {
	AtstNode val;

	this(AtstNode value) {
		this.val = value;
	}

	override void analyze(AnalyzerScope s, Type neededType) {}
	override LLVMValueRef gen(AnalyzerScope s) {
		GenerationContext ctx = s.genctx;
		return castTo(ctx,LLVMSizeOf(ctx.getLLVMType(val,s)),LLVMInt32Type());
	}
}

class AstNodeCast : AstNode {
	AtstNode type;
	AstNode val;

	this(AtstNode t, AstNode v) {
		this.type = t;
		this.val = v;
	}

	override void analyze(AnalyzerScope s, Type neededType) {}
	override LLVMValueRef gen(AnalyzerScope s) {
		GenerationContext ctx = s.genctx;
		return castTo(ctx, val.gen(s), ctx.getLLVMType(type,s));
	}
	override TList getTokens(string caller) {
		TList toret = new TList();
		toret.insertBack(new Token(basic,"cast"));
		toret.insertBack(new Token(basic,"("));
		toret.insertBack(new Token(basic,type.toString()));
		toret.insertBack(new Token(basic,")"));
		toret.insertBack(val.getTokens("AstNodeCast"));
		return toret;
	}
}

class AstNodePtoi : AstNode {
	AstNode val;
	
	this(AstNode val) {
		this.val = val;
	}

	override void analyze(AnalyzerScope s, Type neededType) {}
	override LLVMValueRef gen(AnalyzerScope s) {
		GenerationContext ctx = s.genctx;

		return LLVMBuildPtrToInt(
				ctx.currbuilder,
				val.gen(s),
				LLVMInt32Type(),
				toStringz("ptoi")
		);
	}
	override TList getTokens(string caller) {
		TList toret = new TList();
		toret.insertBack(new Token(basic,"ptoi"));
		toret.insertBack(new Token(basic,"("));
		toret.insertBack(val.getTokens("AstNodePtoi"));
		toret.insertBack(new Token(basic,")"));
		return toret;
	}
}

class AstNodeItop : AstNode {
	AstNode val; // Integer
	AtstNode ptrt; // Pointer type

	this(AstNode val, AtstNode p) {
		this.val = val;
		this.ptrt = p;
	}

	this(AstNode val) {
		this.val = val;
		this.ptrt = new AtstNodePointer(new AtstNodeVoid());
	}

	override void analyze(AnalyzerScope s, Type neededType) {}
	override LLVMValueRef gen(AnalyzerScope s) {
		GenerationContext ctx = s.genctx;

		return LLVMBuildIntToPtr(
			ctx.currbuilder,
			val.gen(s),
			ctx.getLLVMType(ptrt,s),
			toStringz("itop")
		);
	}
	override TList getTokens(string caller) {
		TList toret = new TList();
		toret.insertBack(new Token(basic,"itop"));
		toret.insertBack(new Token(basic,"("));
		toret.insertBack(val.getTokens("AstNodeItop"));
		toret.insertBack(new Token(basic,","));
		toret.insertBack(new Token(basic,ptrt.toString()));
		toret.insertBack(new Token(basic,")"));
		return toret;
	}
}

class AstNodeAlias : AstNode {
	string name;
	AstNode value;

	this(string name, AstNode value) {
		this.name = name;
		this.value = value;
	}
}

class AstNodeFuncCall : AstNode {
	AstNode func;
	TypeFunction funcType;
	AstNode[] args;

	this(SourceLocation where, AstNode func, AstNode[] args) {
		this.where = where;
		this.func = func;
		this.args = args;
	}

	override Type getType(AnalyzerScope s) {
		return this.funcType ? this.funcType.ret : new TypeUnknown();
	}

	override void analyze(AnalyzerScope s, Type neededType) {
		this.func.analyze(s, new TypeFunctionLike());
		auto funcType = this.func.getType(s);
		AstNodeFunction f = func.instanceof!AstNodeFunction;

		if(!funcType.instanceof!TypeFunction) {
			//s.ctx.addError("Cannot call a non-function value!", this.where);
			return;
		}

		if(this.func.instanceof!AstNodeGet) {
			AstNodeGet g = cast(AstNodeGet)this.func;
			if(!g.isNamespace) {
				// The get node is getting a field from a value,
				// so we add that to the first argument.
				this.args = g.value ~ this.args;
			}
			else {
				// The get node is resolving a name in a namespace,
				// do nothing.
			}
		}

		this.funcType = cast(TypeFunction)funcType;

		/*if(this.args.length != this.funcType.args.length) {
			s.ctx.addError("Invalid call to function, "
				~ to!string(this.funcType.args.length)
				~ " arguments are required, but got "
				~ to!string(this.args.length), this.where);
			return;
		}*/
		for(int i = 0; i < this.args.length; ++i) {
			try{this.args[i].analyze(s, this.funcType.args[i]);}
			catch(Error e) {}
		}

		if(!neededType.instanceof!TypeUnknown && !neededType.assignable(getType(s))) {
			s.ctx.addError("Type mismatch: the outer scope requires type '"
				~ neededType.toString()
				~ "', but function call returns type '"
				~ getType(s).toString() ~ "'.", this.where);
		}
	}

	override LLVMValueRef gen(AnalyzerScope s) {
		GenerationContext ctx = s.genctx;
		// _Ravef4exit, not exit! ctx.mangleQualifiedName([name], true) -> string
		AstNodeIden n = cast(AstNodeIden)func;

		bool ishavevararg = false;
		int num = 0;
		LLVMValueRef vararg = null;
		LLVMValueRef* llvm_args = cast(LLVMValueRef*)malloc(LLVMValueRef.sizeof * args.length);
		TypeFunction tf = ctx.gfuncs.getFType(s.ctx, n.name);
		for(int i = 0; i < args.length; i++) {
			if(AtstNodeName nn = ctx.gfuncs.funcs_args[i].instanceof!AtstNodeName) {
				if(nn.name == "args") {
					// VARARG OMG!!
					ishavevararg = true;
					vararg = LLVMBuildAlloca(
						ctx.currbuilder,
						LLVMArrayType(
							LLVMPointerType(
								LLVMInt8Type(),
								0,
							),
							cast(uint)args.length-(i+1)
						),
						toStringz("vararg")
					);
					/*
					LLVMBuildInBoundsGEP(
						ctx.currbuilder,
						vararg,
						[LLVMConstInt(LLVMInt32Type(),0,false),LLVMConstInt(LLVMInt32Type(),0,false)].ptr,
						2,
						toStringz("getel")
					);
					*/
				}
			}

			auto genv = args[i].gen(s);
			if(vararg == null) {
			if(tf.args.length>0) llvm_args[i] = genv;
			else if(tf.args.length>0 && tf.args[i].toString == "void*") {
				LLVMTypeKind kind = LLVMGetTypeKind(LLVMTypeOf(genv));
				if(kind == LLVMPointerTypeKind) {
					llvm_args[i] = LLVMBuildPointerCast(
						ctx.currbuilder,
						genv,
						LLVMPointerType(LLVMInt8Type(),0),
						toStringz("temp")
					);
				}
				else if(kind == LLVMIntegerTypeKind) {
					llvm_args[i] = LLVMBuildIntToPtr(
						ctx.currbuilder,
						genv,
						LLVMPointerType(LLVMInt8Type(),0),
						toStringz("temp2")
					);
				}
				else llvm_args[i] = genv;
			}
			else llvm_args[i] = genv;}
			else {
				LLVMBuildStore(
					ctx.currbuilder,
					castTo(ctx, args[i].gen(s), LLVMPointerType(LLVMInt8Type(),0)),
					LLVMBuildInBoundsGEP(
						ctx.currbuilder,
						vararg,
						[LLVMConstInt(LLVMInt32Type(),0,false),LLVMConstInt(LLVMInt32Type(),cast(ulong)num,false)].ptr,
						2,
						toStringz("gep")
					)
				);
				num += 1;
			}
		}
		if(vararg != null) {
			llvm_args[ctx.gfuncs.funcs_varargs[n.name]] = vararg;
			llvm_args[ctx.gfuncs.funcs_varargs[n.name]+1] 
			= LLVMConstInt(LLVMInt32Type(),cast(ulong)args.length-(ctx.gfuncs.funcs_varargs[n.name]+1),false);
		}
		if(ctx.gfuncs.getFType(s.ctx, n.name).ret.toString() == "void") {
			return LLVMBuildCall(
				ctx.currbuilder,
				ctx.gfuncs[n.name],
				llvm_args,
				cast(uint)args.length,
				toStringz("")
			);
		}
		return LLVMBuildCall(
			ctx.currbuilder,
			ctx.gfuncs[n.name],
			llvm_args,
			cast(uint)args.length,
			toStringz("call")
		);
	}

	override TList getTokens(string caller) {
		TList toret = new TList();
		AstNodeIden n = cast(AstNodeIden)func;
		toret.insertBack(new Token(basic,n.name,TokType.tok_id));
		toret.insertBack(new Token(basic,"(",TokType.tok_lpar));
		if(args.length>0) {
			for(int i=0; i<args.length; i++) {
				toret.insertBack(args[i].getTokens("AstNodeFuncCall"));
				if((i+1) != args.length) toret.insertBack(new Token(basic,",",TokType.tok_comma));
			}
		}
		toret.insertBack(new Token(basic,")",TokType.tok_lpar));
		if(caller == "AstNodeBlock") toret.insertBack(new Token(basic,";",TokType.tok_semicolon));
		return toret;
	}

	debug {
		override void debugPrint(int indent) {
			writeTabs(indent);
			writeln("Func Call:");
			func.debugPrint(indent + 1);
			writeTabs(indent);
			writeln("^Arguments:");
			foreach(AstNode arg; args)
				arg.debugPrint(indent + 1);
		}
	}
}


class AstNodeInt : AstNode {
	ulong value;
	Type valueType;

	this(SourceLocation where, ulong value) {
		this.where = where;
		this.value = value;
		this.valueType = null;
	}

	override Type getType(AnalyzerScope s) {
		return valueType;
	}

	private ulong pow2(char n) {
		ulong l = 2;
		while(n--) l *= 2;
		return l;
	}

	override void analyze(AnalyzerScope s, Type neededType) {
		if(instanceof!TypeUnknown(neededType)) {
			this.valueType = _getMinType();
		}
		else {
			if(!_isArithmetic(neededType)) {
				//s.ctx.addError("Expected " ~ neededType.toString() ~ ", got an integer", this.where);
			}
			this.valueType = neededType;
		}

		// writeln("int type ", this.valueType.toString());
	}

	private bool _isArithmetic(Type t) {
		if(auto b = t.instanceof!TypeBasic) {
			return true;
		}
		return false;
	}

	private Type _getMinType() {
		if(this.value > 0) {
			// might and might not work for pow2(N)/2
			// if(this.value < pow2(8)/2)  return new TypeBasic(BasicType.t_char);
			// if(this.value < pow2(8))    return new TypeBasic(BasicType.t_uchar);
			// if(this.value < pow2(16)/2) return new TypeBasic(BasicType.t_short);
			// if(this.value < pow2(16))   return new TypeBasic(BasicType.t_ushort);
			if(this.value < pow2(32)/2) return new TypeBasic(BasicType.t_int);
			if(this.value < pow2(32))   return new TypeBasic(BasicType.t_uint);
			if(this.value < pow2(64)/2) return new TypeBasic(BasicType.t_long);
			if(this.value < pow2(64))   return new TypeBasic(BasicType.t_ulong);
			assert(0);
		}
		else {
			// TODO implement something similar to above.
			return new TypeBasic(BasicType.t_int);
		}
	}

	override LLVMValueRef gen(AnalyzerScope s) {
		GenerationContext ctx = s.genctx;
		TypeBasic type = cast(TypeBasic)valueType;

		if(type is null) type = new TypeBasic(BasicType.t_int);

		switch(type.basic) {
			case BasicType.t_bool:
				return LLVMConstInt(LLVMInt1Type(),value,false);
			case BasicType.t_char:
				return LLVMConstInt(LLVMInt8Type(),value,true);
			case BasicType.t_uchar:
				return LLVMConstInt(LLVMInt8Type(),value,false);
			case BasicType.t_short:
				return LLVMConstInt(LLVMInt16Type(),value,true);
			case BasicType.t_ushort:
				return LLVMConstInt(LLVMInt16Type(),value,false);
			case BasicType.t_int:
				return LLVMConstInt(LLVMInt32Type(),value,true);
			case BasicType.t_uint:
				return LLVMConstInt(LLVMInt32Type(),value,false);
			case BasicType.t_long:
				return LLVMConstInt(LLVMInt64Type(),value,true);
			case BasicType.t_ulong:
				return LLVMConstInt(LLVMInt64Type(),value,false);
			case BasicType.t_float:
				return LLVMConstReal(LLVMFloatType(),cast(double)value);
			default: assert(0);
		}
	}

	override TList getTokens(string caller) {
		TList toret = new TList();
		toret.insertBack(new Token(basic,to!string(value)));
		return toret;
	}

	debug {
		override void debugPrint(int indent) {
			writeTabs(indent);
			writeln("Int: ", value);
		}
	}
}

class AstNodeFloat : AstNode {
	float value;

	this(SourceLocation where, float value) { this.where = where; this.value = value; }
	
	override Type getType(AnalyzerScope s) {
		return new TypeBasic(BasicType.t_float);
	}

	override void analyze(AnalyzerScope s, Type neededType) {}
	
	override LLVMValueRef gen(AnalyzerScope s) {
		auto ctx = s.genctx;
		return LLVMConstReal(LLVMFloatType(),cast(double)value);
	}

	override TList getTokens(string caller) {
		TList toret = new TList();
		toret.insertBack(new Token(basic,to!string(value)));
		return toret;
	}

	debug {
		override void debugPrint(int indent) {
			writeTabs(indent);
			writeln("Float: ", value);
		}
	}
}

class AstNodeString : AstNode {
	string value;

	this(SourceLocation where, string value) { this.where = where; this.value = value; }

	override Type getType(AnalyzerScope s) {
		return new TypePointer(new TypeBasic(BasicType.t_char));
	}

	override void analyze(AnalyzerScope s, Type neededType) {}

	override LLVMValueRef gen(AnalyzerScope s) {
		auto ctx = s.genctx;
		
		return LLVMBuildGlobalStringPtr(
			ctx.currbuilder,
			toStringz(value[0..$-1]),
			toStringz("str")
		);
	}

	override TList getTokens(string caller) {
		TList toret = new TList();
		toret.insertBack(new Token(basic,"\""~value~"\""));
		return toret;
	}

	debug {
		override void debugPrint(int indent) {
			writeTabs(indent);
			writeln("String: \"", value, '"');
		}
	}
}

class AstNodeChar : AstNode {
	char value;

	this(SourceLocation where, char value) { this.where = where; this.value = value; }

	override Type getType(AnalyzerScope s) {
		return new TypeBasic(BasicType.t_char);
	}

	override void analyze(AnalyzerScope s, Type neededType) {

	}

	override LLVMValueRef gen(AnalyzerScope s) {
		auto ctx = s.genctx;
		return LLVMConstInt(LLVMInt8Type(),value,false);
	}

	override TList getTokens(string caller) {
		TList toret = new TList();
		toret.insertBack(new Token(basic,"\'"~value~"\'"));
		return toret;
	}

	debug {
		override void debugPrint(int indent) {
			writeTabs(indent);
			writeln("Char: '", value, '\'');
		}
	}
}

