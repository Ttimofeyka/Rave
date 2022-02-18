module ast;
import std.stdio;
import std.array : join;
import std.algorithm.iteration : map;
import gen, tokens;

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
	void gen(GenerationContext ctx) {}

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

	override void gen(GenerationContext ctx) {}

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

	override void gen(GenerationContext ctx) {}

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

	override void gen(GenerationContext ctx) {}

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

	override void gen(GenerationContext ctx) {}
	
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

	override void gen(GenerationContext ctx) {}

	debug {
		override void debugPrint(int indent) {
			writeTabs(indent);
			writeln("While:");
			cond.debugPrint(indent + 1);
			writeTabs(indent);
			writeln("^Then:");
			body_.debugPrint(indent + 1);
		}
	}
}

class AstNodeAsm : AstNode {
	string value; // TODO

	override void gen(GenerationContext ctx) {}
}

class AstNodeBlock : AstNode {
	AstNode[] nodes;

	this(AstNode[] nodes) {
		this.nodes = nodes;
	}

	override void gen(GenerationContext ctx) {}

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

	override void gen(GenerationContext ctx) {}

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

	override void gen(GenerationContext ctx) {}

	debug {
		override void debugPrint(int indent) {
			writeTabs(indent);
			writeln("Return:");
			value.debugPrint(indent  + 1);
		}
	}
}

class AstNodeIden : AstNode {
	string name;

	this(string name) {
		this.name = name;
	}

	override void gen(GenerationContext ctx) {}

	debug {
		override void debugPrint(int indent) {
			writeTabs(indent);
			writeln("Identifier: ", name);
		}
	}
}

class AstNodeLabel : AstNode {
	string name;

	override void gen(GenerationContext ctx) {}

	debug {
		override void debugPrint(int indent) {
			writeTabs(indent);
			writeln("?");
		}
	}
}

class AstNodeBreak : AstNode {
	string label; /* optional */

	override void gen(GenerationContext ctx) {}

	debug {
		override void debugPrint(int indent) {
			writeTabs(indent);
			writeln("?");
		}
	}
}

class AstNodeContinue : AstNode {
	string label; /* optional */

	override void gen(GenerationContext ctx) {}

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

	override void gen(GenerationContext ctx) {}

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

	debug {
		override void debugPrint(int indent) {
			writeTabs(indent);
			writeln("Int: ", value);
		}
	}
}

// class AstNodeFloat : AstNode { float value; }
