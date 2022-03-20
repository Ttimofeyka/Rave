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

class GStack {
    private LLVMValueRef[string] globals; // Global variables
    private LLVMValueRef[string] locals; // Local variables
	bool setVar = false;

    this() {}

    void addGlobal(LLVMValueRef v, string n) {
        globals[n] = v;
    }
    
    void addLocal(LLVMValueRef v, string n) {
		locals[n] = v;
	}

	bool isGlobal(string var) {
		return globals.keys.canFind(var);
	}
	bool isLocal(string var) {
		return locals.keys.canFind(var);
	}
	bool isVariable(string var) {
		return isGlobal(var) || isLocal(var);
	}

	void set(LLVMValueRef newval, string n) {
		if(isGlobal(n)) globals[n] = newval;
		else locals[n] = newval;
	}

	void remove(string n) {
		if(n in locals) locals.remove(n);
		else if(n in globals) globals.remove(n);
		else {}
	}

	void clean() {locals.clear();}

	LLVMValueRef opIndex(string n)
	{
		if(n in locals) return locals[n];
		else if(n in globals) return globals[n];
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
    private LLVMValueRef[string] funcs;
    private LLVMTypeRef[string] types;

    void add(string n, LLVMValueRef f, LLVMTypeRef t) {
        funcs[n] = f;
        types[n] = t;
    }

    LLVMTypeRef getType(string n) {return types[n];}

    LLVMValueRef opIndex(string n)
    {
       return funcs[n]; 
    }
}

class GenerationContext {
    LLVMModuleRef mod;
	SemanticAnalyzerContext sema;
	LLVMBuilderRef currbuilder;
	GStack gstack;
	GArgs gargs;
	GFuncs gfuncs;
    GPresets presets;
	LLVMValueRef currfunc;
	int basicblocks_count = 0;
    string entryFunction = "main";
    LLVMBasicBlockRef currbb;
	LLVMBasicBlockRef exitbb; // if CYCLE or IF
	LLVMBasicBlockRef thenbb; // if CYCLE or IF

    this() {
        mod = LLVMModuleCreateWithName(toStringz("rave"));
		sema = new SemanticAnalyzerContext(new AtstTypeContext());
		gstack = new GStack();
		gargs = new GArgs();
        presets = new GPresets();
        gfuncs = new GFuncs();

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

	LLVMTypeRef getLLVMType(T)(T t, AnalyzerScope s) {
		static if(is(typeof(t) == Type)) {
			if(auto tb = t.instanceof!TypeBasic) {
				switch(tb.basic) {
					case BasicType.t_int:
						return LLVMInt32Type();
					case BasicType.t_short:
						return LLVMInt16Type();
					case BasicType.t_long:
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
			else if(auto p = t.instanceof!TypePointer) {
				return LLVMPointerType(getLLVMType(p.to, s), 1);
			}
        	else if(auto v = t.instanceof!TypeVoid) {
            	return LLVMVoidType();
        	}
			return LLVMInt32Type();
		}
		else {
			// Array, or...?
			if(AtstNodeArray a = t.instanceof!AtstNodeArray) {
				// If array then
				auto array_type = getLLVMType(a.node,s);
				return LLVMVectorType(array_type, cast(uint)a.count);
			}
			// Else get Type and return getLLVMType(Type)
			return getLLVMType(t.get(s),s);
		}
	}

	bool isIntType(LLVMTypeRef t) {
		return (t==LLVMInt1Type())||(t==LLVMInt8Type())
			 ||(t==LLVMInt16Type())||(t==LLVMInt32Type());
	}

    void gen(AnalyzerScope s, AstNode[] nodes, string file, bool debugMode) {
		for(int i=0; i<nodes.length; i++) {
			//nodes[i].debugPrint(0);
			nodes[i].gen(s);
		}
		genTarget(file, debugMode);
	}

	void genTarget(string file,bool d) {
		if(d) LLVMWriteBitcodeToFile(this.mod,cast(const char*)(file~".be"));

		char* errors;
		LLVMTargetRef target;
    	LLVMGetTargetFromTriple(LLVMGetDefaultTargetTriple(), &target, &errors);

    	LLVMTargetMachineRef machine = LLVMCreateTargetMachine(
			target, 
			LLVMGetDefaultTargetTriple(), 
			"generic", 
			LLVMGetHostCPUFeatures(),
			 LLVMCodeGenLevelDefault, 
			 LLVMRelocDefault, 
		LLVMCodeModelDefault);

		LLVMDisposeMessage(errors);

    	LLVMSetTarget(this.mod, LLVMGetDefaultTargetTriple());
    	LLVMTargetDataRef datalayout = LLVMCreateTargetDataLayout(machine);
    	char* datalayout_str = LLVMCopyStringRepOfTargetData(datalayout);
    	LLVMSetDataLayout(this.mod, datalayout_str);
    	LLVMDisposeMessage(datalayout_str);
		char* file_ptr = cast(char*)toStringz(file);

		char* err;
		LLVMPrintModuleToFile(this.mod, "tmp.ll", &err);

    	LLVMTargetMachineEmitToFile(machine,this.mod,file_ptr, LLVMObjectFile, &errors);
	}
}
