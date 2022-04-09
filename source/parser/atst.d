module parser.atst;

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
import parser.mparser;

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