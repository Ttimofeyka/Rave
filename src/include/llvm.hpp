/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <vector>
#include <string>
#include <llvm-c/Core.h>

class Type;

/*
Improved version of old LLVMValueRef.
Includes type of value as replace of LLVMTypeOf.
*/
struct RaveValue {
    LLVMValueRef value;
    Type* type;
};

namespace LLVM {
    extern RaveValue load(RaveValue value, const char* name, int loc);
    extern RaveValue alloc(Type* type, const char* name);
    extern RaveValue alloc(RaveValue size, const char* name);
    extern RaveValue call(RaveValue fn, LLVMValueRef* args, unsigned int argsCount, const char* name, std::vector<int> byVals = {});
    extern RaveValue call(RaveValue fn, std::vector<RaveValue> args, const char* name, std::vector<int> byVals = {});
    extern RaveValue gep(RaveValue ptr, LLVMValueRef* indices, unsigned int indicesCount, const char* name);
    extern RaveValue cInboundsGep(RaveValue ptr, LLVMValueRef* indices, unsigned int indicesCount);
    extern RaveValue structGep(RaveValue ptr, unsigned int idx, const char* name);

    extern void setFastMath(LLVMBuilderRef builder, bool infs, bool nans, bool arcp, bool nsz);
    extern void setFastMathAll(LLVMBuilderRef builder, bool value);

    extern LLVMValueRef makeInt(size_t n, unsigned long long value, bool isUnsigned);
    extern RaveValue makeCArray(Type* ty, std::vector<RaveValue> values);

    extern LLVMBasicBlockRef makeBlock(std::string name, LLVMValueRef function);
    extern LLVMBasicBlockRef makeBlock(std::string name, std::string fName);

    extern bool isLoad(LLVMValueRef operation);
    extern void undoLoad(RaveValue& value);

    extern void makeAsPointer(RaveValue& value);

    extern void cast(RaveValue& value, Type* type, int loc = -1);
    extern void castForExpression(RaveValue& one, RaveValue& two);

    extern RaveValue sum(RaveValue first, RaveValue second);
    extern RaveValue sub(RaveValue first, RaveValue second);
    extern RaveValue mul(RaveValue first, RaveValue second);
    extern RaveValue div(RaveValue first, RaveValue second);
    extern RaveValue compare(RaveValue first, RaveValue second, char op);
}