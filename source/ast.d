module ast;
import std.stdio, std.conv;
import std.array : join;
import std.algorithm.iteration : map;
import gen, tokens;
import llvm;
import std.conv : to;
import core.stdc.stdlib : exit;
import core.stdc.stdlib : malloc;

/// We have two separate syntax trees: for types and for values.

// Abstract type syntax tree node.
class AtstNode {
	debug {
		override string toString() const {
			return "?";
		}
	}
}

class AtstNodeVoid : AtstNode {
	debug {
		override string toString() const {
			return "void";
		}
	}
}

class AtstNodeUnknown : AtstNode {
	// For inference
	
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

	debug {
		override string toString() const {
			return node.toString() ~ "*";
		}
	}
}

class AtstNodeArray : AtstNode {
	AtstNode node;
	uint count;

	this(AtstNode node, uint count) {
		this.node = node;
		this.count = count;
	}

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

// Abstract syntax tree Node.
class AstNode {
	LLVMValueRef gen(GenerationContext ctx) {
		LLVMValueRef a;
		return a;
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

	debug {
		string toString() const {
			return signature.toString() ~ " [" ~ name ~ "(" ~ join(argNames, ", ") ~ ")]";
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
}

class TypeDeclaration {
	string name;
}

class TypeDeclarationStruct : TypeDeclaration {
	VariableDeclaration[] decls;
}

class AstNodeFunction : AstNode {
	FunctionDeclaration decl;
	AstNode body_;

	this(FunctionDeclaration decl, AstNode body_) {
		this.decl = decl;
		this.body_ = body_;
	}

	override LLVMValueRef gen(GenerationContext ctx) {
		LLVMValueRef a;
		return a;
	}

	debug {
		override void debugPrint(int indent) {
			writeTabs(indent);
			writeln("Function decl: ", decl.toString());
			body_.debugPrint(indent + 1);
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

class AstNodeBinary : AstNode {
	AstNode lhs, rhs;
	TokType type;

	this(AstNode lhs, AstNode rhs, TokType type) {
		this.lhs = lhs;
		this.rhs = rhs;
		this.type = type;
	}

	override LLVMValueRef gen(GenerationContext ctx) {
		LLVMValueRef LHS = lhs.gen(ctx);
		LLVMValueRef RHS = rhs.gen(ctx);

		switch(type) {
			case TokType.tok_plus:
				return LLVMBuildFAdd(ctx.builder,LHS,RHS,cast(const char*)"addtmp");
			case TokType.tok_minus:
				return LLVMBuildFSub(ctx.builder,LHS,RHS,cast(const char*)"subtmp");
			case TokType.tok_multiply:
				return LLVMBuildFMul(ctx.builder,LHS,RHS,cast(const char*)"multmp");
			case TokType.tok_divide:
				return LLVMBuildFDiv(ctx.builder,LHS,RHS,cast(const char*)"divtmp");
			default:
				writeln("\033[0;31mError: Undefined operator "~to!string(type)~"!\033[0;0m");
				return LHS;
		}
	}

	debug {
		override void debugPrint(int indent) {
			writeTabs(indent);
			writeln("Binary ", tokTypeToStr(type));
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
			writeln("Unary ", tokTypeToStr(type));
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

class AstNodeExtern : AstNode {
	FunctionDeclaration decl;
	// TODO: Maybe something else? Like calling convention and etc.

	this(FunctionDeclaration decl) {
		this.decl = decl;
	}

	override LLVMValueRef gen(GenerationContext ctx) {
		LLVMValueRef a;
		return a;
	}

	debug {
		override void debugPrint(int indent) {
			writeTabs(indent);
			writeln("Extern: ", decl.toString());
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
		if(name in ctx.values) {
			return ctx.values[name];
		}
		else {
			writeln("\033[0;31mError: Undefined variable "~name~"!\033[0;0m");
			exit(-1);
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
	string name;
	AtstNode type;
	AstNode value; // can be null!

	this(string name, AtstNode type, AstNode value) {
		this.name = name;
		this.type = type;
		this.value = value;
	}

	override LLVMValueRef gen(GenerationContext ctx) {
		LLVMValueRef a;
		return a;
	}

	debug {
		override void debugPrint(int indent) {
			writeTabs(indent);
			writeln("Variable Declaration: ", name, ": ", type.toString());
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
		AstNodeIden mn = cast(AstNodeIden)func;
		string n = mn.name;
		LLVMValueRef func = LLVMGetNamedFunction(
			ctx.main_module,
			cast(const char*)n
		);
		if(LLVMCountParams(func) != cast(uint)args.length) {
			writeln("\033[0;31mError: Too little or too more args!\033[0;0m");
		}

		LLVMValueRef* argss = cast(LLVMValueRef*)malloc(LLVMValueRef.sizeof*args.length);
		for(int i=0; i<args.length; i++) {
			argss[i] = args[i].gen(ctx);
		}
		
		return LLVMBuildCall(
			ctx.builder,func,argss,cast(uint)args.length,cast(const char*)"calltmp"
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
	uint value;

	this(uint value) { this.value = value; }

	override LLVMValueRef gen(GenerationContext ctx) {
		return LLVMConstInt(LLVMInt32Type(),cast(ulong)value,false);
	}

	debug {
		override void debugPrint(int indent) {
			writeTabs(indent);
			writeln("Int: ", value);
		}
	}
}

// class AstNodeFloat : AstNode { float value; }
