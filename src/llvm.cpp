/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "./include/llvm.hpp"
#include <llvm-c/Target.h>
#include "./include/parser/ast.hpp"
#include <iostream>

#include <llvm/Target/TargetMachine.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/Analysis/TargetLibraryInfo.h>
#include <llvm/Transforms/IPO.h>
#include <llvm/IR/LegacyPassManager.h>

bool LLVM::isPointerType(LLVMTypeRef type) {
    return LLVMGetTypeKind(type) == LLVMPointerTypeKind;
}

bool LLVM::isPointer(LLVMValueRef value) {
    #if LLVM_VERSION_MAJOR <= 16
        return LLVM::isPointerType(LLVMTypeOf(value));
    #else
        return LLVMIsAGlobalVariable(value) || LLVMIsAAllocaInst(value); || LLVMIsAIntToPtrInst(value); // TODO
    #endif
}

LLVMValueRef LLVM::load(LLVMValueRef value, const char* name) {
    #if LLVM_VERSION_MAJOR >= 15
        return LLVMBuildLoad2(generator->builder, LLVMGetElementType(LLVMTypeOf(value)), value, name);
    #else
        return LLVMBuildLoad(generator->builder, value, name);
    #endif
}

LLVMValueRef LLVM::call(LLVMValueRef fn, LLVMValueRef* args, unsigned int argsCount, const char* name) {
    #if LLVM_VERSION_MAJOR >= 15
        return LLVMBuildCall2(generator->builder, LLVMGetReturnType(LLVMTypeOf(fn)), fn, args, argsCount, name);
    #else
        return LLVMBuildCall(generator->builder, fn, args, argsCount, name);
    #endif
}

LLVMValueRef LLVM::gep(LLVMValueRef ptr, LLVMValueRef* indices, unsigned int indicesCount, const char* name) {
    #if LLVM_VERSION_MAJOR >= 15
        return LLVMBuildGEP2(generator->builder, LLVMGetElementType(LLVMTypeOf(ptr)), ptr, indices, indicesCount, name);
    #else
        return LLVMBuildGEP(generator->builder, ptr, indices, indicesCount, name);
    #endif
}

LLVMValueRef LLVM::inboundsGep(LLVMValueRef ptr, LLVMValueRef* indices, unsigned int indicesCount, const char* name) {
    #if LLVM_VERSION_MAJOR >= 15
        return LLVMBuildInBoundsGEP2(generator->builder, LLVMGetElementType(LLVMTypeOf(ptr)), ptr, indices, indicesCount, name);
    #else
        return LLVMBuildInBoundsGEP(generator->builder, ptr, indices, indicesCount, name);
    #endif
}

LLVMValueRef LLVM::constInboundsGep(LLVMValueRef ptr, LLVMValueRef* indices, unsigned int indicesCount) {
    #if LLVM_VERSION_MAJOR >= 15
        return LLVMConstInBoundsGEP2(LLVMGetElementType(LLVMTypeOf(ptr)), ptr, indices, indicesCount);
    #else
        return LLVMConstInBoundsGEP(ptr, indices, indicesCount);
    #endif
}

LLVMValueRef LLVM::structGep(LLVMValueRef ptr, unsigned int idx, const char* name) {
    #if LLVM_VERSION_MAJOR >= 15
        return LLVMBuildStructGEP2(
            generator->builder, LLVMGetElementType(LLVMTypeOf(ptr)),
            ptr, idx, name
        );
    #else
        return LLVMBuildStructGEP(generator->builder, ptr, idx, name);
    #endif
}

LLVMValueRef LLVM::alloc(LLVMTypeRef type, const char* name) {
    LLVMPositionBuilder(generator->builder, LLVMGetFirstBasicBlock(generator->functions[currScope->funcName]), LLVMGetFirstInstruction(LLVMGetFirstBasicBlock(generator->functions[currScope->funcName])));
    LLVMValueRef value = LLVMBuildAlloca(generator->builder, type, name);
    LLVMPositionBuilderAtEnd(generator->builder, generator->currBB);
    return value;
}

void LLVM::setFastMath(LLVMBuilderRef builder, bool infs, bool nans, bool arcp, bool nsz) {
    llvm::FastMathFlags flags;
    if(infs) flags.setNoInfs(true);
    if(nans) flags.setNoNaNs(true);
    if(arcp) flags.setAllowReciprocal(true);
    if(nsz) flags.setNoSignedZeros(true);
    llvm::unwrap(builder)->setFastMathFlags(flags);
}

void LLVM::setFastMathAll(LLVMBuilderRef builder, bool value) {
    llvm::FastMathFlags flags;
    flags.setFast(value);
    llvm::unwrap(builder)->setFastMathFlags(flags);
}