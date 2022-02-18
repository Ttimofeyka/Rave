module gen;

import llvm;

class LLVMValues {
    LLVMValueRef[int] values;
    int i = 0;

    LLVMValueRef opIndex(int index) {
        return values[index];
    }

    void insertBack(LLVMValueRef r) {
        values[i] = r;
        i+=1;
    }

    int length() { return cast(int)values.length; }
}

class GenerationContext {
    LLVMContextRef context;
    LLVMBuilderRef builder;
    LLVMModuleRef main_module;
    LLVMValues values;

    this() {
        context = LLVMContextCreate();
        builder = LLVMCreateBuilder();
        main_module = LLVMModuleCreateWithName(cast(const char*)"main");
    }
}