/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "../../include/parser/ast.hpp"
#include <vector>
#include <string>
#include "../../include/parser/nodes/NodeItop.hpp"
#include "../../include/parser/ast.hpp"

NodeItop::NodeItop(Node* value, Type* type, long loc) {
    this->value = value;
    this->type = type;
    this->loc = loc;
}

Type* NodeItop::getType() {return this->type->copy();}
Node* NodeItop::comptime() {return this;}
Node* NodeItop::copy() {return new NodeItop(this->value->copy(), this->type->copy(), this->loc);}

void NodeItop::check() {
    bool oldCheck = this->isChecked;
    this->isChecked = true;
    if(!oldCheck) this->value->check();
}

LLVMValueRef NodeItop::generate() {
    LLVMValueRef _int = this->value->generate();
    /*if(Generator.opts.runtimeChecks && !FuncTable[currScope.func].isNoChecks) {
        LLVMValueRef isNull = LLVMBuildICmp(
            Generator.Builder,
            LLVMIntNE,
            _int,
            LLVMConstInt(LLVMTypeOf(_int),0,false),
            toStringz("assert(number==0)_")
        );
        LLVMBuildCall(Generator.Builder,Generator.Functions[NeededFunctions["assert"]],[isNull,new NodeString("Runtime error in '"~Generator.file~"' file on "~to!string(loc)~" line: an attempt to turn a number into a null pointer in itop!\n",false).generate()].ptr,2,toStringz(""));
    }*/
    return LLVMBuildIntToPtr(
        generator->builder,
        _int,
        generator->genType(this->type,this->loc),
        "itop"
    );
}