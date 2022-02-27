module parser.ast;

import std.stdio, std.conv;
import std.array : join;
import std.algorithm.iteration : map;
import std.algorithm : canFind;
import parser.gen, lexer.tokens;
import llvm;
import std.conv : to;
import core.stdc.stdlib : exit;
import parser.typesystem;
import parser.util;
import parser.analyzer;
import std.string;

/// We have two separate syntax trees: for types and for values.

class AtstTypeContext {
	Type[string] types;

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
			s ~= "): "; // ~ signature.ret.toString();
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

	this(FunctionDeclaration decl, AstNode body_) {
		this.decl = decl;
		this.body_ = body_;
	}

	override void analyze(AnalyzerScope s, Type) {
		auto lastCurrentFunc = s.ctx.currentFunc;
		s.ctx.currentFunc = this;

		this.body_.analyze(s, this.decl.signature.ret.get(s.ctx.typeContext));
		actualDecl = new FunctionSignatureTypes(s.returnType, []);

		s.ctx.currentFunc = lastCurrentFunc;
	}

	override LLVMValueRef gen(GenerationContext ctx) {
		LLVMBuilderRef builder = LLVMCreateBuilder();
		LLVMTypeRef* param_types;

		AstNodeBlock f_body = cast(AstNodeBlock)body_;
		AstNodeReturn ret_ast = cast(AstNodeReturn)
			f_body.nodes[0];

		LLVMTypeRef ret_type = LLVMFunctionType(
		    ctx.getLLVMType(actualDecl.ret),
		    param_types,
		    cast(uint)decl.signature.args.length,
		    false
	    );

		LLVMValueRef func = LLVMAddFunction(
		    ctx.mod,
		    cast(const char*)(toStringz(ctx.mangleQualifiedName([decl.name], true))),
		    ret_type
	    );

		LLVMBasicBlockRef entry = LLVMAppendBasicBlock(func,cast(const char*)"entry");
		LLVMPositionBuilderAtEnd(builder, entry);

	    LLVMValueRef retval = ret_ast.gen(ctx);
	    LLVMBuildRet(builder, retval);

		ctx.currbuilder = builder;

		LLVMVerifyFunction(func, 0);

		return func;
	}

	debug {
		override void debugPrint(int indent) {
			writeTabs(indent);
			writeln("Function decl: ", decl.toString(), actualDecl.ret.toString());
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
		LLVMValueRef a;
		return a;
	}

	private void checkTypes(AnalyzerScope s, Type neededType) {
		if(lhs.getType(s).instanceof!(TypeUnknown)) {
			lhs.analyze(s, neededType);
		}

		if(rhs.getType(s).instanceof!(TypeUnknown)) {
			rhs.analyze(s, neededType);
		}

		if(lhs.getType(s) == neededType
		&& rhs.getType(s) == neededType) {
  			this.valueType = neededType;
		}
		else {
			writeln("Error!");
			exit(-1);
		}
	}

	override void analyze(AnalyzerScope s, Type neededType) {
		if(_isArithmetic()) {
			if(!instanceof!TypeBasic(neededType)) {
				s.ctx.addError("Non-basic type required for an arithmetic binary operation");
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

	override void analyze(AnalyzerScope s, Type neededType) {
		auto newScope = new AnalyzerScope(s.ctx, s);
		foreach(node; this.nodes) {
			node.analyze(newScope, neededType);
			if(newScope.hadReturn && !neededType.instanceof!TypeUnknown) {
				if(!neededType.assignable(newScope.returnType)) {
					s.ctx.addError("Wrong return type: expected '" ~ neededType.toString()
						~ "', got: '" ~ newScope.returnType.toString() ~ '\'');
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
		writeln(parent.decl.name);
		return value.gen(ctx);
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
		if(name in ctx.global_vars) {
			return ctx.global_vars[name];
		}
		assert(0);
	}

	override Type getType(AnalyzerScope s) {
		auto t = s.get(name);
		if(auto v = t.instanceof!VariableSignatureTypes)
			return v.type;
		
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
		if(decl.name in ctx.global_vars) {
			writeln("The variable " ~ decl.name ~ " has been declared multiple times!");
			exit(-1);
		}
		
		LLVMValueRef constval = value.gen(ctx);

		LLVMValueRef var = LLVMAddGlobal(
			ctx.mod,
			ctx.getLLVMType(decl.type.get(ctx.typecontext)),
			toStringz(decl.name)
		);

		LLVMSetInitializer(var, constval);

		ctx.global_vars[decl.name] = var;

		return var;
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

	override LLVMValueRef gen(GenerationContext ctx) {
		LLVMValueRef a;
		// _EPLf4exit, not exit! ctx.mangleQualifiedName([name], true) -> string
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
			this.valueType = neededType;
		}

		writeln("int type ", this.valueType.toString());
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
	
	override Type getType(AnalyzerScope s) {
		return new TypeBasic(BasicType.t_float);
	}
		
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
