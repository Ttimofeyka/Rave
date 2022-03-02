module parser.generator.gen;

import llvm;
import std.stdio : writeln;
import core.stdc.stdlib : exit;
import parser.typesystem, parser.util;
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
	private int[string] fargs_numbers;

	void set(int v, LLVMTypeRef type, string name) {
		fargs_numbers[name] = v;
		fargs_types[name] = type;
	}

	int getNum(string s) {return fargs_numbers[s];}
	LLVMTypeRef getType(string s) {return fargs_types[s];}

	this() {}
}

class GFuncs {
	private LLVMValueRef[string] funcs;

	this() {}

	void set(LLVMValueRef v, string n) {
		funcs[n] = v;
	}

	LLVMValueRef opIndex(string index) {
		return funcs[index];
	}
}

class GenerationContext {
    LLVMModuleRef mod;
	AtstTypeContext typecontext;
	LLVMBuilderRef currbuilder;
	GStack gstack;
	GArgs gargs;
	GFuncs gfuncs;

    this() {
        mod = LLVMModuleCreateWithName(toStringz("epl"));
		typecontext = new AtstTypeContext();
		gstack = new GStack();
		gargs = new GArgs();
		gfuncs = new GFuncs();

		// Initialization
		LLVMInitializeAllTargets();
		LLVMInitializeAllAsmParsers();
		LLVMInitializeAllAsmPrinters();
		LLVMInitializeAllTargetInfos();
		LLVMInitializeAllTargetMCs();

		typecontext.setType("int",new TypeBasic(BasicType.t_int));
		typecontext.setType("short",new TypeBasic(BasicType.t_short));
		typecontext.setType("char",new TypeBasic(BasicType.t_char));
		typecontext.setType("long",new TypeBasic(BasicType.t_long));
    }

	string mangleQualifiedName(string[] parts, bool isFunction) {
		string o = "_EPL" ~ (isFunction ? "f" : "g");
		foreach(part; parts) {
			o ~= to!string(part.length) ~ part;
		}
		return o;
	}

	LLVMTypeRef getLLVMType(Type t) {
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
			return LLVMPointerType(getLLVMType(p.to), 0);
		}
		return LLVMInt32Type();
	}

    void gen(AstNode[] nodes, string file, bool debugMode) {
		for(int i=0; i<nodes.length; i++) {
			nodes[i].gen(this);
		}
		genTarget(file, debugMode);
	}

	void genTarget(string file,bool d) {
		if(d) 
			LLVMWriteBitcodeToFile(this.mod,cast(const char*)(file~".be"));

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