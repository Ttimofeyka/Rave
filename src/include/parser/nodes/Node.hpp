#pragma once

#include "../../llvm-c/Core.h"
#include "../Type.hpp"

struct RetGenStmt {
    LLVMBasicBlockRef where;
    LLVMValueRef value;
};

class Node {
public:
    bool isChecked = false;

    virtual LLVMValueRef generate();
    virtual void check();
    virtual Node* comptime();
    virtual Type* getType();
    virtual Node* copy();
};
