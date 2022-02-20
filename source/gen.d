module gen;

import llvm;
import std.stdio : writeln;
import core.stdc.stdlib : exit;
import typesystem, util;
import ast;
import std.conv : to;
import std.string;

class GenerationContext {
    LLVMExecutionEngineRef engine;
    LLVMModuleRef mod;
    LLVMBuilderRef builder;
	AtstTypeContext typecontext;

    this() {
        mod = LLVMModuleCreateWithName(cast(const char*)"epl");
        builder = LLVMCreateBuilder();

		// Initialization
		LLVMInitializeAllTargets();
		LLVMInitializeAllAsmParsers();
		LLVMInitializeAllAsmPrinters();
		LLVMInitializeAllTargetInfos();
		LLVMInitializeAllTargetMCs();
    }

	LLVMTypeRef getLLVMType(Type t) {
		if(t.instanceof!(TypeBasic)) {
			TypeBasic tb = cast(TypeBasic)t;
			switch(tb.basic) {
				case BasicType.t_int:
				case BasicType.t_uint:
					return LLVMInt32Type();
				case BasicType.t_short:
					return LLVMInt16Type();
				case BasicType.t_uchar:
				case BasicType.t_char:
					return LLVMInt8Type();
				case BasicType.t_long:
				case BasicType.t_ulong:
					return LLVMInt64Type();
				default: return LLVMInt16Type();
			}
		}
		else return LLVMInt32Type();
	}

	LLVMTypeRef* getLLVMArgs(AtstNode[] argss) {
		LLVMTypeRef* llvm_args =
		 cast(LLVMTypeRef*)(LLVMTypeRef.sizeof*argss.length);
		for(int i=0; i<argss.length; i++) {
			llvm_args[i] = getLLVMType(argss[i].get(this.typecontext));
		}
		return llvm_args;
	}

	void genFunc(AstNode node,string entryf) {
		AstNodeFunction astf = 
			cast(AstNodeFunction)node;
		LLVMTypeRef* param_types = 
		getLLVMArgs(astf.decl.signature.args);

		AstNodeBlock f_body = cast(AstNodeBlock)astf.body_;
		AstNodeReturn ret_ast = cast(AstNodeReturn)
			f_body.nodes[0];

		LLVMTypeRef ret_type = LLVMFunctionType(
		    LLVMInt32Type(),
		    param_types,
		    cast(uint)astf.decl.signature.args.length,
		    false
	    );

		LLVMValueRef func = LLVMAddFunction(
		    this.mod,
		    cast(const char*)(astf.decl.name~"\0"),
		    ret_type
	    );

		LLVMBasicBlockRef entry = LLVMAppendBasicBlock(func,cast(const char*)"entry");
	    LLVMPositionBuilderAtEnd(this.builder, entry);
		AstNodeInt retint = cast(AstNodeInt)ret_ast.value;

	    LLVMValueRef retval = LLVMConstInt(
	    	LLVMInt32Type(),
	    	retint.value,
	    	false
	    );
	    LLVMBuildRet(this.builder, retval);
	}

    void gen(AstNode[] nodes,string file,bool debugMode,string entryf) {
		for(int i=0; i<nodes.length; i++) {
			if(nodes[i].instanceof!(AstNodeFunction)) {
				genFunc(nodes[i],entryf);
			}
		}
		genTarget(file,debugMode);
	}

	void genTarget(string file,bool d) {
		if(d) 
			LLVMWriteBitcodeToFile(this.mod,cast(const char*)(file~".debug.be"));

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

    	LLVMTargetMachineEmitToFile(machine,this.mod,file_ptr, LLVMObjectFile, &errors);
	}
}