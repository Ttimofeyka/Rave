/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <llvm-c/Core.h>
#include "../Type.hpp"
#include "../../llvm.hpp"

struct RetGenStmt {
    LLVMBasicBlockRef where;
    LLVMValueRef value;
};

class Node {
public:
    bool isChecked = false;

    virtual RaveValue generate();
    virtual void check();
    virtual void optimize();
    virtual Node* comptime();
    virtual Type* getType();
    virtual Node* copy();
    virtual ~Node();
};
