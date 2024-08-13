/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <string>
#include <map>
#include <llvm-c/Core.h>
#include <llvm-c/Initialization.h>
#include <llvm-c/lto.h>
#include <llvm-c/Target.h>
#include "./utils.hpp"
#include "./parser/Type.hpp"
#include "./parser/Types.hpp"
#include "./parser/nodes/Node.hpp"
#include "./json.hpp"
#include <vector>

extern size_t getCountOfInternalArgs(std::vector<FuncArgSet> args);
extern FuncArgSet normalizeArgCdecl64(FuncArgSet arg, int loc);
extern std::vector<LLVMValueRef> normalizeCallCdecl64(std::vector<FuncArgSet> cdeclArgs, std::vector<LLVMValueRef> callArgs, int loc);
extern std::vector<FuncArgSet> normalizeArgsCdecl64(std::vector<FuncArgSet> args, int loc);