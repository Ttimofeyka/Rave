module parser.generator.gen;

import llvm;
import std.stdio : writeln;
import core.stdc.stdlib : exit;
import parser.typesystem, parser.util;
import parser.analyzer;
import parser.ast;
import std.conv : to;
import std.range;
import std.string;
import std.algorithm : canFind;
import std.ascii;
import std.uni;
import parser.atst;

class GStack {
    LLVMValueRef[string] globals; // Global variables
    private LLVMValueRef[string] locals; // Local variables
	string[string] structures;
	bool setVar = false;
	GenerationContext ctx;
	string[string] newnames; // For namespace-vars

    this() {}

    void addGlobal(LLVMValueRef v, string n) {
        globals[n] = v;
    }
    
    void addLocal(LLVMValueRef v, string n) {
		locals[n] = v;
	}

	bool isGlobal(string var) {
		if(var in newnames) return globals.keys.canFind(newnames[var]);
		return globals.keys.canFind(var);
	}
	bool isLocal(string var) {
		if(var in newnames) return locals.keys.canFind(newnames[var]);
		return locals.keys.canFind(var);
	}
	bool isVariable(string var) {
		return isGlobal(var) || isLocal(var);
	}

	void set(LLVMValueRef newval, string n) {
		if(n in newnames) {
			if(isGlobal(newnames[n])) globals[newnames[n]] = newval;
			else locals[newnames[n]] = newval;
		}
		else {
			if(isGlobal(n)) globals[n] = newval;
			else locals[n] = newval;
		}
	}

	void remove(string n) {
		if(n in newnames) {
			if(isLocal(newnames[n])) locals.remove(newnames[n]);
			else globals.remove(newnames[n]);
		}
		if(n in locals) locals.remove(n);
		else if(n in globals) globals.remove(n);
		else {}
	}

	void gen_init(GenerationContext ctx) {
		this.ctx = ctx;
	}

	void clean() {locals.clear();}

	LLVMValueRef opIndex(string n)
	{
		string nn = n;
		if(n in newnames) {
			nn = newnames[n];
		}
		if(nn in locals) return locals[nn];
		else if(nn in globals) return globals[nn];
		else if(nn in ctx.gargs.fargs_values) {
			return LLVMGetParam(ctx.currfunc, cast(uint)ctx.gargs.fargs_values[nn]);
		}
		else return null;
	}
}

class GArgs {
	private LLVMTypeRef[string] fargs_types;
	private int[string] fargs_values;

	void set(int v, LLVMTypeRef type, string name) {
		fargs_values[name] = v;
		fargs_types[name] = type;
	}

	void setType(LLVMTypeRef type, string name) {
		fargs_types[name] = type;
	}

	LLVMTypeRef getType(string s) {return fargs_types[s];}

	void clean() {
		fargs_types.clear();
		fargs_values.clear();
	}

	bool isArg(string s) {
		return (s in fargs_types) != null;
	}

	this() {}

	int opIndex(string index)
	{	
		return fargs_values[index];
	}
}

class GPresets {
    private LLVMValueRef[int] presets;
    int i = 0;

    void add(LLVMValueRef f) {presets[i] = f;}
    long length() {return presets.length;}

    LLVMValueRef opIndex(int index) {return presets[index];}
}

class GFuncs {
    LLVMValueRef[string] funcs;
    LLVMTypeRef[string] types;
	TypeFunction[string] typesf;
	int[string] funcs_varargs;
	AtstNode[int] funcs_args;
	string[string] newfuncs;

    void add(string n, LLVMValueRef f, LLVMTypeRef t, TypeFunction tf) {
        funcs[n] = f;
        types[n] = t;
		typesf[n] = tf;
    }

    LLVMTypeRef getType(string n) {
		if(n in newfuncs) return types[newfuncs[n]];
		return types[n];
	}
	TypeFunction getFType(SemanticAnalyzerContext sac, string n) {
		if(n in newfuncs) return typesf[newfuncs[n]];
		if(n !in typesf) {
			// Error
			writeln("\u001b[31mError: undefined function \""~n~"\"!\u001b[0m");
			exit(-1);
		}
		return typesf[n];
	}

    LLVMValueRef opIndex(string n)
    {
	   if(n in newfuncs) return funcs[newfuncs[n]];
       return funcs[n]; 
    }
}

class GStructs {
	LLVMTypeRef[string] ss;
	uint[string[]] vs;
	string[string] structs;
	AstNodeDecl[][string] variables;
	string[string] newstructs;

	this() {}

	void addS(LLVMTypeRef s, string n) {
		this.ss[n] = s;
	}

	void addV(uint index, string name, string sname) {
		this.vs[cast(immutable)[sname,name]] = index;
	}

	void addVar(AstNodeDecl d, string name) {
		variables[name] ~= d;
	}

	LLVMTypeRef getS(string n) {
		if(n in newstructs) return ss[newstructs[n]];
		return ss[n];
	}
	uint getV(string vname, string sname) {
		if(sname in newstructs) return vs[cast(immutable)[newstructs[sname],vname]];
		return vs[cast(immutable)[sname,vname]];
	}
}

bool isSimpleType(string t) {
	return (t.indexOf("[") == -1) && (t.indexOf("*") == -1);
}

AtstNode strToType(string s) {
	s = s.replace("\"","");
	if(isSimpleType(s)) {
		if(s == "void") return new AtstNodeVoid();
		return new AtstNodeName(s);
	}
	// Array or pointer
	int i = 0;
	while(s[i] != '*' && s[i] != '[') i += 1;
	if(s[i] == '*') {
		return new AtstNodePointer(strToType(s[0..i]~s[i+1..$]));
	}
	// Array
	int array_founded = i;
	i += 1;
	string temp = "";
	while(s[i] != ']') {temp ~= s[i]; i+=1;}
	return new AtstNodeArray(strToType(s[0..array_founded]~s[i+1..$]), to!ulong(temp));
}

class GenerationContext {
    LLVMModuleRef mod;
	SemanticAnalyzerContext sema;
	LLVMBuilderRef currbuilder;
	GStack gstack;
	GArgs gargs;
	GFuncs gfuncs;
	GStructs gstructs;
    GPresets presets;
	LLVMValueRef currfunc;
	int basicblocks_count = 0;
    string entryFunction = "main";
    LLVMBasicBlockRef currbb;
	LLVMBasicBlockRef exitbb; // if CYCLE or IF
	LLVMBasicBlockRef thenbb; // if CYCLE or IF
	string target_platform;
	LLVMContextRef context;
	int optLevel;
	LLVMBasicBlockRef whileexitbb;
	bool break_inserted = false;

    this() {
        mod = LLVMModuleCreateWithName(toStringz("rave"));
		context = LLVMContextCreate();
		sema = new SemanticAnalyzerContext(new AtstTypeContext());
		gargs = new GArgs();
        presets = new GPresets();
        gfuncs = new GFuncs();
		gstructs = new GStructs();
		gstack = new GStack();

		// Initialization
		LLVMInitializeAllTargets();
		LLVMInitializeAllAsmParsers();
		LLVMInitializeAllAsmPrinters();
		LLVMInitializeAllTargetInfos();
		LLVMInitializeAllTargetMCs();
    }

	string mangleQualifiedName(string[] parts, bool isFunction) {
		string o = "_Rave" ~ (isFunction ? "f" : "g");
		foreach(part; parts) {
			o ~= to!string(part.length) ~ part;
		}
		return o;
	}

	// Get LLVM type from Type or Atst
	LLVMTypeRef getLLVMType(T)(T t, AnalyzerScope s) {
		static if(is(typeof(t) == Type)) {
			if(auto tb = t.instanceof!TypeBasic) { // If basic type
				switch(tb.basic) {
					case BasicType.t_int:
					case BasicType.t_ushort:
						return LLVMInt32Type();
					case BasicType.t_short:
					case BasicType.t_uchar:
						return LLVMInt16Type();
					case BasicType.t_long:
					case BasicType.t_uint:
					case BasicType.t_usize:
						return LLVMInt64Type();
					case BasicType.t_char:
						return LLVMInt8Type();
					case BasicType.t_float:
						return LLVMFloatType();
					case BasicType.t_bool:
						return LLVMInt1Type(); // Bool
					default: return LLVMInt8Type();
				}
			}
			else if(TypePointer p = t.instanceof!TypePointer) { // If pointer
				if(getLLVMType(p.to,s) == LLVMVoidType()) {
					return LLVMPointerType(LLVMInt8Type(),0);
				}
				return LLVMPointerType(getLLVMType(p.to,s), 0);
			}
        	else if(auto v = t.instanceof!TypeVoid) { // If void
            	return LLVMVoidType();
        	}
			// If unknown - return int
			return LLVMInt32Type();
		}
		else {
			// Array, or...?
			if(AtstNodeArray a = t.instanceof!AtstNodeArray) { // If array
				auto array_type = getLLVMType(a.node,s);
				//return LLVMVectorType(array_type, cast(uint)a.count);
				return LLVMArrayType(array_type, cast(uint)a.count);
			}
			else if(AtstNodeName struc = t.instanceof!AtstNodeName) {
				// If struct
				if(struc.name in s.genctx.gstructs.ss) {
					return s.genctx.gstructs.getS(struc.name);
				}
				else if(struc.name == "args") {
					return LLVMPointerType(
						LLVMPointerType(
							LLVMInt8Type(),
							0
						),
						0
					);
				}
			}
			else if(AtstNodePointer p = t.instanceof!AtstNodePointer) {
				if(p.toString() == "void*") {
					return LLVMPointerType(
						LLVMInt8Type(),
						0
					);
				}
				return LLVMPointerType(
					getLLVMType(p.node, s),
					0
				);
			}
			// Else get Type and return getLLVMType(Type)
			return getLLVMType(t.get(s),s);
		}
	}

	string llvmTypeToString(LLVMTypeRef type) {
		auto i1 = LLVMInt1Type();
		auto i8 = LLVMInt8Type();
		auto i16 = LLVMInt16Type();
		auto i32 = LLVMInt32Type();
		auto i64 = LLVMInt64Type();
		auto name = "";
		if(type == i1) name = "bool";
		else if(type == i8) name = "char";
		else if(type == i16) name = "short";
		else if(type == i32) name = "int";
		else if(type == i64) name = "long";
		else name = "nobasic";
		return name;
	}

	bool isIntType(LLVMTypeRef t) {
		return (t==LLVMInt1Type())||(t==LLVMInt8Type())
			 ||(t==LLVMInt16Type())||(t==LLVMInt32Type());
	}

    void gen(string t, AnalyzerScope s, AstNode[] nodes, string file, bool debugMode) {
		for(int i=0; i<nodes.length; i++) {
			//nodes[i].debugPrint(0);
			nodes[i].gen(s);
		}
		this.target_platform = t;
		genTarget(file, debugMode);
	}

	void genTarget(string file,bool d) {
		// Optimization
		// optLevel - level of optimization(0, 1(default), 2 or 3)
		LLVMPassManagerBuilderRef p = LLVMPassManagerBuilderCreate();
		LLVMPassManagerBuilderSetOptLevel(p, cast(uint)optLevel);
		LLVMPassManagerRef pm = LLVMCreatePassManager();
		LLVMPassManagerBuilderPopulateModulePassManager(p, pm);
		LLVMRunPassManager(pm, mod);

		// Setting target triple
		char* errors;
		LLVMTargetRef target;
    	LLVMGetTargetFromTriple(toStringz(target_platform), &target, &errors);
    	LLVMTargetMachineRef machine = LLVMCreateTargetMachine(
			target, 
			toStringz(target_platform), 
			"generic", 
			LLVMGetHostCPUFeatures(),
			 LLVMCodeGenLevelDefault, 
			 LLVMRelocDefault, 
		LLVMCodeModelDefault);

		LLVMDisposeMessage(errors);

    	LLVMSetTarget(this.mod, toStringz(target_platform));
    	LLVMTargetDataRef datalayout = LLVMCreateTargetDataLayout(machine);
    	char* datalayout_str = LLVMCopyStringRepOfTargetData(datalayout);
    	LLVMSetDataLayout(this.mod, datalayout_str);
    	LLVMDisposeMessage(datalayout_str);
		char* file_ptr = cast(char*)toStringz(file);

		if(d) {
			char* err;
			LLVMPrintModuleToFile(this.mod, "debug.ll", &err);
		}

    	LLVMTargetMachineEmitToFile(machine,this.mod,file_ptr, LLVMObjectFile, &errors);
	}
}
