module ast;
import std.stdio, std.conv;
import std.array : join;
import std.algorithm.iteration : map;
import gen, tokens;
import llvm;
import std.conv : to;
import core.stdc.stdlib : exit;
import core.stdc.stdlib : malloc;
import typesystem;

/// We have two separate syntax trees: for types and for values.

class AtstTypeContext {
	Type[string] types;

	Type getType(string name) {
		// TODO
		return null;
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

class AtstNodeArray : AtstNode {
	AtstNode node;
	uint count;

	this(AtstNode node, uint count) {
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
	VariableDeclaration[] fieldDecls;
	FunctionDeclaration[] methodDecls;

	AstNode[string] fieldValues;
	AstNode[string] methodValues;

	string name;

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
			s ~= "    " ~ method.name ~ "(";

			if(method.signature.args.length > 0) {
				s ~= method.argNames[0] ~ ": " ~ method.signature.args[0].toString();
				for(int i = 1; i < method.signature.args.length; ++i) {
					s ~= ", " ~ method.argNames[i] ~ ": " ~ method.signature.args[i].toString();
				}
			}
			s ~= "): " ~ method.signature.ret.toString();
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
		LLVMTypeRef *params = 
			cast(LLVMTypeRef*)malloc(LLVMTypeRef.sizeof*decl.argNames.length);
		for(int i=0; i<decl.argNames.length; i++) {
            params[i] = LLVMInt16Type();
        }
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
		AstNodeIden mn = cast(AstNodeIden)func; // FIXME
		string n = mn.name;
		LLVMValueRef func = LLVMGetNamedFunction(
			ctx.main_module,
			cast(const char*)n
		);
		if(LLVMCountParams(func) != cast(uint)args.length) {
			writeln("\033[0;31mError: Too little or too much args!\033[0;0m");
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
	ulong value;

	this(ulong value) { this.value = value; }

	override LLVMValueRef gen(GenerationContext ctx) {
		return LLVMConstInt(LLVMInt32Type(),value,false);
	}

	private ulong pow2(uchar n) {
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
		return LLVMConstReal(
			LLVMDoubleType(),
			cast(double)value
		);
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