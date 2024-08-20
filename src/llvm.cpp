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

bool LLVM::isPointerType(LLVMTypeRef type) {
    return LLVMGetTypeKind(type) == LLVMPointerTypeKind;
}

bool LLVM::isPointer(LLVMValueRef value) {
    if(LLVMIsAGlobalVariable(value) || LLVMIsAAllocaInst(value) || LLVMIsAIntToPtrInst(value) || LLVMIsAGetElementPtrInst(value)) return true;
    return LLVM::isPointerType(LLVMTypeOf(value));
}

LLVMTypeRef LLVM::getPointerElType(LLVMValueRef value) {
    #if RAVE_OPAQUE_POINTERS
    if(LLVMIsAAllocaInst(value)) return LLVMGetAllocatedType(value);
    if(LLVMIsAGlobalValue(value)) return LLVMGlobalGetValueType(value);
    if(LLVMIsAArgument(value)) {
        Type* ty = AST::funcTable[currScope->funcName]->getInternalArgType(value);
        if(instanceof<TypePointer>(ty)) return generator->genType(((TypePointer*)ty)->instance, -1);
        return generator->genType(ty, -1);
    }
    if(LLVMIsConstant(value) && LLVMIsNull(value) && LLVM::isPointerType(LLVMTypeOf(value))) return LLVMInt8TypeInContext(generator->context);
    if(LLVMIsACallInst(value)) {
        std::string fName = LLVMGetValueName(LLVMGetCalledValue(value));
        if(fName.find("_RaveF") != std::string::npos) fName = fName.substr(7);
        while(isdigit(fName[0])) fName = fName.substr(1);

        if(AST::funcTable.find(fName) != AST::funcTable.end()) {
            if(instanceof<TypePointer>(AST::funcTable[fName]->getType())) return generator->genType(((TypePointer*)AST::funcTable[fName]->getType())->instance, -1);
            else if(instanceof<TypeStruct>(AST::funcTable[fName]->getType())) return generator->genType(AST::funcTable[fName]->getType(), -1);
        }
        else {
            for(auto &&kv : AST::funcTable) {
                if(kv.second->linkName == fName) {
                    if(instanceof<TypePointer>(kv.second->getType())) return generator->genType(((TypePointer*)kv.second->getType())->instance, -1);
                    else if(instanceof<TypeStruct>(kv.second->getType())) return generator->genType(kv.second->getType(), -1);
                    break;
                }
            }
            
        }

        return LLVMGetReturnType(LLVMGetCalledFunctionType(value));
    }
    if(LLVMIsALoadInst(value)) return LLVM::getPointerElType(LLVMGetOperand(value, 0));
    if(LLVMIsAIntToPtrInst(value)) return LLVMInt8TypeInContext(generator->context);
    if(LLVMIsAGetElementPtrInst(value)) {
        if(LLVMIsInBounds(value)) {
            LLVMTypeRef elType = LLVM::getPointerElType(LLVMGetOperand(value, 0));
    
            if(LLVMGetTypeKind(elType) == LLVMStructTypeKind) {
                std::string structName = LLVMGetStructName(elType);

                if(AST::structTable.find(structName) != AST::structTable.end()) {
                    std::string __number = LLVMPrintValueToString(LLVMGetOperand(value, 2));
                    int number = std::stoi(__number.substr(4));
                    std::vector<NodeVar*> variables = AST::structTable[structName]->getVariables();
                    return generator->genType(variables[number]->getType(), -1);
                }
            }
        }
        return LLVM::getPointerElType(LLVMGetOperand(value, 0));
    }
    std::cout << LLVMPrintValueToString(value) << std::endl; // For future debug
    return LLVMGetElementType(LLVMTypeOf(value));
    #else
    return LLVMGetElementType(LLVMTypeOf(value));
    #endif
}

LLVMValueRef LLVM::load(LLVMValueRef value, const char* name) {
    #if LLVM_VERSION_MAJOR >= 15
        return LLVMBuildLoad2(generator->builder, LLVM::getPointerElType(value), value, name);
    #else
        return LLVMBuildLoad(generator->builder, value, name);
    #endif
}

LLVMValueRef LLVM::call(LLVMValueRef fn, LLVMValueRef* args, unsigned int argsCount, const char* name) {
    #if LLVM_VERSION_MAJOR >= 15
        #if RAVE_OPAQUE_POINTERS
        return LLVMBuildCall2(generator->builder, LLVMGlobalGetValueType(fn), fn, args, argsCount, name);
        #else
        return LLVMBuildCall2(generator->builder, LLVMGetReturnType(LLVMTypeOf(fn)), fn, args, argsCount, name);
        #endif
    #else
        return LLVMBuildCall(generator->builder, fn, args, argsCount, name);
    #endif
}

LLVMValueRef LLVM::gep(LLVMValueRef ptr, LLVMValueRef* indices, unsigned int indicesCount, const char* name) {
    #if LLVM_VERSION_MAJOR >= 15
        return LLVMBuildGEP2(generator->builder, LLVM::getPointerElType(ptr), ptr, indices, indicesCount, name);
    #else
        return LLVMBuildGEP(generator->builder, ptr, indices, indicesCount, name);
    #endif
}

LLVMValueRef LLVM::inboundsGep(LLVMValueRef ptr, LLVMValueRef* indices, unsigned int indicesCount, const char* name) {
    #if LLVM_VERSION_MAJOR >= 15
        return LLVMBuildInBoundsGEP2(generator->builder, LLVM::getPointerElType(ptr), ptr, indices, indicesCount, name);
    #else
        return LLVMBuildInBoundsGEP(generator->builder, ptr, indices, indicesCount, name);
    #endif
}

LLVMValueRef LLVM::constInboundsGep(LLVMValueRef ptr, LLVMValueRef* indices, unsigned int indicesCount) {
    #if LLVM_VERSION_MAJOR >= 15
        return LLVMConstInBoundsGEP2(LLVM::getPointerElType(ptr), ptr, indices, indicesCount);
    #else
        return LLVMConstInBoundsGEP(ptr, indices, indicesCount);
    #endif
}

LLVMValueRef LLVM::structGep(LLVMValueRef ptr, unsigned int idx, const char* name) {
    #if LLVM_VERSION_MAJOR >= 15
        return LLVMBuildStructGEP2(
            generator->builder, LLVM::getPointerElType(ptr),
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

LLVMValueRef LLVM::alloc(LLVMValueRef size, const char* name) {
    return LLVMBuildArrayAlloca(generator->builder, LLVMInt8TypeInContext(generator->context), size, name);
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