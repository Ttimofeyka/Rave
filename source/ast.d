module ast;
import std.stdio;
import std.array : join;
import std.algorithm.iteration : map;

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

	debug {
		override void debugPrint(int indent) {
			writeTabs(indent);
			writeln("Function decl: ", decl.toString());
			body_.debugPrint(indent + 1);
		}
	}
}

enum BinaryOpType {
	binary_op_add, // addition
	binary_op_sub, // substraction
	binary_op_mul, // multiplication
	binary_op_div, // division
	binary_op_grt, // greater than
	binary_op_lst, // less than
	binary_op_geq, // greater than or equal
	binary_op_leq, // less than or equal
	binary_op_eql, // equal to
	binary_op_neq, // not equal to
}

class AstNodeBinary : AstNode {
	AstNode lhs, rhs;
	BinaryOpType type;
}

class AstNodeIf : AstNode {
	AstNode cond;
	AstNode body_;
	AstNode else_;
}

class AstNodeWhile : AstNode {
	AstNode cond;
	AstNode body_;
}

class AstNodeAsm : AstNode {
	string value; // TODO
}

class AstNodeBlock : AstNode {
	AstNode[] nodes;

	this(AstNode[] nodes) {
		this.nodes = nodes;
	}
}

class AstNodeExtern : AstNode {
	FunctionDeclaration decl;
	// TODO: Maybe something else? Like calling convention and etc.

	this(FunctionDeclaration decl) {
		this.decl = decl;
	}
}

class AstNodeIden : AstNode { string name; }
class AstNodeLabel : AstNode { string name; }
class AstNodeBreak : AstNode { string label; /* optional */ }
class AstNodeContinue : AstNode { string label; /* optional */ }
class AstNodeFuncCall : AstNode { AstNode func; AstNode[] args; }
class AstNodeInt : AstNode { uint value; }
class AstNodeFloat : AstNode { float value; }
