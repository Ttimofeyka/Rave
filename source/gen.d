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
	}

	LLVMTypeRef* getLLVMArgs(AtstNode[] args) {
		LLVMTypeRef[args.length] llvm_args;
		for(int i=0; i<args.length; i++) {
			llvm_args[i] = getLLVMType(args[i].get(this.typecontext));
		}
		return llvm_args.ptr;
	}

    void gen() {
        LLVMTypeRef* param_types;

	    LLVMTypeRef ret_type = LLVMFunctionType(
		    LLVMInt32Type(),
		    param_types,
		    0,
		    false
	    );

	    LLVMValueRef mainf = LLVMAddFunction(
		    this.mod,
		    cast(const char*)"main",
		    ret_type
	    );

	    LLVMBasicBlockRef entry = LLVMAppendBasicBlock(mainf, "entry");
	    LLVMPositionBuilderAtEnd(this.builder, entry);

	    LLVMValueRef retval = LLVMConstInt(
	    	LLVMInt32Type(),
	    	cast(ulong)5,
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