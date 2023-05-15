module llvm.functions;

import std.array : array;
import std.algorithm.iteration : map, joiner;
import std.range : chain;

import llvm.config;
import llvm.types;

private nothrow auto orEmpty(T)(T v)
{
    return v? v : "";
}

nothrow void LLVMInitializeAllTargetInfos()
{
    mixin(LLVM_Targets.map!(t => "LLVMInitialize" ~ t ~ "TargetInfo();").joiner.array.orEmpty);
}

nothrow void LLVMInitializeAllTargets()
{
    mixin(LLVM_Targets.map!(t => "LLVMInitialize" ~ t ~ "Target();").joiner.array.orEmpty);
}

nothrow void LLVMInitializeAllTargetMCs()
{
    mixin(LLVM_Targets.map!(t => "LLVMInitialize" ~ t ~ "TargetMC();").joiner.array.orEmpty);
}

nothrow void LLVMInitializeAllAsmPrinters()
{
    mixin(LLVM_AsmPrinters.map!(t => "LLVMInitialize" ~ t ~ "AsmPrinter();").joiner.array.orEmpty);
}

nothrow void LLVMInitializeAllAsmParsers()
{
    mixin(LLVM_AsmParsers.map!(t => "LLVMInitialize" ~ t ~ "AsmParser();").joiner.array.orEmpty);
}

nothrow void LLVMInitializeAllDisassemblers()
{
    mixin(LLVM_Disassemblers.map!(t => "LLVMInitialize" ~ t ~ "Disassembler();").joiner.array.orEmpty);
}

nothrow LLVMBool LLVMInitializeNativeTarget()
{
    static if (LLVM_NativeTarget != "") {
        mixin("LLVMInitialize" ~ LLVM_NativeTarget ~ "TargetInfo();");
        mixin("LLVMInitialize" ~ LLVM_NativeTarget ~ "Target();");
        mixin("LLVMInitialize" ~ LLVM_NativeTarget ~ "TargetMC();");
        return 0;
    } else {
        return 1;
    }
}

static if (LLVM_Version >= asVersion(3, 4, 0))
{
    nothrow LLVMBool LLVMInitializeNativeAsmParser()
    {
        static if (LLVM_NativeTarget != "") {
            mixin("LLVMInitialize" ~ LLVM_NativeTarget ~ "AsmParser();");
            return 0;
        } else {
            return 1;
        }
    }

    nothrow LLVMBool LLVMInitializeNativeAsmPrinter()
    {
        static if (LLVM_NativeTarget != "") {
            mixin("LLVMInitialize" ~ LLVM_NativeTarget ~ "AsmPrinter();");
            return 0;
        } else {
            return 1;
        }
    }

    nothrow LLVMBool LLVMInitializeNativeDisassembler()
    {
        static if (LLVM_NativeTarget != "") {
            mixin("LLVMInitialize" ~ LLVM_NativeTarget ~ "Disassembler();");
            return 0;
        } else {
            return 1;
        }
    }
}

     version (LLVM_Load) public import llvm.functions.load;
else                     public import llvm.functions.link;
