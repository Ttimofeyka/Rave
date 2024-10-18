/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "./include/llvm.hpp"
#include <llvm-c/Target.h>
#include "./include/parser/ast.hpp"
#include "./include/parser/nodes/NodeFunc.hpp"
#include "./include/parser/nodes/NodeStruct.hpp"
#include "./include/parser/nodes/NodeVar.hpp"
#include <iostream>

#include <llvm/Target/TargetMachine.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/Analysis/TargetLibraryInfo.h>
#include <llvm/Transforms/IPO.h>
#include <llvm/IR/LegacyPassManager.h>

// Wrapper for the LLVMBuildLoad2 function using RaveValue.
RaveValue LLVM::load(RaveValue value, const char* name, int loc) {
    return {LLVMBuildLoad2(generator->builder, generator->genType(value.type->getElType(), loc), value.value, name), value.type->getElType()};
}

// Wrapper for the LLVMBuildCall2 function using RaveValue.
RaveValue LLVM::call(RaveValue fn, LLVMValueRef* args, unsigned int argsCount, const char* name) {
    TypeFunc* tfunc = instanceof<TypePointer>(fn.type) ? (TypeFunc*)fn.type->getElType() : (TypeFunc*)fn.type;

    std::vector<LLVMTypeRef> types;
    for(int i=0; i<tfunc->args.size(); i++) types.push_back(generator->genType(tfunc->args[i]->type, -1));

    return {LLVMBuildCall2(generator->builder, LLVMFunctionType(generator->genType(tfunc->main, -1), types.data(), types.size(), tfunc->isVarArg), fn.value, args, argsCount, name), tfunc->main};
}

// Wrapper for the LLVMBuildCall2 function using RaveValue; accepts a vector of RaveValue instead of pointer to LLVMValueRef.
RaveValue LLVM::call(RaveValue fn, std::vector<RaveValue> args, const char* name) {
    TypeFunc* tfunc = instanceof<TypePointer>(fn.type) ? (TypeFunc*)fn.type->getElType() : (TypeFunc*)fn.type;
    std::vector<LLVMValueRef> lArgs;
    
    for(int i=0; i<args.size(); i++) lArgs.push_back(args[i].value);

    std::vector<LLVMTypeRef> types;
    for(int i=0; i<tfunc->args.size(); i++) {
        if(instanceof<TypeStruct>(tfunc->args[i]->type)) {
            TypeStruct* tstruct = (TypeStruct*)tfunc->args[i]->type;
            if(AST::structTable.find(tstruct->name) == AST::structTable.end() && generator->toReplace.find(tstruct->name) == generator->toReplace.end()) {
                types.push_back(generator->genType(args[i].type->getElType(), -2));
            }
            else types.push_back(generator->genType(tfunc->args[i]->type, -2));
        }
        else types.push_back(generator->genType(tfunc->args[i]->type, -2));
    }

    return {LLVMBuildCall2(generator->builder, LLVMFunctionType(generator->genType(tfunc->main, -2), types.data(), types.size(), tfunc->isVarArg), fn.value, lArgs.data(), lArgs.size(), name), tfunc->main};
}

// Wrapper for the LLVMConstInBoundsGEP2 function using RaveValue.
RaveValue LLVM::cInboundsGep(RaveValue ptr, LLVMValueRef* indices, unsigned int indicesCount) {
    return {LLVMConstInBoundsGEP2(generator->genType(ptr.type->getElType(), -1), ptr.value, indices, indicesCount), ptr.type->getElType()};
}

// Wrapper for the LLVMBuildGEP2 function using RaveValue.
RaveValue LLVM::gep(RaveValue ptr, LLVMValueRef* indices, unsigned int indicesCount, const char* name) {
    return {LLVMBuildGEP2(generator->builder, generator->genType(ptr.type->getElType(), -1), ptr.value, indices, indicesCount, name), new TypePointer(ptr.type->getElType())};
}

// Wrapper for the LLVMBuildStructGEP2 function using RaveValue.
RaveValue LLVM::structGep(RaveValue ptr, unsigned int idx, const char* name) {
    TypeStruct* ts = instanceof<TypePointer>(ptr.type) ? (TypeStruct*)ptr.type->getElType() : (TypeStruct*)ptr.type;

    return {LLVMBuildStructGEP2(
        generator->builder, generator->genType(ts, -1),
        ptr.value, idx, name
    ), new TypePointer(AST::structTable[ts->name]->getVariables()[idx]->getType())};
}

// Wrapper for the LLVMBuildAlloca function using RaveValue. Builds alloca at the first basic block for saving stack memory (C behaviour).
RaveValue LLVM::alloc(Type* type, const char* name) {
    LLVMPositionBuilder(generator->builder, LLVMGetFirstBasicBlock(generator->functions[currScope->funcName].value), LLVMGetFirstInstruction(LLVMGetFirstBasicBlock(generator->functions[currScope->funcName].value)));
    LLVMValueRef value = LLVMBuildAlloca(generator->builder, generator->genType(type, -1), name);
    LLVMPositionBuilderAtEnd(generator->builder, generator->currBB);
    return {value, new TypePointer(type)};
}

// Wrapper for the LLVMBuildArrayAlloca function using RaveValue.
RaveValue LLVM::alloc(RaveValue size, const char* name) {
    return {LLVMBuildArrayAlloca(generator->builder, LLVMInt8TypeInContext(generator->context), size.value, name), new TypePointer(new TypeBasic(BasicType::Char))};
}

// Enables/disables fast math.
void LLVM::setFastMath(LLVMBuilderRef builder, bool infs, bool nans, bool arcp, bool nsz) {
    llvm::FastMathFlags flags;
    if(infs) flags.setNoInfs(true);
    if(nans) flags.setNoNaNs(true);
    if(arcp) flags.setAllowReciprocal(true);
    if(nsz) flags.setNoSignedZeros(true);
    llvm::unwrap(builder)->setFastMathFlags(flags);
}

// Enables/disables all flags of fast math.
void LLVM::setFastMathAll(LLVMBuilderRef builder, bool value) {
    llvm::FastMathFlags flags;
    flags.setFast(value);
    llvm::unwrap(builder)->setFastMathFlags(flags);
}

// Wrapper for the LLVMConstInt function.
LLVMValueRef LLVM::makeInt(size_t n, unsigned long long value, bool isUnsigned) {
    return LLVMConstInt(LLVMIntTypeInContext(generator->context, n), value, isUnsigned);
}

// Wrapper for LLVMConstArray function.
RaveValue LLVM::makeCArray(Type* ty, std::vector<RaveValue> values) {
    std::vector<LLVMValueRef> data;
    for(int i=0; i<values.size(); i++) data.push_back(values[i].value);
    return {LLVMConstArray(generator->genType(ty, -1), data.data(), data.size()), new TypeArray(data.size(), ty)};
}