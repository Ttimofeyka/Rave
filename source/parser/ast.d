module parser.ast;

import std.stdio, std.conv;
import std.algorithm.iteration : map;
import std.algorithm : canFind;
import parser.generator.gen, lexer.tokens;
import llvm;
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

/// We have two separate syntax trees: for types and for values.

class AtstTypeContext {
	Type[string] types;

	this() {
		types["bool"] = new TypeBasic(BasicType.t_bool);
	}

	void setType(string name, Type type) {
		if(name in types) {
			writeln("Type already exists: ", name);
			return;
		}
		types[name] = type;
	}

	Type getType(string name) {
		if(name !in types) {
			// writeln("No such type: ", name);
			return null;
		}
		return types[name];
	}
}

// Abstract type syntax tree node.
class AtstNode {
	Type get(AnalyzerScope _ctx) { return new Type(); }

	debug {
		override string toString() const {
			return "?";
		}
	}
}

class AtstNodeVoid : AtstNode {
	override Type get(AnalyzerScope ctx) { return new TypeVoid(); }

	debug {
		override string toString() const {
			return "void";
		}
	}
}

// For inference
class AtstNodeUnknown : AtstNode {
	override Type get(AnalyzerScope ctx) { return new TypeUnknown(); }
	
	debug {
		override string toString() const {
			return "unknown";
		}
	}
}

class AtstNodeName : AtstNode {
	string name;
	this(string name) {
		this.name = name;
	}

	override Type get(AnalyzerScope s) { return s.ctx.typeContext.getType(name); }

	debug {
		override string toString() const {
			return name;
		}
	}
}

class AtstNodePointer : AtstNode {
	AtstNode node;

	this(AtstNode node) {
		this.node = node;
	}

	override Type get(AnalyzerScope ctx) { return new TypePointer(node.get(ctx)); }

	debug {
		override string toString() const {
			return node.toString() ~ "*";
		}
	}
}

class AtstNodeConst : AtstNode {
	AtstNode node;

	this(AtstNode node) {
		this.node = node;
	}

	override Type get(AnalyzerScope ctx) { return new TypeConst(node.get(ctx)); }

	debug {
		override string toString() const {
			return "const " ~ node.toString();
		}
	}
}

class AtstNodeArray : AtstNode {
	AtstNode node;
	ulong count;

	this(AtstNode node, ulong count) {
		this.node = node;
		this.count = count;
	}

	override Type get(AnalyzerScope ctx) { /* TODO */ return null; }

	debug {
		override string toString() const {
			if(count == 0) {
				return node.toString() ~ "[]";
			}
			else {
				return node.toString() ~ "[" ~ to!string(count)  ~ "]";
			}
		}
	}
}

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
		LLVMTypeRef* types = cast(LLVMTypeRef*)malloc(LLVMTypeRef.sizeof * variables.length);
		for(int i=0; i<variables.length; i++) {
			ctx.gstructs.addV(cast(uint)i, variables[i].decl.name, name);
			//types[i] = ctx.getLLVMType(variables[i].actualType, s);
			/*if(AtstNodeName n = variables[i].decl.type) {
				if(n.name in ctx.gstructs.structs) {
					auto vv = LLVMBuildAlloca(
						ctx.currbuilder,
						ctx.gstructs.getS(ctx.gstructs.structs[n.name]),
						toStringz("alloca_struct")
					);
					presets ~= LLVMBuildLoad(
						ctx.currbuilder,
						vv,
						toStringz("preset")
					);
				}
			}*/
		}
		ctx.gstructs.addS(LLVMVectorType(LLVMPointerType(LLVMInt8Type(),0), cast(uint)variables.length), name);
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

	LLVMTypeRef* paramTypes = null;

	this(SourceLocation loc, FunctionDeclaration decl, AstNode body_) {
		this.where = loc;
		this.decl = decl;
		this.body_ = body_;
	}

	override void analyze(AnalyzerScope s, Type) {
		auto retType = this.decl.signature.ret.get(s);
		Type[] argTypes = array(map!((a) => a.get(s))(this.decl.signature.args));
		type = new TypeFunction(retType, argTypes);

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
		type = new TypeFunction(retType, argTypes);
		s.vars[this.decl.name] = new ScopeVar(type, decl.isStatic, decl.isExtern);
		}

	private LLVMTypeRef* getParamsTypes(AnalyzerScope s) {
		GenerationContext ctx = s.genctx;
		if(paramTypes is null) {
			paramTypes = stackMemory.createBuffer!LLVMTypeRef(
				decl.signature.args.length
			);

			for(int i = 0; i < decl.signature.args.length; i++) {
				
				paramTypes[i] = ctx.getLLVMType(type.args[i],s);
				//char* ptype = LLVMPrintTypeToString(paramTypes[i]);
				//writeln(fromStringz(ptype));
				ctx.gargs.set(i, paramTypes[i], decl.argNames[i]);
			}
		}

		return paramTypes;
	}

	override LLVMValueRef gen(AnalyzerScope s) {
		GenerationContext ctx = s.genctx;
		builder = LLVMCreateBuilder();

		AstNodeBlock funcBody = cast(AstNodeBlock)body_;

		LLVMTypeRef retType = ctx.getLLVMType(type.ret,s);

		LLVMTypeRef funcType = LLVMFunctionType(
		    retType,
			getParamsTypes(s),
		    cast(uint)decl.signature.args.length,
		    false
	    );

		bool shouldMangle = true;
		foreach(mod; decl.decl_mods) {
			if(mod.name == "C") shouldMangle = false;
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
			else {
				s.ctx.addError("Unknown modifier: '" ~ mod.name ~ "'", this.where);
			}
		}

		LLVMValueRef func = LLVMAddFunction(
			ctx.mod,
			toStringz(
				shouldMangle
				? ctx.mangleQualifiedName([decl.name], true)
				: decl.name),
			funcType
		);

		ctx.gfuncs.add(decl.name, func, retType, type);
		ctx.currfunc = func;

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

class AstNodeGet : AstNode {
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
			writeln("Didn't find structure '",base,"'!");
			exit(-1);
		}
		if(isEqu) return LLVMBuildStructGEP(
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
		);
	}

	debug {
		override void debugPrint(int indent) {
			writeTabs(indent);
			writeln("Get(" ~ (isSugar?"->":".") ~ ") ", this.field);
			value.debugPrint(indent + 1);
		}
	}
}

class AstNodeBinary : AstNode {
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
				if(auto iden = lhs.instanceof!AstNodeIden) return LLVMBuildStore(
						ctx.currbuilder,
						rhs.gen(s),
						ctx.gstack[iden.name]
				);
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
											LLVMConstInt(LLVMInt32Type(), cast(ulong)ctx.gstructs.getV(ss.value[0..$-1], ctx.gstructs.structs[id.name]), false),
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
						return LLVMBuildInsertElement(
							ctx.currbuilder,
							iiden,
							rhs.gen(s),
							iindex,
							toStringz("set_arr_el")
						);
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
						rhs.gen(s),
						get.gen(s)
					);
					get.isEqu = false;
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
						tmp = LLVMBuildAdd(
							ctx.currbuilder,
							lhs.gen(s),
							rhs.gen(s),
							toStringz("tmp")
						);
					}
					else if (type == TokType.tok_shortmin) {
						tmp = LLVMBuildSub(
							ctx.currbuilder,
							lhs.gen(s),
							rhs.gen(s),
							toStringz("tmp")
						);
					}
					else if (type == TokType.tok_shortmul) {
						tmp = LLVMBuildMul(
							ctx.currbuilder,
							lhs.gen(s),
							rhs.gen(s),
							toStringz("tmp")
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
							rhs.gen(s),
							false 
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
				auto result = LLVMBuildAlloca(
					s.genctx.currbuilder,
					LLVMInt32Type(),
					toStringz("result")
				);
				LLVMValueRef operation;
				if(type == TokType.tok_plus) {
					operation = LLVMBuildAdd(
						ctx.currbuilder,
						lhs.gen(s),
						rhs.gen(s),
						toStringz("operation")
					);
				}
				else if(type == TokType.tok_minus) {
					operation = LLVMBuildSub(
						ctx.currbuilder,
						lhs.gen(s),
						rhs.gen(s),
						toStringz("operation")
					);
				}
				else if(type == TokType.tok_multiply) {
					operation = LLVMBuildMul(
						ctx.currbuilder,
						lhs.gen(s),
						rhs.gen(s),
						toStringz("operation")
					);
				}
				LLVMBuildStore(
					s.genctx.currbuilder,
					operation,
					result
				);
				auto loaded_result = LLVMBuildLoad(
					s.genctx.currbuilder,
					result,
					toStringz("r_loaded")
				);
				return loaded_result;
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
					rhs_g = castNum(ctx, rhs_g, LLVMTypeOf(lhs_g), false);
				}
				if(type == TokType.tok_equal) return LLVMBuildICmp(
					s.genctx.currbuilder,
					LLVMIntEQ,
					lhs_g,
					rhs_g,
					toStringz("icmp_eq")
				);
				return LLVMBuildICmp(
					s.genctx.currbuilder,
					LLVMIntNE,
					lhs_g,
					rhs_g,
					toStringz("icmp_ne")
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
				auto lhs_g = lhs.gen(s);
				auto rhs_g = rhs.gen(s);
				if(LLVMTypeOf(lhs_g) != LLVMTypeOf(rhs_g)) {
					rhs_g = castNum(ctx, rhs_g, LLVMTypeOf(lhs_g), false);
				}
				return LLVMBuildICmp(
					ctx.currbuilder,
					LLVMIntSGT,
					lhs_g,
					rhs_g,
					toStringz("less")
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
		if(!neededType.instanceof!TypeUnknown) {
			if(neededType.assignable(lhs.getType(s))
			&& neededType.assignable(rhs.getType(s))) {
				this.valueType = neededType;
			}
			else {
				/*s.ctx.addError("Bad binary operator types: expected '"
					~ neededType.toString() ~ "' but got '"
					~ lhs.getType(s).toString() ~ "' and '"
					~ rhs.getType(s).toString() ~ "'.", where);*/

				this.valueType = neededType;
			}
		}
		else { // neededType --> unknown.
			// TODO: Find best suitable type!
			this.valueType = lhs.getType(s);
		}
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

class AstNodeUnary : AstNode {
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
		auto ctx = s.genctx;
		if(type == TokType.tok_multiply) {
			auto val = node.gen(s);
			auto a = LLVMBuildAlloca(
				ctx.currbuilder,
				LLVMPointerType(LLVMTypeOf(val),0),
				toStringz("unary")
			);
			LLVMBuildStore(
				ctx.currbuilder,
				val,
				a
			);
			return a;
		}
		else if(type == TokType.tok_minus) {
			return LLVMBuildNeg(
				ctx.currbuilder,
				node.gen(s),
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

		ctx.thenbb = thenBlock;
		ctx.exitbb = endBlock;

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

		LLVMBuildCondBr(ctx.currbuilder,cond.gen(s), _while, _after);
		LLVMPositionBuilderAtEnd(ctx.currbuilder, _after);

		ctx.currbb = _after;

		return null;
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
	string value; // TODO

	override LLVMValueRef gen(AnalyzerScope s) {
		auto ctx = s.genctx;
		LLVMValueRef a;
		return a;
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
				s.ctx.addError("Wrong return type: expected '" ~ s.neededReturnType.toString()
					~ "', got: '" ~ t.toString() ~ '\'', this.where);
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
			parent.genReturns ~= ReturnGenStmt(ctx.currbb, value.gen(s));
			v = LLVMBuildBr(ctx.currbuilder, parent.exitBlock);
		}
		else {
			v = LLVMBuildBr(ctx.currbuilder, parent.exitBlock);
		}

		s.hadReturn = true;
		return v;
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
				1,
				toStringz("get_el_by_index")
			);
		}
		return LLVMBuildExtractElement(
			ctx.currbuilder,
			baseg,
			index.gen(s),
			toStringz("get_el_by_index")
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
				toStringz("get_el_by_index")
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
					LLVMConstInt(LLVMInt32Type(), cast(ulong)ctx.gstructs.getV(ss.value[0..$-1], ctx.gstructs.structs[id.name]), false),
					toStringz("get_el_by_index_struct")
				);
			}
		}
		return LLVMBuildExtractElement(
			ctx.currbuilder,
			baseg,
			index.gen(s),
			toStringz("get_el_by_index")
		);
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
			if(!neededType.assignable(v.type) && !neededType.instanceof!TypeUnknown) {
				s.ctx.addError("Type mismatch: variable '" ~ name ~ "' is of type "
					~ v.type.toString() ~ ", but " ~ neededType.toString() ~ " was expected.",
					this.where);
			}
			type = v.type;
		}
		else {
			auto t = s.ctx.typeContext.getType(name);
			if(t !is null) type = new TypeType(t);
			else {
				if(name != "sizeof") s.ctx.addError("Unknown binding: '" ~ name ~ "'.", this.where);
			}
		}
	}

	debug {
		override void debugPrint(int indent) {
			writeTabs(indent);
			writeln("Identifier: ", name);
		}
	}
}

class AstNodeDecl : AstNode {
	VariableDeclaration decl;
	Type actualType;
	AstNode value; // can be null!

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
		if(ctx.gstack.isVariable(decl.name)) {
			writeln("The variable " ~ decl.name ~ " has been declared multiple times!");
			exit(-1);
		}
		if(s.genctx.currbuilder == null) {
			// Global var
			auto global = ctx.createGlobal(
				ctx.getLLVMType(decl.type,s),
				LLVMConstNull(ctx.getLLVMType(decl.type,s)),
				decl.name
			);

			LLVMTypeRef ret_type = LLVMFunctionType(
		    	LLVMVoidType(),
		    	null,
		    	0,
		    	false
	    	);

			LLVMValueRef func = LLVMAddFunction(
				ctx.mod,
				toStringz("_Ravef"~to!string((decl.name~"_init").length)~decl.name~"_init"),
				ret_type
			);

			auto entry = LLVMAppendBasicBlock(func,toStringz("entry"));
			ctx.currbuilder = LLVMCreateBuilder();
			LLVMPositionBuilderAtEnd(ctx.currbuilder, entry);

			if(value !is null) LLVMBuildStore(
				ctx.currbuilder,
				value.gen(s),
				global
			);
			else {
				LLVMBuildStore(
					ctx.currbuilder,
					LLVMConstNull(LLVMGlobalGetValueType(global)),
					global
				);
			}
			LLVMBuildRetVoid(ctx.currbuilder);

			ctx.presets.add(func);

			return global;
		}
		else {
			// Local var
			if(AtstNodeName n = decl.type.instanceof!AtstNodeName) {
				if(n.name in ctx.gstructs.ss) {
					// Structure
					ctx.gstructs.structs[decl.name] = n.name;
					return ctx.createLocal(
						ctx.getLLVMType(decl.type, s),
						null,
						decl.name
					);
				}
			}
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

	override LLVMValueRef gen(AnalyzerScope s) {
		auto ctx = s.genctx;
		return LLVMBuildBr(ctx.currbuilder, ctx.exitbb);
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
	AstNode val;

	this(AstNode value) {
		this.val = value;
	}

	override void analyze(AnalyzerScope s, Type neededType) {}
	override LLVMValueRef gen(AnalyzerScope s) {
		GenerationContext ctx = s.genctx;
		return LLVMSizeOf(LLVMTypeOf(val.gen(s)));
	}
}

class AstNodeFuncCall : AstNode {
	AstNode func;
	TypeFunction funcType;
	AstNode[] args;
	bool issizeof = false;

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

		if(!funcType.instanceof!TypeFunction) {
			if(AstNodeIden _sizeof = func.instanceof!AstNodeIden) {
				if(_sizeof.name == "sizeof") {
					issizeof = true;
					return;
				}
			}
			s.ctx.addError("Cannot call a non-function value!", this.where);
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

		if(this.args.length != this.funcType.args.length) {
			s.ctx.addError("Invalid call to function, "
				~ to!string(this.funcType.args.length)
				~ " arguments are required, but got "
				~ to!string(this.args.length), this.where);
			return;
		}

		for(int i = 0; i < this.args.length; ++i) {
			this.args[i].analyze(s, this.funcType.args[i]);
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

		if(!issizeof) {LLVMValueRef* llvm_args = cast(LLVMValueRef*)malloc(LLVMValueRef.sizeof * args.length);
		for(int i = 0; i < args.length; i++) {
			auto genv = args[i].gen(s);
			llvm_args[i] = genv;
		}

		if(ctx.gfuncs.getFType(n.name).ret.toString() == "void") {
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
		);}
		if(AstNodeIden id = args[0].instanceof!AstNodeIden) {
			switch(id.name) {
				case "bool":
					return LLVMSizeOf(LLVMInt1Type());
				case "char":
					return LLVMSizeOf(LLVMInt8Type());
				case "short":
					return LLVMSizeOf(LLVMInt16Type());
				case "int":
					return LLVMSizeOf(LLVMInt32Type());
				case "long":
					return LLVMSizeOf(LLVMInt64Type());
				default:
					// What it's?
					// Oof... generate?
					return LLVMSizeOf(LLVMTypeOf(id.gen(s)));
			}
		}
		assert(0);
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

	debug {
		override void debugPrint(int indent) {
			writeTabs(indent);
			writeln("Char: '", value, '\'');
		}
	}
}

