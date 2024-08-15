/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "../include/callconv.hpp"
#include "../include/parser/nodes/NodeStruct.hpp"
#include "../include/parser/nodes/NodeVar.hpp"
#include "../include/parser/nodes/NodeCast.hpp"
#include "../include/parser/nodes/NodeIndex.hpp"
#include "../include/parser/nodes/NodeDone.hpp"
#include "../include/parser/nodes/NodeInt.hpp"
#include "../include/llvm.hpp"
#include <iostream>

size_t getCountOfInternalArgs(std::vector<FuncArgSet> args) {
    size_t size = 0;
    for(size_t i=0; i<args.size(); i++) size += args[i].internalTypes.size();
    return size;
}

bool isAllCharType(std::vector<NodeVar*> variables) {
    for(int i=0; i<variables.size(); i++) {
        if(!instanceof<TypeBasic>(variables[i]->getType())) return false;
        if(((TypeBasic*)variables[i]->getType())->type != BasicType::Char && ((TypeBasic*)variables[i]->getType())->type != BasicType::Uchar) return false;
    }
    return true;
}

FuncArgSet normalizeArgCdecl64(FuncArgSet arg, int loc) {
    if(instanceof<TypeStruct>(arg.type)) {
        TypeStruct* tstruct = (TypeStruct*)arg.type;
        generator->genType(tstruct, loc); // Fix for possible template structure
        
        if(tstruct->getSize() >= 192) {arg.type = new TypePointer(arg.type); arg.internalTypes[0] = arg.type;}
        else if(AST::structTable.find(tstruct->toString()) != AST::structTable.end()) {
            NodeStruct* nstruct = AST::structTable[tstruct->toString()];
            std::vector<NodeVar*> variables = nstruct->getVariables();

            if(variables.size() == 1) {
                // Just replace structure with this type
                arg.internalTypes = {variables[0]->getType()};
            }
            else if(variables.size() == 2) {
                if(instanceof<TypeBasic>(variables[0]->getType())) {
                    if(instanceof<TypeBasic>(variables[1]->getType())) {
                        TypeBasic* tbasic1 = (TypeBasic*)variables[0]->getType();
                        TypeBasic* tbasic2 = (TypeBasic*)variables[1]->getType();

                        if(!tbasic1->isFloat()) {
                            if(!tbasic2->isFloat()) {
                                // If both integer type - replace with nearest integer type that bigger than the sum of type sizes
                                int sumOfSizes = tbasic1->getSize() + tbasic2->getSize();
                                if(sumOfSizes <= 16) arg.internalTypes = {new TypeDivided(new TypeBasic(BasicType::Short), {tbasic1, tbasic2})};
                                else if(sumOfSizes <= 32) arg.internalTypes = {new TypeDivided(new TypeBasic(BasicType::Int), {tbasic1, tbasic2})};
                                else if(sumOfSizes <= 64) arg.internalTypes = {new TypeDivided(new TypeBasic(BasicType::Long), {tbasic1, tbasic2})};
                            }
                            else if(tbasic1->getSize() + tbasic2->getSize() <= 64) arg.internalTypes = {new TypeDivided(new TypeBasic(BasicType::Long), {tbasic1, tbasic2})};
                        }
                        else {
                            if(!tbasic2->isFloat()) {
                                if(tbasic1->getSize() + tbasic2->getSize() <= 64) arg.internalTypes = {new TypeDivided(new TypeBasic(BasicType::Long), {tbasic1, tbasic2})};
                            }
                            else arg.internalTypes = {new TypeVector(tbasic1, 2)}; // TODO: add support of different float types (float/double)
                        }
                    }
                }
                // TODO: Add support of TypePointer
            }
            else if(variables.size() == 3) {
                if(instanceof<TypeBasic>(variables[0]->getType())) {
                    if(instanceof<TypeBasic>(variables[1]->getType())) {
                        if(instanceof<TypeBasic>(variables[2]->getType())) {
                            TypeBasic* tbasic1 = (TypeBasic*)variables[0]->getType();
                            TypeBasic* tbasic2 = (TypeBasic*)variables[1]->getType();
                            TypeBasic* tbasic3 = (TypeBasic*)variables[2]->getType();

                            if(!tbasic1->isFloat()) {
                                if(!tbasic2->isFloat() && !tbasic3->isFloat()) {
                                    if(isAllCharType(variables)) arg.internalTypes = {new TypeDivided(new TypeLLVM(LLVMIntTypeInContext(generator->context, 24)), {tbasic1, tbasic2, tbasic3})};
                                    else {
                                        int sumOfSizes = tbasic1->getSize() + tbasic2->getSize() + tbasic3->getSize();
                                        if(sumOfSizes <= 32) arg.internalTypes = {new TypeDivided(new TypeBasic(BasicType::Int), {tbasic1, tbasic2, tbasic3})};
                                        else if(sumOfSizes <= 64) arg.internalTypes = {new TypeDivided(new TypeBasic(BasicType::Long), {tbasic1, tbasic2, tbasic3})};
                                    }
                                }
                                else if(tbasic1->getSize() + tbasic2->getSize() + tbasic3->getSize() <= 64) arg.internalTypes = {new TypeDivided(new TypeBasic(BasicType::Long), {tbasic1, tbasic2, tbasic3})};
                            }
                            else {
                                if(!tbasic2->isFloat() || !tbasic3->isFloat()) {
                                    if(tbasic1->getSize() + tbasic2->getSize() + tbasic3->getSize() <= 64) arg.internalTypes = {new TypeDivided(new TypeBasic(BasicType::Long), {tbasic1, tbasic2, tbasic3})};
                                }
                                else arg.internalTypes = {new TypeVector(tbasic1, 2), tbasic1}; // TODO: add support of different float types (float/double)
                            }
                        }
                    }
                }
                // TODO: Add support of TypePointer
            }
            else if(variables.size() == 4) {
                if(instanceof<TypeBasic>(variables[0]->getType())) {
                    if(instanceof<TypeBasic>(variables[1]->getType())) {
                        if(instanceof<TypeBasic>(variables[2]->getType())) {
                            if(instanceof<TypeBasic>(variables[3]->getType())) {
                                TypeBasic* tbasic1 = (TypeBasic*)variables[0]->getType();
                                TypeBasic* tbasic2 = (TypeBasic*)variables[1]->getType();
                                TypeBasic* tbasic3 = (TypeBasic*)variables[2]->getType();
                                TypeBasic* tbasic4 = (TypeBasic*)variables[3]->getType();

                                if(!tbasic1->isFloat()) {
                                    if(!tbasic2->isFloat() && !tbasic3->isFloat() && !tbasic4->isFloat()) {
                                        if(isAllCharType(variables)) arg.internalTypes = {new TypeDivided(new TypeBasic(BasicType::Int), {tbasic1, tbasic2, tbasic3, tbasic4})};
                                        else {
                                            int sumOfSizes = tbasic1->getSize() + tbasic2->getSize() + tbasic3->getSize();
                                            if(sumOfSizes <= 32) arg.internalTypes = {new TypeDivided(new TypeBasic(BasicType::Int), {tbasic1, tbasic2, tbasic3, tbasic4})};
                                            else if(sumOfSizes <= 64) arg.internalTypes = {new TypeDivided(new TypeBasic(BasicType::Long), {tbasic1, tbasic2, tbasic3, tbasic4})};
                                        }
                                    }
                                    else if(tbasic1->getSize() + tbasic2->getSize() + tbasic3->getSize() <= 64) arg.internalTypes = {new TypeDivided(new TypeBasic(BasicType::Long), {tbasic1, tbasic2, tbasic3})};
                                }
                                // TODO: Add support of float/double
                            }
                        }
                    }
                }
                // TODO: Add support of TypePointer
            }
        }
    }
    return arg;
}

std::vector<FuncArgSet> normalizeArgsCdecl64(std::vector<FuncArgSet> args, int loc) {
    std::vector<FuncArgSet> newArgs;

    for(int i=0; i<args.size(); i++) {
        while(instanceof<TypeConst>(args[i].type)) args[i].type = ((TypeConst*)args[i].type)->instance;
        newArgs.push_back(normalizeArgCdecl64(args[i], loc));
    }

    return newArgs;
}

std::vector<LLVMValueRef> normalizeCallCdecl64(std::vector<FuncArgSet> cdeclArgs, std::vector<LLVMValueRef> callArgs, int loc) {
    std::vector<LLVMValueRef> newCallArgs;

    for(int i=0; i<cdeclArgs.size(); i++) {
        if(instanceof<TypeStruct>(cdeclArgs[i].type)) {
            if(cdeclArgs[i].internalTypes.size() == 1) {
                if(instanceof<TypeDivided>(cdeclArgs[i].internalTypes[0])) {
                    if(LLVMGetTypeKind(LLVMTypeOf(callArgs[i])) == LLVMStructTypeKind) {
                        // Allocate temp buffer
                        LLVMValueRef tempBuffer = LLVM::alloc(LLVMTypeOf(callArgs[i]), "cdecl64_tempBuffer");
                        LLVMBuildStore(generator->builder, callArgs[i], tempBuffer);
                        callArgs[i] = tempBuffer;
                    }
                    TypeDivided* tdivided = (TypeDivided*)cdeclArgs[i].internalTypes[0];
                    newCallArgs.push_back((new NodeIndex(new NodeCast(new TypePointer(tdivided->mainType), new NodeDone(callArgs[i]), loc), {new NodeInt(0)}, loc))->generate());
                }
                else if(instanceof<TypeBasic>(cdeclArgs[i].internalTypes[0]) || instanceof<TypeVector>(cdeclArgs[i].internalTypes[0])) {
                    if(LLVMGetTypeKind(LLVMTypeOf(callArgs[i])) == LLVMStructTypeKind) {
                        // Allocate temp buffer
                        LLVMValueRef tempBuffer = LLVM::alloc(LLVMTypeOf(callArgs[i]), "cdecl64_tempBuffer");
                        LLVMBuildStore(generator->builder, callArgs[i], tempBuffer);
                        callArgs[i] = tempBuffer;
                    }
                    newCallArgs.push_back((new NodeIndex(new NodeCast(new TypePointer(cdeclArgs[i].internalTypes[0]), new NodeDone(callArgs[i]), loc), {new NodeInt(0)}, loc))->generate());
                }
                else newCallArgs.push_back(callArgs[i]);
            }
            else if(cdeclArgs[i].internalTypes.size() == 2) {
                if(instanceof<TypeVector>(cdeclArgs[i].internalTypes[0]) && instanceof<TypeBasic>(cdeclArgs[i].internalTypes[1])) {
                    if(LLVMGetTypeKind(LLVMTypeOf(callArgs[i])) == LLVMStructTypeKind) {
                        // Allocate temp buffer
                        LLVMValueRef tempBuffer = LLVM::alloc(LLVMTypeOf(callArgs[i]), "cdecl64_tempBuffer");
                        LLVMBuildStore(generator->builder, callArgs[i], tempBuffer);
                        callArgs[i] = tempBuffer;
                    }

                    LLVMValueRef anonymStruct = LLVMBuildBitCast(
                        generator->builder, callArgs[i],
                        LLVMPointerType(LLVMStructTypeInContext(generator->context, std::vector<LLVMTypeRef>({
                            generator->genType(cdeclArgs[i].internalTypes[0], loc), generator->genType(cdeclArgs[i].internalTypes[1], loc)
                        }).data(), 2, false), 0),
                        "cdecl64_bitcast"
                    );

                    newCallArgs.push_back(LLVM::load(LLVM::structGep(anonymStruct, 0, "cdecl64_bitcastV"), "cdecl64_load"));
                    newCallArgs.push_back(LLVM::load(LLVM::structGep(anonymStruct, 1, "cdecl64_bitcastF"), "cdecl64_load"));
                }
                else newCallArgs.push_back(callArgs[i]);
            }
            else newCallArgs.push_back(callArgs[i]);
        }
        else newCallArgs.push_back(callArgs[i]);
    }
    
    return newCallArgs;
}