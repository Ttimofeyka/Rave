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
#include "./include/parser/nodes/NodeInt.hpp"
#include <iostream>
#include <string>

#include <llvm/Target/TargetMachine.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/Analysis/TargetLibraryInfo.h>
#include <llvm/Transforms/IPO.h>
#include <llvm/IR/LegacyPassManager.h>

// Wrapper for the LLVMBuildLoad2 function using RaveValue.
RaveValue LLVM::load(RaveValue value, const char* name, int loc) {
    return {LLVMBuildLoad2(generator->builder, generator->genType(value.type->getElType(), loc), value.value, name), value.type->getElType()};
}

// Checking for LLVM::load operation
bool LLVM::isLoad(LLVMValueRef operation) {
    return LLVMIsALoadInst(operation);
}

void LLVM::undoLoad(RaveValue& value) {
    LLVMValueRef origArg = LLVMGetArgOperand(value.value, 0);
    value.value = origArg;
    value.type = new TypePointer(value.type);
}

// Wrapper for the LLVMBuildCall2 function using RaveValue.
RaveValue LLVM::call(RaveValue fn, LLVMValueRef* args, unsigned int argsCount, const char* name, std::vector<int> byVals) {
    TypeFunc* tfunc = instanceof<TypePointer>(fn.type) ? (TypeFunc*)fn.type->getElType() : (TypeFunc*)fn.type;

    std::vector<LLVMTypeRef> types;
    for (size_t i=0; i<tfunc->args.size(); i++) types.push_back(generator->genType(tfunc->args[i]->type, -1));

    RaveValue result = {LLVMBuildCall2(generator->builder, LLVMFunctionType(generator->genType(tfunc->main, -1), types.data(), types.size(), tfunc->isVarArg), fn.value, args, argsCount, name), tfunc->main};

    for (size_t i=0; i<byVals.size(); i++) LLVMAddCallSiteAttribute(result.value, byVals[i] + 1, LLVMCreateTypeAttribute(generator->context, LLVMGetEnumAttributeKindForName("byval", 5), types[byVals[i]]));

    return result;
}

// Wrapper for the LLVMBuildCall2 function using RaveValue; accepts a vector of RaveValue instead of pointer to LLVMValueRef.
RaveValue LLVM::call(RaveValue fn, std::vector<RaveValue> args, const char* name, std::vector<int> byVals) {
    TypeFunc* tfunc = instanceof<TypePointer>(fn.type) ? (TypeFunc*)fn.type->getElType() : (TypeFunc*)fn.type;
    std::vector<LLVMValueRef> lArgs(args.size());
    std::transform(args.begin(), args.end(), lArgs.begin(), [](RaveValue& arg) { return arg.value; });

    std::vector<LLVMTypeRef> types(tfunc->args.size());
    std::map<int, LLVMTypeRef> byValStructures;

    for (size_t i=0; i<tfunc->args.size(); i++) {
        Type* argType = tfunc->args[i]->type;

        if (instanceof<TypeStruct>(argType)) {
            TypeStruct* tstruct = static_cast<TypeStruct*>(argType);
            if (AST::structTable.find(tstruct->name) == AST::structTable.end() && generator->toReplace.find(tstruct->name) == generator->toReplace.end())
                types[i] = generator->genType(args[i].type->getElType(), -2);
            else types[i] = generator->genType(argType, -2);
        }
        else types[i] = generator->genType(argType, -2);

        if (std::find(byVals.begin(), byVals.end(), i) != byVals.end()) {
            byValStructures[i] = types[i];
            types[i] = LLVMPointerType(types[i], 0);
        }
    }

    RaveValue result = {LLVMBuildCall2(generator->builder, LLVMFunctionType(generator->genType(tfunc->main, -2), types.data(), types.size(), tfunc->isVarArg), fn.value, lArgs.data(), lArgs.size(), name), tfunc->main->copy()};

    for (int i : byVals) {
        LLVMAddCallSiteAttribute(result.value, i + 1, LLVMCreateTypeAttribute(generator->context, LLVMGetEnumAttributeKindForName("byval", 5), byValStructures[i]));
        LLVMAddCallSiteAttribute(result.value, i + 1, LLVMCreateEnumAttribute(generator->context, LLVMGetEnumAttributeKindForName("align", 5), 8));
    }

    return result;
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
    ), new TypePointer(AST::structTable[ts->name]->variables[idx]->getType())};
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
    return {LLVMBuildArrayAlloca(generator->builder, LLVMInt8TypeInContext(generator->context), size.value, name), new TypePointer(basicTypes[BasicType::Char])};
}

// Enables/disables fast math.
void LLVM::setFastMath(LLVMBuilderRef builder, bool infs, bool nans, bool arcp, bool nsz) {
    llvm::FastMathFlags flags;
    if (infs) flags.setNoInfs(true);
    if (nans) flags.setNoNaNs(true);
    if (arcp) flags.setAllowReciprocal(true);
    if (nsz) flags.setNoSignedZeros(true);
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
    for (size_t i=0; i<values.size(); i++) data.push_back(values[i].value);
    return {LLVMConstArray(generator->genType(ty, -1), data.data(), data.size()), new TypeArray(new NodeInt(data.size()), ty)};
}

// Wrappers for LLVMAppendBasicBlockInContext
LLVMBasicBlockRef LLVM::makeBlock(std::string name, LLVMValueRef function) {
    return LLVMAppendBasicBlockInContext(generator->context, function, name.c_str());
}

LLVMBasicBlockRef LLVM::makeBlock(std::string name, std::string fName) {
    return LLVM::makeBlock(name, generator->functions[fName].value);
}

void LLVM::makeAsPointer(RaveValue& value) {
    if (LLVM::isLoad(value.value)) LLVM::undoLoad(value);
    else {
        // Make a temp variable
        RaveValue temp = LLVM::alloc(value.type, "temp");

        // Copy value to temp
        LLVMBuildStore(generator->builder, value.value, temp.value);

        // Replace value with temp
        value = temp;
    }
}

void LLVM::cast(RaveValue& value, Type* type, int loc) {
    if (instanceof<TypePointer>(value.type) || instanceof<TypeByval>(value.type)) {
        if (instanceof<TypePointer>(type)) value = {LLVMBuildPointerCast(generator->builder, value.value, generator->genType(type, loc), "LLVM_ptopcast"), type};
        else if (instanceof<TypeArray>(type)) {
            if (LLVMIsNull(value.value)) value = {LLVMConstNull(generator->genType(type, loc)), type};
            else generator->error("cannot cast a pointer to an array!", loc);
        }
        else if (instanceof<TypeBasic>(type)) {
            if (((TypeBasic*)type)->isFloat()) {
                if (LLVMIsNull(value.value)) value = {LLVMConstNull(generator->genType(type, loc)), type};
                generator->error("cannot cast a pointer to a float!", loc);
            }

            value = {LLVMBuildPtrToInt(generator->builder, value.value, generator->genType(type, loc), "LLVM_ptoi"), type};
        }
        else if (instanceof<TypeStruct>(type)) {
            if (LLVMIsNull(value.value)) value = {LLVMConstNull(generator->genType(type, loc)), type};
            generator->error("cannot cast a pointer to a struct!", loc);
        }
    }
    else if (instanceof<TypeArray>(value.type)) generator->error("cannot cast an array to any type!", loc);
    else if (instanceof<TypeStruct>(value.type)) generator->error("cannot cast a struct to any type!", loc);
    else if (instanceof<TypeBasic>(value.type)) {
        if (instanceof<TypePointer>(type) || instanceof<TypeByval>(type)) {
            if (((TypeBasic*)value.type)->isFloat()) generator->error("cannot cast a float to a pointer!", loc);
            else value = {LLVMBuildIntToPtr(generator->builder, value.value, generator->genType(type, loc), "LLVM_itop"), type};
        }
        else if (instanceof<TypeArray>(type)) generator->error("cannot cast a basic type to an array!", loc);
        else if (instanceof<TypeBasic>(type)) {
            if (((TypeBasic*)value.type)->isFloat()) {
                if (((TypeBasic*)type)->isFloat()) value = {LLVMBuildFPCast(generator->builder, value.value, generator->genType(type, loc), "LLVM_ftofcast"), type};
                else value = {LLVMBuildFPToSI(generator->builder, value.value, generator->genType(type, loc), "LLVM_ftoicast"), type};
            }
            else if (((TypeBasic*)type)->isFloat()) value = {LLVMBuildSIToFP(generator->builder, value.value, generator->genType(type, loc), "LLVM_itofcast"), type};
            else value = {LLVMBuildIntCast2(generator->builder, value.value, generator->genType(type, loc), !(((TypeBasic*)type)->isUnsigned()), "LLVM_itoicast"), type};
        }
        else if (instanceof<TypeStruct>(type)) generator->error("cannot cast a basic type to a struct!", loc);
    }
    else if (instanceof<TypeVector>(value.type)) {
        if (instanceof<TypeVector>(type)) {
            if (type->getElType()->getSize() > value.type->getElType()->getSize()) {
                if (isFloatType(type) && isFloatType(value.type)) value = {LLVMBuildFPExt(generator->builder, value.value, generator->genType(type, loc), "LLVM_fvtofvcast"), type};
                else value = {LLVMBuildSExt(generator->builder, value.value, generator->genType(type, loc), "LLVM_fvtofvcast"), type};
            }
            else value = {LLVMBuildBitCast(generator->builder, value.value, generator->genType(type, loc), "LLVM_bitcast"), type};
        }
    }
}

void LLVM::castForExpression(RaveValue& first, RaveValue& second) {
    while (instanceof<TypeConst>(first.type)) first.type = first.type->getElType();
    while (instanceof<TypeConst>(second.type)) second.type = second.type->getElType();

    TypeBasic* fType = (TypeBasic*)first.type;
    TypeBasic* sType = (TypeBasic*)second.type;

    if (fType->type != sType->type) {
        if (fType->isFloat()) {
            if (sType->isFloat()) {
                if (fType->getSize() > sType->getSize()) LLVM::cast(second, fType);
                else LLVM::cast(first, sType);
            }
            else LLVM::cast(second, fType);
        }
        else if (sType->isFloat()) LLVM::cast(first, sType);
        else {
            if (fType->getSize() > sType->getSize()) {
                if (!fType->isUnsigned() && sType->isUnsigned()) fType = basicTypes[fType->type + 10];
                LLVM::cast(second, fType);
            }
            else {
                if (fType->isUnsigned() && !sType->isUnsigned()) sType = basicTypes[sType->type + 10];
                LLVM::cast(first, sType);
            }
        }
    }
}

RaveValue LLVM::sum(RaveValue first, RaveValue second) {
    if (isFloatType(first.type)) return {LLVMBuildFAdd(generator->builder, first.value, second.value, "LLVM_fsum"), first.type};
    return {LLVMBuildAdd(generator->builder, first.value, second.value, "LLVM_sum"), first.type};
}

RaveValue LLVM::sub(RaveValue first, RaveValue second) {
    if (isFloatType(first.type)) return {LLVMBuildFSub(generator->builder, first.value, second.value, "LLVM_fsub"), first.type};
    return {LLVMBuildSub(generator->builder, first.value, second.value, "LLVM_sub"), first.type};
}

RaveValue LLVM::mul(RaveValue first, RaveValue second) {
    if (isFloatType(first.type)) return {LLVMBuildFMul(generator->builder, first.value, second.value, "LLVM_fmul"), first.type};
    return {LLVMBuildMul(generator->builder, first.value, second.value, "LLVM_mul"), first.type};
}

RaveValue LLVM::div(RaveValue first, RaveValue second, bool isUnsigned) {
    if (isFloatType(first.type)) return {LLVMBuildFDiv(generator->builder, first.value, second.value, "LLVM_fdiv"), first.type};
    if (isUnsigned) return {LLVMBuildUDiv(generator->builder, first.value, second.value, "LLVM_div"), first.type};
    return {LLVMBuildSDiv(generator->builder, first.value, second.value, "LLVM_div"), first.type};
}

RaveValue LLVM::compare(RaveValue first, RaveValue second, char op) {
    if (instanceof<TypePointer>(first.type) && instanceof<TypePointer>(second.type)) {
        LLVM::cast(first, basicTypes[BasicType::Long]);
        LLVM::cast(second, basicTypes[BasicType::Long]);
    }

    Type* returnType = basicTypes[BasicType::Bool];

    if (instanceof<TypeVector>(first.type) && instanceof<TypeVector>(second.type)) returnType = new TypeVector(basicTypes[BasicType::Bool], ((TypeVector*)(first.type))->count);

    RaveValue value = {nullptr, nullptr};

    if (isFloatType(first.type)) switch(op) {
        case TokType::Equal: value = {LLVMBuildFCmp(generator->builder, LLVMRealOEQ, first.value, second.value, "compareOEQ"), returnType}; break;
        case TokType::Nequal: value = {LLVMBuildFCmp(generator->builder, LLVMRealUNE, first.value, second.value, "compareONE"), returnType}; break;
        case TokType::More: value = {LLVMBuildFCmp(generator->builder, LLVMRealOGT, first.value, second.value, "compareOGT"), returnType}; break;
        case TokType::Less: value = {LLVMBuildFCmp(generator->builder, LLVMRealOLT, first.value, second.value, "compareOLT"), returnType}; break;
        case TokType::MoreEqual: value = {LLVMBuildFCmp(generator->builder, LLVMRealOGE, first.value, second.value, "compareOGE"), returnType}; break;
        case TokType::LessEqual: value = {LLVMBuildFCmp(generator->builder, LLVMRealOLE, first.value, second.value, "compareOLE"), returnType}; break;
    }
    else {
        bool isUnsigned = ((TypeBasic*)first.type->getElType())->isUnsigned();

        switch(op) {
            case TokType::Equal: value = {LLVMBuildICmp(generator->builder, LLVMIntEQ, first.value, second.value, "compareIEQ"), returnType}; break;
            case TokType::Nequal: value = {LLVMBuildICmp(generator->builder, LLVMIntNE, first.value, second.value, "compareINEQ"), returnType}; break;
            case TokType::More: value = {LLVMBuildICmp(generator->builder, isUnsigned ? LLVMIntUGT : LLVMIntSGT, first.value, second.value, "compareIMR"), returnType}; break;
            case TokType::Less: value = {LLVMBuildICmp(generator->builder, isUnsigned ? LLVMIntULT : LLVMIntSLT, first.value, second.value, "compareILS"), returnType}; break;
            case TokType::MoreEqual: value = {LLVMBuildICmp(generator->builder, isUnsigned ? LLVMIntUGE : LLVMIntSGE, first.value, second.value, "compareIME"), returnType}; break;
            case TokType::LessEqual: value = {LLVMBuildICmp(generator->builder, isUnsigned ? LLVMIntULE : LLVMIntSLE, first.value, second.value, "compareILE"), returnType}; break;
        }
    }

    if (instanceof<TypeVector>(first.type) && instanceof<TypeVector>(second.type)) LLVM::cast(value, new TypeVector(first.type->getElType(), ((TypeVector*)(first.type))->count), -1);

    return value;
}

void LLVM::Builder::atEnd(LLVMBasicBlockRef block) {
    generator->currBB = block;
    LLVMPositionBuilderAtEnd(generator->builder, block);
}