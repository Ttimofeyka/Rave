module parser.analyzer;
import std.stdio;
import std.container : SList;
import parser.ast;
import parser.typesystem;
import parser.generator.gen;
import lexer.tokens : SourceLocation;

struct AnalyzerError {
	string msg;
	SourceLocation where;
}

// class SemanticAnalyzerContext;

class ScopeVar {
    Type type;
    bool isStatic, isExtern;

	this(Type type, bool isStatic = false, bool isExtern = false) {
		this.type = type;
		this.isStatic = isStatic;
		this.isExtern = isExtern;
	}
}

class ScopeVarIntConstant : ScopeVar {
	ulong value;

	this(Type type, ulong value, bool isStatic = false) {
		super(type, isStatic, false);
		this.value = value;
	}
}

/** 
 * Generic namespace (a struct, an enum, a function, namespaces)
 */
class Namespace {
	private Namespace parent;
	private ScopeVar[string] vars;

	public this(Namespace parent = null) {
		this.parent = parent;
		this.vars = null;
	}

	public ScopeVar get(string name) {
		if(name in vars) return vars[name];
		else if(parent !is null) return parent.get(name);
		return null;
	}

	public bool has(string name) {
		if(name in vars) return true;
		else if(parent !is null) return parent.has(name);
		return false;
	}

	public void setHere(string name, ScopeVar v) {
		this.vars[name] = v;
	}
}

class AnalyzerScope {
	bool hadReturn = false; 
	Type returnType = null;
	Type neededReturnType = null;

	Namespace ns;
	AnalyzerScope parent;

	SemanticAnalyzerContext ctx;
	GenerationContext genctx;

	this(SemanticAnalyzerContext ctx, GenerationContext genctx, AnalyzerScope parent = null) {
		this.ctx = ctx;
		this.genctx = genctx;
		this.parent = parent;
		this.ns = this.parent is null ? new Namespace() : new Namespace(this.parent.ns);
	}
}

class SemanticAnalyzerContext {
	AstNodeFunction currentFunc = null;
	AtstTypeContext typeContext;
	bool writeErrors = false;
	SList!AnalyzerError errs;
	size_t errorCount;

	/// Create a SemanticAnalyzerContext
	/// Params:
	///   typeContext = the type context. must be shared between the analyzer and the generator.
	this(AtstTypeContext typeContext) {
		this.typeContext = typeContext;
	}

	/** 
	 * Write all errors to stdout.
	 */
	void flushErrors() {
		foreach(err; errs) {
			writeln(err.where, ": \033[0;31mError:\033[0;0m ", err.msg);
		}
		errorCount = 0;
	}

	/** 
	 * Add an error to the front of the error list. Won't do anything if writeErrors is false.
	 * Params:
	 *   msg = The error message.
	 *   where = The error location.
	 */
	void addError(string msg, SourceLocation where = SourceLocation(0, 0, "")) {
		if(!writeErrors) return;
		errs.insertFront(AnalyzerError(msg, where));
		++errorCount;
	}
}
