/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <vector>
#include <llvm-c/Core.h>

class Type;

struct RaveValue {
    LLVMValueRef value;
    Type* type;
};

namespace LLVM {
    extern LLVMValueRef load(LLVMValueRef value, const char* name);
    extern LLVMValueRef call(LLVMValueRef fn, LLVMValueRef* args, unsigned int argsCount, const char* name);
    extern LLVMValueRef gep(LLVMValueRef ptr, LLVMValueRef* indices, unsigned int indicesCount, const char* name);
    extern LLVMValueRef cInboundsGep(LLVMValueRef ptr, LLVMValueRef* indices, unsigned int indicesCount);
    extern LLVMValueRef structGep(LLVMValueRef ptr, unsigned int idx, const char* name);
    extern LLVMValueRef alloc(LLVMTypeRef type, const char* name);
    extern LLVMValueRef alloc(LLVMValueRef size, const char* name);

    extern RaveValue load(RaveValue value, const char* name, int loc);
    extern RaveValue alloc(Type* type, const char* name);

    extern bool isPointerType(LLVMTypeRef type);
    extern bool isPointer(LLVMValueRef value);
    extern LLVMTypeRef getPointerElType(LLVMValueRef value);

    extern void setFastMath(LLVMBuilderRef builder, bool infs, bool nans, bool arcp, bool nsz);
    extern void setFastMathAll(LLVMBuilderRef builder, bool value);
}