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
import core.stdc.stdlib : malloc;
import parser.generator.llvm_abs;

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
			writeln("No such type: ", name);
			return new TypeUnknown();
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

struct FunctionDeclaration {
	FuncSignature signature;
	string name;
	string[] argNames;
	bool isStatic;
	bool isExtern;
	DeclMod[] decl_mods;

	debug {
		string toString() const {
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

struct VariableDeclaration {
	AtstNode type;
	string name;
	bool isStatic;
	bool isExtern;
	DeclMod[] decl_mods;

	debug {
		string toString() const {
			string s = "";
			if(isStatic) s ~= "static ";
			if(isExtern) s ~= "extern ";
			return s ~ name ~ ": ";
		}
	}
}

class TypeDeclaration {
	string name;
}

struct EnumEntry {
	string name;
	ulong value;
}

class TypeDeclarationEnum : TypeDeclaration {
	EnumEntry[] entries;

	override string toString() const {
		string s = "enum " ~ name ~ " {";
		if(entries.length > 0) {
			s ~= '\n';
		}
		foreach(entry; entries) {
			s ~= "    " ~ entry.name ~ " = " ~ to!string(entry.value) ~ "\n";
		}
		return s ~ "}";
	}
}

class TypeDeclarationStruct : TypeDeclaration {
	VariableDeclaration[] fieldDecls;
	FunctionDeclaration[] methodDecls;

	AstNode[string] fieldValues;
	AstNode[string] methodValues;

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
}

struct ReturnStmt {
	AstNodeReturn node;
	LLVMValueRef value;
	AnalyzerScope scope_;
}

class AstNodeFunction : AstNode {
	FunctionDeclaration decl;
	FunctionSignatureTypes actualDecl;
	AstNode body_;
	
	LLVMBuilderRef builder;
	ReturnStmt[] returns;

	this(SourceLocation loc, FunctionDeclaration decl, AstNode body_) {
		this.where = loc;
		this.decl = decl;
		this.body_ = body_;
	}

	override void analyze(AnalyzerScope s, Type) {

		auto retType = this.decl.signature.ret.get(s);
		Type[] argTypes = array(map!((a) => a.get(s))(this.decl.signature.args));

		if(this.body_ is null) {
			if(retType.instanceof!TypeUnknown) {
				s.ctx.addError("Cannot infer return type of a function with no body.",
					this.where);
				return;
			}

			if(this.decl.isExtern) {
				actualDecl = new FunctionSignatureTypes(retType, argTypes);
				return;
			}
			else {
				s.ctx.addError("Forward declarations are not yet implemented.",
					this.where);
				// Forward declaration... TODO
				return;
			}
		}

		auto ss = new AnalyzerScope(s.ctx, s.genctx, s);
		ss.ctx.currentFunc = this;

		for(int i = 0; i < this.decl.argNames.length; ++i) {
			ss.vars[this.decl.argNames[i]] = new VariableSignatureTypes(argTypes[i]);
		}

		ss.neededReturnType = retType;
		this.body_.analyze(ss, new TypeUnknown());

		if(this.returns.length == 0 && retType.instanceof!TypeUnknown)
			retType = new TypeVoid();

		foreach(ret; this.returns) {
			Type type = new TypeVoid();
			if(ret.node.value !is null)
				type = ret.node.value.getType(ret.scope_);
			
			if(retType.instanceof!TypeUnknown)
				retType = type;
			else if(!retType.assignable(type))
			{
				s.ctx.addError("First mismatching return is of type '" ~ type.toString()
					~ "', while all previous returns are of type '" ~ retType.toString() ~ "'",
					ret.node.where);
				s.ctx.addError("Return types don't match in function '" ~ decl.name ~ "'.", this.where);
				break;
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
		actualDecl = new FunctionSignatureTypes(retType, argTypes);
	}

	private LLVMTypeRef* getParamsTypes(GenerationContext ctx) {
		LLVMTypeRef* param_types = cast(LLVMTypeRef*)malloc(
			LLVMTypeRef.sizeof * decl.signature.args.length
		);
		for(int i=0; i<decl.signature.args.length; i++) {
			
			param_types[i] = ctx.getLLVMType(actualDecl.args[i]);
			ctx.gargs.set(i,param_types[i],decl.argNames[i]);
		}
		return param_types;
	}

	override LLVMValueRef gen(AnalyzerScope s) {
		auto ctx = s.genctx;
		builder = LLVMCreateBuilder();

		AstNodeBlock f_body = cast(AstNodeBlock)body_;

		LLVMTypeRef ret_type = LLVMFunctionType(
		    ctx.getLLVMType(actualDecl.ret),
		    getParamsTypes(ctx),
		    cast(uint)decl.signature.args.length,
		    false
	    );

		bool should_mangle = true;
		foreach(mod; decl.decl_mods) {
			if(mod.name == "C") should_mangle = false;
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
			toStringz(ctx.mangleQualifiedName([decl.name], true)),
			ret_type
		);

		ctx.gfuncs.set(func,decl.name);
		ctx.currfunc = func;

		if(!decl.isExtern) {
			LLVMBasicBlockRef entry = LLVMAppendBasicBlock(func,toStringz("entry"));
			LLVMPositionBuilderAtEnd(builder, entry);

			ctx.currbuilder = builder;
			for(int i=0; i<f_body.nodes.length; i++) {
				f_body.nodes[i].gen(s);
			}

			if(decl.signature.ret.get(s).instanceof!(TypeVoid)) {
				LLVMBuildRetVoid(ctx.currbuilder);
			}

			ctx.currbuilder = null;
			ctx.gstack.clean();
			ctx.gargs.clean();
		}
		else {
			LLVMSetLinkage(func, LLVMExternalLinkage);
		}

		ctx.currfunc = null;

		LLVMVerifyFunction(func, 0);
		
		return func;
	}

	debug {
		override void debugPrint(int indent) {
			writeTabs(indent);
			writeln("Function Declaration: ",
				actualDecl is null ? decl.toString() : actualDecl.toString(decl.name, decl.argNames)
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
	bool isSugar; // true if -> false if .
	
	this(SourceLocation where, AstNode value, string field, bool isSugar) {
		this.where = where;
		this.value = value;
		this.field = field;
		this.isSugar = isSugar;
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
		auto ctx = s.genctx;
		switch(type) {
			case TokType.tok_equ:
				AstNodeIden iden = cast(AstNodeIden)lhs;
				if(ctx.gstack.isLocal(iden.name)) {
					ctx.setLocal(rhs.gen(s),iden.name);
					return ctx.getVarPtr(iden.name);
				}
				else {
					return LLVMBuildStore(
						ctx.currbuilder,
						rhs.gen(s),
						ctx.gstack[iden.name]
					);
				}
			case TokType.tok_shortplu:
			case TokType.tok_shortmin:
			case TokType.tok_shortmul:
				AstNodeIden iden = cast(AstNodeIden)lhs;
				if(s.genctx.gstack.isLocal(iden.name)) {
					auto tmpval = LLVMBuildAlloca(
						s.genctx.currbuilder,
						LLVMGetAllocatedType(s.genctx.gstack[iden.name]),
						toStringz(iden.name)
					);

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
						tmpval
					);

					s.genctx.gstack.set(tmpval,iden.name);
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
				LLVMTypeRef equal_type;
				LLVMValueRef lhsgen;
				if(lhs.instanceof!(AstNodeIden)) {
					lhsgen = lhs.gen(s);
					AstNodeIden iden = cast(AstNodeIden)lhs;
					if(ctx.gstack.isGlobal(iden.name)) {
						equal_type = getVarType(ctx, iden.name);
					}
					else equal_type = getAType(lhsgen);
				}
				else if(lhs.instanceof!(AstNodeInt)) {
					lhsgen = lhs.gen(s);
					AstNodeInt nint = cast(AstNodeInt)lhs;
					equal_type = s.genctx.getLLVMType(nint.valueType);
				}
				else {
					lhsgen = lhs.gen(s);
					equal_type = getAType(lhsgen);
				}
				
				//if(ctx.isIntType(equal_type)) {
					if(type == TokType.tok_equal) return LLVMBuildICmp(
						s.genctx.currbuilder,
						LLVMIntEQ,
						lhsgen,
						rhs.gen(s),
						toStringz("icmp_eq")
					);
					return LLVMBuildICmp(
						s.genctx.currbuilder,
						LLVMIntNE,
						lhsgen,
						rhs.gen(s),
						toStringz("icmp_ne")
					);
				//}
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
				s.ctx.addError("Bad binary operator types: expected '"
					~ neededType.toString() ~ "' but got '"
					~ lhs.getType(s).toString() ~ "' and '"
					~ rhs.getType(s).toString() ~ "'.", where);

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

	override LLVMValueRef gen(AnalyzerScope s) {
		auto ctx = s.genctx;
		LLVMValueRef a;
		return a;
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
		auto boolType = new TypeBasic(BasicType.t_bool);
		this.cond.analyze(s, boolType);

		this.body_.analyze(s, new TypeUnknown());

		if(this.else_ !is null) {
			auto prevHadReturn = s.hadReturn;
			this.else_.analyze(s, new TypeUnknown());
			s.hadReturn = prevHadReturn && s.hadReturn;
		}
		else s.hadReturn = false;
	}

	override LLVMValueRef gen(AnalyzerScope s) {
		auto ctx = s.genctx;
		LLVMBasicBlockRef thenbb;
		LLVMBasicBlockRef elsebb;
		LLVMBasicBlockRef endbb;

		thenbb = LLVMAppendBasicBlock(ctx.gfuncs[parent.decl.name], toStringz("then"));
		elsebb = LLVMAppendBasicBlock(ctx.gfuncs[parent.decl.name], toStringz("else"));
		endbb = LLVMAppendBasicBlock(ctx.gfuncs[parent.decl.name], toStringz("end"));

		LLVMBuildCondBr(
			ctx.currbuilder,
			cond.gen(s),
			thenbb,
			elsebb
		);
		
		LLVMPositionBuilderAtEnd(s.genctx.currbuilder,thenbb);

		if(body_ is null) retNull(s.genctx, LLVMInt32Type());
		else {
			if(body_.instanceof!(AstNodeBlock)) {
				AstNodeBlock b = cast(AstNodeBlock)body_;
				for(int i = 0; i < b.nodes.length; i++) {
					b.nodes[i].gen(s);
				}
			}
			else body_.gen(s);
		}

		LLVMBuildBr(s.genctx.currbuilder,endbb);

		LLVMPositionBuilderAtEnd(s.genctx.currbuilder,elsebb);
		LLVMBuildBr(s.genctx.currbuilder,endbb);

		LLVMPositionBuilderAtEnd(s.genctx.currbuilder,endbb);

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
	}

	override LLVMValueRef gen(AnalyzerScope s) {
		auto ctx = s.genctx;
		LLVMBasicBlockRef _while;
		LLVMBasicBlockRef _after;

		_while = LLVMAppendBasicBlock(s.genctx.gfuncs[parent.decl.name],toStringz("_while"));
		_after = LLVMAppendBasicBlock(s.genctx.gfuncs[parent.decl.name],toStringz("_after"));

		LLVMValueRef cond_as_cmp = cond.gen(s);
		LLVMBuildCondBr(ctx.currbuilder,cond_as_cmp,_while,_after);
		
		LLVMPositionBuilderAtEnd(s.genctx.currbuilder,_while);

		if(body_.instanceof!(AstNodeBlock)) {
			AstNodeBlock block = cast(AstNodeBlock)body_;
			for(int i=0; i<block.nodes.length; i++) {
				block.nodes[i].gen(s);
			}
		}
		else body_.gen(s);

		LLVMBuildCondBr(ctx.currbuilder,cond.gen(s), _while, _after);

		LLVMPositionBuilderAtEnd(ctx.currbuilder, _after);

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
		auto ctx = s.genctx;
		LLVMValueRef a;
		return a;
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
			s.ctx.addError("Wrong return type: expected '" ~ s.neededReturnType.toString()
				~ "', got: '" ~ t.toString() ~ '\'', this.where);
			s.hadReturn = true;
		}
		else {
			s.hadReturn = true;
			s.returnType = t;
		}

		parent.returns ~= ReturnStmt(this, null, s);
	}

	override LLVMValueRef gen(AnalyzerScope s) {
		auto ctx = s.genctx;
		if(value !is null) {
			LLVMValueRef retval = value.gen(s);
			LLVMBuildRet(ctx.currbuilder, retval);
		}
		return null;
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

	this(AstNode base, AstNode index) {
		this.base = base;
		this.index = index;
	}

	override LLVMValueRef gen(AnalyzerScope s) {
		auto ctx = s.genctx;
		LLVMValueRef a;
		return a;
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

	this(SourceLocation where, string name) {
		this.where = where;
		this.name = name;
	}

	override LLVMValueRef gen(AnalyzerScope s) {
		auto ctx = s.genctx;
		// _EPLf4exit, not exit! ctx.mangleQualifiedName([name], true) -> string
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
		auto t = s.get(name);
		if(t is null) {
			s.ctx.addError("No such variable or function binding: '" ~ name ~ "'", this.where);
			return new TypeBasic(BasicType.t_int);
		}

		if(auto v = t.instanceof!VariableSignatureTypes)
			return v.type;
		
		if(auto v = t.instanceof!FunctionSignatureTypes)
			return v.ret;

		assert(0);
	}

	override void analyze(AnalyzerScope s, Type neededType) {
		auto t = s.get(name);

		if(auto v = t.instanceof!VariableSignatureTypes) {
			if(!neededType.assignable(v.type) && !neededType.instanceof!TypeUnknown) {
				s.ctx.addError("Type mismatch: variable '" ~ name ~ "' is of type "
					~ v.type.toString() ~ ", but " ~ neededType.toString() ~ " was expected.",
					this.where);
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
		s.vars[decl.name] = new VariableSignatureTypes(actualType);
	}

	override LLVMValueRef gen(AnalyzerScope s) {
		auto ctx = s.genctx;
		if(ctx.gstack.isVariable(decl.name)) {
			writeln("The variable " ~ decl.name ~ " has been declared multiple times!");
			exit(-1);
		}

		LLVMValueRef constval = value.gen(s);

		if(s.genctx.currbuilder == null) {
			// Global var
			return ctx.createGlobal(
				ctx.getLLVMType(decl.type.get(s)),
				constval,
				decl.name
			);
		}
		else {
			// Local var
			return ctx.createLocal(
				ctx.getLLVMType(decl.type.get(s)),
				constval,
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
		LLVMValueRef a;
		return a;
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
		LLVMValueRef a;
		return a;
	}

	debug {
		override void debugPrint(int indent) {
			writeTabs(indent);
			writeln("Continue");
		}
	}
}

class AstNodeFuncCall : AstNode {
	AstNode func;
	AstNode[] args;

	this(AstNode func, AstNode[] args) {
		this.func = func;
		this.args = args;
	}

	override void analyze(AnalyzerScope s, Type neededType) {
		
	}

	override LLVMValueRef gen(AnalyzerScope s) {
		auto ctx = s.genctx;
		// _EPLf4exit, not exit! ctx.mangleQualifiedName([name], true) -> string
		AstNodeIden n = cast(AstNodeIden)func;

		LLVMValueRef* llvm_args = cast(LLVMValueRef*)malloc(LLVMValueRef.sizeof*args.length);
		for(int i=0; i<args.length; i++) {
			llvm_args[i] = args[i].gen(s);
		}

		return LLVMBuildCall(
			ctx.currbuilder,
			ctx.gfuncs[n.name],
			llvm_args,
			cast(uint)args.length,
			toStringz("call")
		);
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
				s.ctx.addError("Expected " ~ neededType.toString() ~ ", got an integer", this.where);
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
		auto ctx = s.genctx;
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
		return LLVMConstString(
			cast(const char*)toStringz(value[0..$-1]),
			cast(uint)value.length-1,
			0
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

