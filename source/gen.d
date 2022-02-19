module gen;

import llvm;

class GenerationContext {
    LLVMContextRef context;
    LLVMBuilderRef builder;
    LLVMModuleRef main_module;
    LLVMValueRef[string] values;

    this() {
        context = LLVMContextCreate();
        builder = LLVMCreateBuilder();
        main_module = LLVMModuleCreateWithName(cast(const char*)"main");
    }
}