module parser.ast;

import std.stdio, std.conv;
import std.array : join;
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
	Type get(AtstTypeContext _ctx) { return new Type(); }

	debug {
		override string toString() const {
			return "?";
		}
	}
}

class AtstNodeVoid : AtstNode {
	override Type get(AtstTypeContext ctx) { return new TypeVoid(); }

	debug {
		override string toString() const {
			return "void";
		}
	}
}

// For inference
class AtstNodeUnknown : AtstNode {
	override Type get(AtstTypeContext ctx) { return new TypeUnknown(); }
	
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

	override Type get(AtstTypeContext ctx) { return ctx.getType(name); }

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

	override Type get(AtstTypeContext ctx) { return new TypePointer(node.get(ctx)); }

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

	override Type get(AtstTypeContext ctx) { return new TypeConst(node.get(ctx)); }

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

	override Type get(AtstTypeContext ctx) { /* TODO */ return null; }

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

	LLVMValueRef gen(GenerationContext ctx) {
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


struct FunctionDeclaration {
	FuncSignature signature;
	string name;
	string[] argNames;
	bool isStatic;
	bool isExtern;

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

class AstNodeFunction : AstNode {
	FunctionDeclaration decl;
	FunctionSignatureTypes actualDecl;
	AstNode body_;
	LLVMBuilderRef builder;

	this(FunctionDeclaration decl, AstNode body_) {
		this.decl = decl;
		this.body_ = body_;
	}

	override void analyze(AnalyzerScope s, Type) {
		auto lastCurrentFunc = s.ctx.currentFunc;
		s.ctx.currentFunc = this;
		s.returnType = this.decl.signature.ret.get(s.ctx.typeContext);

		this.body_.analyze(s, this.decl.signature.ret.get(s.ctx.typeContext));
		Type[] argTypes = array(map!((a) => a.get(s.ctx.typeContext))(this.decl.signature.args));
		actualDecl = new FunctionSignatureTypes(s.returnType, argTypes);

		s.ctx.currentFunc = lastCurrentFunc;
	}

	private LLVMTypeRef* getParamsTypes(GenerationContext ctx) {
		LLVMTypeRef* param_types = cast(LLVMTypeRef*)malloc(
			LLVMTypeRef.sizeof * decl.signature.args.length
		);
		for(int i=0; i<decl.signature.args.length; i++) {
			
			param_types[i] = ctx.getLLVMType(actualDecl.args[i]);
		}
		return param_types;
	}

	override LLVMValueRef gen(GenerationContext ctx) {
		builder = LLVMCreateBuilder();

		AstNodeBlock f_body = cast(AstNodeBlock)body_;

		LLVMTypeRef ret_type = LLVMFunctionType(
		    ctx.getLLVMType(actualDecl.ret),
		    getParamsTypes(ctx),
		    cast(uint)decl.signature.args.length,
		    false
	    );

		LLVMValueRef func = LLVMAddFunction(
		    ctx.mod,
		   toStringz(ctx.mangleQualifiedName([decl.name], true)),
		    ret_type
	    );

		ctx.gfuncs.set(func,decl.name);

		if(!decl.isExtern) {
			LLVMBasicBlockRef entry = LLVMAppendBasicBlock(func,toStringz("entry"));
			LLVMPositionBuilderAtEnd(builder, entry);

			ctx.currbuilder = builder;
			for(int i=0; i<f_body.nodes.length; i++) {
				f_body.nodes[i].gen(ctx);
			}

			if(decl.signature.ret.get(ctx.typecontext).instanceof!(TypeVoid)) {
				LLVMBuildRetVoid(ctx.currbuilder);
			}

			ctx.currbuilder = null;
			ctx.gstack.clean();
		}
		else {
			LLVMSetLinkage(func, LLVMExternalLinkage);
		}

		LLVMVerifyFunction(func, 0);
		
		return func;
	}

	debug {
		override void debugPrint(int indent) {
			writeTabs(indent);
			writeln("Function decl: ",
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
	
	this(AstNode value, string field, bool isSugar) {
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

	this(AstNode lhs, AstNode rhs, TokType type) {
		this.lhs = lhs;
		this.rhs = rhs;
		this.type = type;
	}

	override Type getType(AnalyzerScope s) {
		return valueType;
	}

	override LLVMValueRef gen(GenerationContext ctx) {
		switch(type) {
			case TokType.tok_equ:
				AstNodeIden iden = cast(AstNodeIden)lhs;
				if(ctx.gstack.isLocal(iden.name)) {
					ctx.setLocal(rhs.gen(ctx),iden.name);
					return ctx.getVarPtr(iden.name);
				}
				else {
					return LLVMBuildStore(
						ctx.currbuilder,
						rhs.gen(ctx),
						ctx.gstack[iden.name]
					);
				}
			case TokType.tok_shortplu:
			case TokType.tok_shortmin:
			case TokType.tok_shortmul:
				AstNodeIden iden = cast(AstNodeIden)lhs;
				if(ctx.gstack.isLocal(iden.name)) {
					auto tmpval = LLVMBuildAlloca(
						ctx.currbuilder,
						LLVMGetAllocatedType(ctx.gstack[iden.name]),
						toStringz(iden.name)
					);

					LLVMValueRef tmp;
					if(type == TokType.tok_shortplu) {
						tmp = LLVMBuildAdd(
							ctx.currbuilder,
							lhs.gen(ctx),
							rhs.gen(ctx),
							toStringz("tmp")
						);
					}
					else if (type == TokType.tok_shortmin) {
						tmp = LLVMBuildSub(
							ctx.currbuilder,
							lhs.gen(ctx),
							rhs.gen(ctx),
							toStringz("tmp")
						);
					}
					else if (type == TokType.tok_shortmul) {
						tmp = LLVMBuildMul(
							ctx.currbuilder,
							lhs.gen(ctx),
							rhs.gen(ctx),
							toStringz("tmp")
						);
					}
					
					LLVMBuildStore(
						ctx.currbuilder,
						tmp,
						tmpval
					);

					ctx.gstack.set(tmpval,iden.name);
					return ctx.gstack[iden.name];
				}
				else {
					LLVMValueRef tmp;
					if(type == TokType.tok_shortplu) {
						tmp = LLVMBuildAdd(
							ctx.currbuilder,
							ctx.gstack[iden.name],
							rhs.gen(ctx),
							toStringz("tmp")
						);
					}
					else if(type == TokType.tok_shortmin) {
						tmp = LLVMBuildSub(
							ctx.currbuilder,
							ctx.gstack[iden.name],
							rhs.gen(ctx),
							toStringz("tmp")
						);
					}
					else if(type == TokType.tok_shortmul) {
						/*tmp = LLVMBuildMul(
							ctx.currbuilder,
							ctx.gstack[iden.name],
							rhs.gen(ctx),
							toStringz("tmp")
						);*/
						tmp = ctx.operMul(
							ctx.gstack[iden.name],
							rhs.gen(ctx),
							false 
						);
					}

					LLVMBuildStore(
						ctx.currbuilder,
						tmp,
						ctx.gstack[iden.name]
					);
					return ctx.gstack[iden.name];
				}
				assert(0);
			case TokType.tok_plus:
			case TokType.tok_minus:
			case TokType.tok_multiply:
				auto result = LLVMBuildAlloca(
					ctx.currbuilder,
					LLVMInt32Type(),
					toStringz("result")
				);
				LLVMValueRef operation;
				if(type == TokType.tok_plus) {
					operation = LLVMBuildAdd(
						ctx.currbuilder,
						lhs.gen(ctx),
						rhs.gen(ctx),
						toStringz("operation")
					);
				}
				else if(type == TokType.tok_minus) {
					operation = LLVMBuildSub(
						ctx.currbuilder,
						lhs.gen(ctx),
						rhs.gen(ctx),
						toStringz("operation")
					);
				}
				else if(type == TokType.tok_multiply) {
					operation = LLVMBuildMul(
						ctx.currbuilder,
						lhs.gen(ctx),
						rhs.gen(ctx),
						toStringz("operation")
					);
				}
				LLVMBuildStore(
					ctx.currbuilder,
					operation,
					result
				);
				auto loaded_result = LLVMBuildLoad(
					ctx.currbuilder,
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
						lhs.gen(ctx),
						rhs.gen(ctx),
						toStringz("bit_ls")
					);
				}
				else if(type == TokType.tok_bit_rs)  {
					bit_result = LLVMBuildAShr(
						ctx.currbuilder,
						lhs.gen(ctx),
						rhs.gen(ctx),
						toStringz("bit_rs")
					);
				}
				else if(type == TokType.tok_bit_and) {
					bit_result = LLVMBuildAnd(
						ctx.currbuilder,
						lhs.gen(ctx),
						rhs.gen(ctx),
						toStringz("bit_and")
					);
				}
				else if(type == TokType.tok_bit_or) {
					bit_result = LLVMBuildOr(
						ctx.currbuilder,
						lhs.gen(ctx),
						rhs.gen(ctx),
						toStringz("bit_or")
					);
				}
				else if(type == TokType.tok_bit_xor) {
					bit_result = LLVMBuildXor(
						ctx.currbuilder,
						lhs.gen(ctx),
						rhs.gen(ctx),
						toStringz("bit_xor")
					);
				}
				return bit_result;
			case TokType.tok_equal:
			case TokType.tok_nequal:
				LLVMTypeRef equal_type;
				LLVMValueRef lhsgen;
				if(lhs.instanceof!(AstNodeIden)) {
					lhsgen = lhs.gen(ctx);
					AstNodeIden iden = cast(AstNodeIden)lhs;
					if(ctx.gstack.isGlobal(iden.name)) {
						equal_type = getVarType(ctx,iden.name);
					}
					else equal_type = getAType(lhsgen);
				}
				else if(lhs.instanceof!(AstNodeInt)) {
					lhsgen = lhs.gen(ctx);
					AstNodeInt nint = cast(AstNodeInt)lhs;
					equal_type = ctx.getLLVMType(nint.valueType);
				}
				else {
					lhsgen = lhs.gen(ctx);
					equal_type = getAType(lhsgen);
				}
				
				//if(ctx.isIntType(equal_type)) {
					if(type == TokType.tok_equal) return LLVMBuildICmp(
						ctx.currbuilder,
						LLVMIntEQ,
						lhsgen,
						rhs.gen(ctx),
						toStringz("icmp_eq")
					);
					return LLVMBuildICmp(
						ctx.currbuilder,
						LLVMIntNE,
						lhsgen,
						rhs.gen(ctx),
						toStringz("icmp_ne")
					);
				//}
			default:
				break;
		}
		assert(0);
	}

	private void checkTypes(AnalyzerScope s, Type neededType) {

		if(!neededType.instanceof!TypeUnknown) {
			if(neededType.assignable(lhs.getType(s))
			&& neededType.assignable(rhs.getType(s))) {
				this.valueType = neededType;
			}
			else {
				if(rhs.instanceof!AstNodeFuncCall) {
					this.valueType = neededType;
				}
				else {
					s.ctx.addError("Bad binary operator types: expected '"
						~ neededType.toString() ~ "' but got '"
						~ lhs.getType(s).toString() ~ "' and '"
						~ rhs.getType(s).toString() ~ "'.");
				}
			}
		}
		else { // neededType --> unknown.
			// TODO: Find best suitable type!
			this.valueType = lhs.getType(s);
		}
	}

	override void analyze(AnalyzerScope s, Type neededType) {
		lhs.analyze(s, neededType);
		rhs.analyze(s, neededType);
		
		if(neededType.instanceof!TypeUnknown)
			neededType = lhs.getType(s); // TODO: Find best suitable type!
		// if(neededType.instanceof!TypeUnknown)
		// 	neededType = rhs.getType(s); // TODO: Find best suitable type!
			
		if(_isArithmetic()) {
			if(!instanceof!TypeBasic(neededType)) {
				s.ctx.addError("Non-basic type " ~ neededType.toString() ~
					" was required for an arithmetic binary operation by the outer scope.");
			}
		}
		checkTypes(s, neededType);
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

	this(AstNode node, TokType type) {
		this.node = node;
		this.type = type;
	}

	override LLVMValueRef gen(GenerationContext ctx) {
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
	}

	override LLVMValueRef gen(GenerationContext ctx) {
		LLVMBasicBlockRef thenbb;
		LLVMBasicBlockRef elsebb;
		LLVMBasicBlockRef endbb;

		thenbb = LLVMAppendBasicBlock(ctx.gfuncs[parent.decl.name],toStringz("then"));
		elsebb = LLVMAppendBasicBlock(ctx.gfuncs[parent.decl.name],toStringz("else"));
		endbb = LLVMAppendBasicBlock(ctx.gfuncs[parent.decl.name],toStringz("end"));

		LLVMBuildCondBr(
			ctx.currbuilder,
			cond.gen(ctx),
			thenbb,
			elsebb
		);
		
		LLVMPositionBuilderAtEnd(ctx.currbuilder,thenbb);

		if(body_ is null) retNull(ctx, LLVMInt32Type());
		else {
			if(body_.instanceof!(AstNodeBlock)) {
				AstNodeBlock b = cast(AstNodeBlock)body_;
				for(int i=0; i<b.nodes.length; i++) {
					b.nodes[i].gen(ctx);
				}
			}
			else body_.gen(ctx);
		}

		LLVMBuildBr(ctx.currbuilder,endbb);

		LLVMPositionBuilderAtEnd(ctx.currbuilder,elsebb);
		LLVMBuildBr(ctx.currbuilder,endbb);

		LLVMPositionBuilderAtEnd(ctx.currbuilder,endbb);

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

	override LLVMValueRef gen(GenerationContext ctx) {
		LLVMBasicBlockRef _while;
		LLVMBasicBlockRef _after;

		_while = LLVMAppendBasicBlock(ctx.gfuncs[parent.decl.name],toStringz("_while"));
		_after = LLVMAppendBasicBlock(ctx.gfuncs[parent.decl.name],toStringz("_after"));

		LLVMValueRef cond_as_cmp = cond.gen(ctx);
		LLVMBuildCondBr(ctx.currbuilder,cond_as_cmp,_while,_after);
		
		LLVMPositionBuilderAtEnd(ctx.currbuilder,_while);

		if(body_.instanceof!(AstNodeBlock)) {
			AstNodeBlock block = cast(AstNodeBlock)body_;
			for(int i=0; i<block.nodes.length; i++) {
				block.nodes[i].gen(ctx);
			}
		}
		else body_.gen(ctx);

		LLVMBuildCondBr(ctx.currbuilder,cond.gen(ctx),_while,_after);

		LLVMPositionBuilderAtEnd(ctx.currbuilder,_after);

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

	override LLVMValueRef gen(GenerationContext ctx) {
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
		auto newScope = new AnalyzerScope(s.ctx, s);
		newScope.returnType = s.returnType;

		foreach(node; this.nodes) {
			node.analyze(newScope, neededType);
			if(newScope.hadReturn && !neededType.instanceof!TypeUnknown) {
				if(!neededType.assignable(newScope.returnType)) {
					if(newScope.returnType.toString() != "?unknown") {
						s.ctx.addError("Wrong return type: expected '" ~ neededType.toString()
						~ "', got: '" ~ newScope.returnType.toString() ~ '\'');
					}
				}
				break;
			}
		}
		s.returnType = newScope.returnType;
	}

	override LLVMValueRef gen(GenerationContext ctx) {
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

	this(AstNode value, AstNodeFunction parent) { 
		this.value = value;
		this.parent = parent;
	}

	override void analyze(AnalyzerScope s, Type neededType) {
		this.parent = s.ctx.currentFunc;
		this.value.analyze(s, neededType);
		s.hadReturn = true;
		s.returnType = this.value.getType(s);
	}

	override LLVMValueRef gen(GenerationContext ctx) {
		LLVMValueRef retval = value.gen(ctx);
	    LLVMBuildRet(ctx.currbuilder, retval);
		return null;
	}

	debug {
		override void debugPrint(int indent) {
			writeTabs(indent);
			writeln("Return:");
			value.debugPrint(indent  + 1);
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

	override LLVMValueRef gen(GenerationContext ctx) {
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

	this(string name) {
		this.name = name;
	}

	override LLVMValueRef gen(GenerationContext ctx) {
		// _EPLf4exit, not exit! ctx.mangleQualifiedName([name], true) -> string
		// TODO: Check if the global variable is referring to a function
		if(ctx.gstack.setVar) {
			ctx.gstack.setVar = false;
			return ctx.gstack[name];
		}
		else {
			if(ctx.gstack.isGlobal(name)) {
				// Global var
				if(ctx.currbuilder != null) {
					return ctx.getValueByPtr(name);
				}
				else return ctx.gstack[name];
			}
			else {
				// Local var
				return ctx.getValueByPtr(name);
			}
		}
		assert(0);
	}

	override Type getType(AnalyzerScope s) {
		auto t = s.get(name);
		if(t is null) {
			s.ctx.addError("No such variable or function binding: '" ~ name ~ "'");
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
					~ v.toString() ~ ", but " ~ neededType.toString() ~ " was expected.");
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
		actualType = this.decl.type.get(s.ctx.typeContext);
		if(this.value !is null) {
			this.value.analyze(s, actualType);
			if(instanceof!TypeUnknown(actualType))
				actualType = this.value.getType(s);
		}
		s.vars[decl.name] = new VariableSignatureTypes(actualType);
	}

	override LLVMValueRef gen(GenerationContext ctx) {
		if(ctx.gstack.isVariable(decl.name)) {
			writeln("The variable " ~ decl.name ~ " has been declared multiple times!");
			exit(-1);
		}

		LLVMValueRef constval = value.gen(ctx);

		if(ctx.currbuilder == null) {
			// Global var
			return ctx.createGlobal(
				ctx.getLLVMType(decl.type.get(ctx.typecontext)),
				constval,
				decl.name
			);
		}
		else {
			// Local var
			return ctx.createLocal(
				ctx.getLLVMType(decl.type.get(ctx.typecontext)),
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

	override LLVMValueRef gen(GenerationContext ctx) {
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

	override LLVMValueRef gen(GenerationContext ctx) {
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

	override LLVMValueRef gen(GenerationContext ctx) {
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

	override LLVMValueRef gen(GenerationContext ctx) {
		// _EPLf4exit, not exit! ctx.mangleQualifiedName([name], true) -> string
		AstNodeIden n = cast(AstNodeIden)func;
		return LLVMBuildCall(
			ctx.currbuilder,
			ctx.gfuncs[n.name],
			null,
			0,
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

	this(ulong value) { 
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
				s.ctx.addError("Expected " ~ neededType.toString() ~ ", got an integer");
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

	override LLVMValueRef gen(GenerationContext ctx) {
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

	this(float value) { this.value = value; }
	
	override Type getType(AnalyzerScope s) {
		return new TypeBasic(BasicType.t_float);
	}

	override void analyze(AnalyzerScope s, Type neededType) {}
	
	override LLVMValueRef gen(GenerationContext ctx) {
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

	this(string value) { this.value = value; }

	override Type getType(AnalyzerScope s) {
		return new TypePointer(new TypeBasic(BasicType.t_char));
	}

	override void analyze(AnalyzerScope s, Type neededType) {}

	override LLVMValueRef gen(GenerationContext ctx) {
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

	this(char value) { this.value = value; }

	override Type getType(AnalyzerScope s) {
		return new TypeBasic(BasicType.t_char);
	}

	override LLVMValueRef gen(GenerationContext ctx) {
		return LLVMConstInt(LLVMInt8Type(),value,false);
	}

	debug {
		override void debugPrint(int indent) {
			writeTabs(indent);
			writeln("Char: '", value, '\'');
		}
	}
}

