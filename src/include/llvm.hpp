/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <vector>
#include "./llvm-c/Core.h"

namespace LLVM {
    extern LLVMValueRef load(LLVMValueRef value, const char* name);
    extern LLVMValueRef call(LLVMValueRef fn, LLVMValueRef* args, unsigned int argsCount, const char* name);
    extern LLVMValueRef gep(LLVMValueRef ptr, LLVMValueRef* indices, unsigned int indicesCount, const char* name);
    extern LLVMValueRef structGep(LLVMValueRef ptr, unsigned int idx, const char* name);
}