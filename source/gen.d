module gen;

import llvm;
import std.stdio : writeln;
import core.stdc.stdlib : exit;
import typesystem, util;
import ast;

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
					return LLVMInt32Type();
				case BasicType.t_short:
					return LLVMInt16Type();
				case BasicType.t_uchar:
				case BasicType.t_char:
					return LLVMInt8Type();
				case BasicType.t_long:
					return LLVMInt64Type();
				default: return LLVMIntType(1);
			}
		}
		assert(0);
	}

	LLVMTypeRef* getLLVMArgs(AtstNode[] argss) {
		LLVMTypeRef* llvm_args =
		 cast(LLVMTypeRef*)(LLVMTypeRef.sizeof*argss.length);
		for(int i=0; i<argss.length; i++) {
			llvm_args[i] = getLLVMType(argss[i].get(this.typecontext));
		}
		return llvm_args;
	}

    void gen(AstNode[] nodes) {
		AstNodeFunction mainf_astf = 
			cast(AstNodeFunction)nodes[0];
        LLVMTypeRef* param_types = 
		getLLVMArgs(mainf_astf.decl.signature.args);

		AstNodeBlock mainf_body = cast(AstNodeBlock)mainf_astf.body_;
		AstNodeReturn ret_ast = cast(AstNodeReturn)
			mainf_body.nodes[0];

	    LLVMTypeRef ret_type = LLVMFunctionType(
		    LLVMInt32Type(),
		    param_types,
		    cast(uint)mainf_astf.decl.signature.args.length,
		    false
	    );

	    LLVMValueRef mainf = LLVMAddFunction(
		    this.mod,
		    cast(const char*)"main",
		    ret_type
	    );

	    LLVMBasicBlockRef entry = LLVMAppendBasicBlock(mainf, "entry");
	    LLVMPositionBuilderAtEnd(this.builder, entry);

		AstNodeInt retint = cast(AstNodeInt)ret_ast.value;

	    LLVMValueRef retval = LLVMConstInt(
	    	LLVMInt32Type(),
	    	retint.value,
	    	false
	    );
	    LLVMBuildRet(this.builder, retval);

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

    	LLVMTargetMachineEmitToFile(machine, this.mod, cast(char*)"llvm.o", LLVMObjectFile, &errors);
	}
}