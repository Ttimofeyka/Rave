module gen;

import llvm;
import std.stdio : writeln;
import core.stdc.stdlib : exit;

class GenerationContext {
    LLVMExecutionEngineRef engine;
    LLVMModuleRef mod;
    LLVMBuilderRef builder;

    this() {
        mod = LLVMModuleCreateWithName(cast(const char*)"epl");
        builder = LLVMCreateBuilder();
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
	    	cast(ulong)0,
	    	false
	    );
	    LLVMBuildRet(this.builder, retval);

	    char *error;
    	LLVMInitializeNativeTarget();
    	LLVMInitializeNativeAsmPrinter();
    	LLVMInitializeNativeAsmParser();

	    LLVMCreateExecutionEngineForModule(&this.engine, this.mod, &error);

	    LLVMGenericValueRef* argss;
	    LLVMGenericValueRef res = LLVMRunFunction(this.engine,mainf,0,argss);
	
	    LLVMWriteBitcodeToFile(this.mod, "llvm.bc");
    }
}