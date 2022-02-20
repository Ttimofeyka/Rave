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
		    ctx.mod,
		    cast(const char*)"main",
		    ret_type
	    );

	    LLVMBasicBlockRef entry = LLVMAppendBasicBlock(mainf, "entry");
	    LLVMPositionBuilderAtEnd(ctx.builder, entry);

	    LLVMValueRef retval = LLVMConstInt(
	    	LLVMInt32Type(),
	    	cast(ulong)0,
	    	false
	    );
	    LLVMBuildRet(ctx.builder, retval);

	    char *error;
    	LLVMInitializeNativeTarget();
    	LLVMInitializeNativeAsmPrinter();
    	LLVMInitializeNativeAsmParser();

	    LLVMCreateExecutionEngineForModule(&ctx.engine, ctx.mod, &error);

	    LLVMGenericValueRef* argss;
	    LLVMGenericValueRef res = LLVMRunFunction(ctx.engine,mainf,0,argss);
	
	    LLVMWriteBitcodeToFile(ctx.mod, "llvm.bc");
    }
}