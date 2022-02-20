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
}