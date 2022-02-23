module parser.ast;

import std.stdio, std.conv;
import std.array : join;
import std.algorithm.iteration : map;
import parser.gen, lexer.tokens;
import llvm;
import std.conv : to;
import core.stdc.stdlib : exit;
import parser.typesystem;
import parser.util;
import std.string;

/// We have two separate syntax trees: for types and for values.

class AtstTypeContext {
	Type[string] types;

	Type getType(string name) {
		return types[name];
	}
}

// Abstract type syntax tree node.
class AtstNode {
	Type get(AtstTypeContext ctx) { return new Type(); }

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

// For inferenceS
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

class TypecheckContext {
	// TODO? see AstNode.typechecK
}

// Abstract syntax tree Node.
class AstNode {
	LLVMValueRef gen(GenerationContext ctx) {
		LLVMValueRef a;
		return a;
	}

	// TODO: Propagate to child classes.
	TypecheckResult typecheck(TypecheckContext ctx) {
		return TypecheckResult(false);
	}

	Type getType(TypecheckContext ctx) {
		return new TypeUnknown();
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
			return s ~ name ~ ": " ~ type.toString();
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
	AstNode body_;

	this(FunctionDeclaration decl, AstNode body_) {
		this.decl = decl;
		this.body_ = body_;
	}

	override LLVMValueRef gen(GenerationContext ctx) {
		LLVMBuilderRef builder = LLVMCreateBuilder();
		LLVMTypeRef* param_types;

		AstNodeBlock f_body = cast(AstNodeBlock)body_;
		AstNodeReturn ret_ast = cast(AstNodeReturn)
			f_body.nodes[0];

		LLVMTypeRef ret_type = LLVMFunctionType(
		    ctx.getLLVMType(decl.signature.ret),
		    param_types,
		    cast(uint)decl.signature.args.length,
		    false
	    );

		LLVMValueRef func = LLVMAddFunction(
		    ctx.mod,
		    cast(const char*)(decl.name~"\0"),
		    ret_type
	    );

		LLVMBasicBlockRef entry = LLVMAppendBasicBlock(func,cast(const char*)"entry");
		LLVMPositionBuilderAtEnd(builder, entry);

	    LLVMValueRef retval = ret_ast.value.gen(ctx);
	    LLVMBuildRet(builder, retval);

		LLVMVerifyFunction(func, 0);

		return func;
	}

	debug {
		override void debugPrint(int indent) {
			writeTabs(indent);
			writeln("Function decl: ", decl.toString());
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

	this(AstNode lhs, AstNode rhs, TokType type) {
		this.lhs = lhs;
		this.rhs = rhs;
		this.type = type;
	}

	override LLVMValueRef gen(GenerationContext ctx) {
		LLVMValueRef a;
		return a;
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

	this(AstNode cond, AstNode body_, AstNode else_) {
		this.cond = cond;
		this.body_ = body_;
		this.else_ = else_;
	}

	override LLVMValueRef gen(GenerationContext ctx) {
		LLVMValueRef a;
		return a;
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

	this(AstNode cond, AstNode body_) {
		this.cond = cond;
		this.body_ = body_;
	}

	override LLVMValueRef gen(GenerationContext ctx) {
		LLVMValueRef a;
		return a;
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
	AstNode value;

	this(AstNode value) { this.value = value; }

	override LLVMValueRef gen(GenerationContext ctx) {
		LLVMValueRef a;
		return a;
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
		if(name in ctx.global_vars) {
			return ctx.global_vars[name];
		}
		assert(0);
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
	AstNode value; // can be null!

	this(VariableDeclaration decl, AstNode value) {
		this.decl = decl;
		this.value = value;
	}

	override LLVMValueRef gen(GenerationContext ctx) {
		if(decl.name in ctx.global_vars) {
			writeln("The variable "~decl.name~" has been declared several times!");
			exit(-1);
		}
		
		LLVMValueRef constval = value.gen(ctx);

		LLVMValueRef var = LLVMAddGlobal(
			ctx.mod,
			ctx.getLLVMType(decl.type),
			toStringz(decl.name)
		);

		LLVMSetInitializer(var, constval);

		ctx.global_vars[decl.name] = var;

		return var;
	}

	debug {
		override void debugPrint(int indent) {
			writeTabs(indent);
			writeln("Variable Declaration: ", decl.toString());
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
			writeln("?");
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
			writeln("?");
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
			writeln("?");
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

	override LLVMValueRef gen(GenerationContext ctx) {
		LLVMValueRef a;
		return a;
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

	this(ulong value) { this.value = value; }

	private ulong pow2(char n) {
		ulong l = 2;
		while(n--) l *= 2;
		return l;
	}

	override Type getType(TypecheckContext _ctx) {
		if(this.value > 0) {
			// might and might not work for pow2(N)/2
			if(this.value < pow2(8)/2)  return new TypeBasic(BasicType.t_char);
			if(this.value < pow2(8))    return new TypeBasic(BasicType.t_uchar);
			if(this.value < pow2(16)/2) return new TypeBasic(BasicType.t_short);
			if(this.value < pow2(16))   return new TypeBasic(BasicType.t_ushort);
			if(this.value < pow2(32)/2) return new TypeBasic(BasicType.t_int);
			if(this.value < pow2(32))   return new TypeBasic(BasicType.t_uint);
			if(this.value < pow2(64)/2) return new TypeBasic(BasicType.t_long);
			if(this.value < pow2(64))   return new TypeBasic(BasicType.t_ulong);
			assert(0);
		}
		else {
			// TODO implement something similar to above.
			return new TypeBasic(BasicType.t_long);
		}
	}

	override LLVMValueRef gen(GenerationContext ctx) {
		TypeBasic type = cast(TypeBasic)getType(null);

		switch(type.basic) {
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
	
	override Type getType(TypecheckContext _ctx) {
		return new TypeBasic(BasicType.t_float);
	}
		
	override LLVMValueRef gen(GenerationContext ctx) {
		LLVMValueRef a;
		return a;
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

	override Type getType(TypecheckContext _ctx) {
		return new TypeConst(new TypeBasic(BasicType.t_char));
	}

	override LLVMValueRef gen(GenerationContext ctx) {
		LLVMValueRef a;
		return a;
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

	override Type getType(TypecheckContext _ctx) {
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
