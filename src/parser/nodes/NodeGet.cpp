/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "../../include/parser/nodes/NodeFunc.hpp"
#include "../../include/parser/nodes/NodeVar.hpp"
#include "../../include/parser/nodes/NodeGet.hpp"
#include "../../include/parser/nodes/NodeIden.hpp"
#include "../../include/parser/nodes/NodeIndex.hpp"
#include "../../include/parser/nodes/NodeCall.hpp"
#include "../../include/parser/ast.hpp"
#include <vector>
#include <string>

NodeGet::NodeGet(Node* base, std::string field, bool isMustBePtr, long loc) {
    this->base = base;
    this->field = field;
    this->isMustBePtr = isMustBePtr;
    this->loc = loc;
}

Type* NodeGet::getType() {
    Type* _t = this->base->getType();
    TypeStruct* ts;
    if(!instanceof<TypeStruct>(_t)) {
        if(!instanceof<TypePointer>(_t)) generator->error("Structure '"+_t->toString()+"' doesn't exist! (getType)", loc);
        ts = (TypeStruct*)(((TypePointer*)_t)->instance);
    }
    else ts = (TypeStruct*)_t;

    if(ts == nullptr) generator->error("Type '"+_t->toString()+"' is not a structure!",loc);

    if(generator->toReplace.find(ts->name) != generator->toReplace.end()) {
        while(generator->toReplace.find(ts->name) != generator->toReplace.end()) ts = (TypeStruct*)(generator->toReplace[ts->toString()]);
    }
    if(AST::methodTable.find(std::pair<std::string, std::string>(ts->name, this->field)) != AST::methodTable.end()) {
        return AST::methodTable[std::pair<std::string, std::string>(ts->name, this->field)]->getType();
    }
    if(AST::structsNumbers.find(std::pair<std::string, std::string>(ts->name, this->field)) != AST::structsNumbers.end()) {
        return AST::structsNumbers[std::pair<std::string, std::string>(ts->name, this->field)].var->getType();
    }
    for(auto const& x : generator->toReplace) {
        if(x.first.find('<') != std::string::npos) return generator->toReplace[x.first];
    }

    generator->error("Structure '"+ts->name+"' doesn't contain element '"+field+"'!", loc);
    return nullptr;
}

LLVMValueRef NodeGet::checkStructure(LLVMValueRef ptr) {
    std::string sType = typeToString(LLVMTypeOf(ptr));
    if(sType[sType.size()-1] != '*') {
        LLVMValueRef temp = LLVMBuildAlloca(generator->builder, LLVMTypeOf(ptr), "NodeGet_checkStructure");
        LLVMBuildStore(generator->builder, ptr, temp);
        return temp;
    }
    while(sType.substr(sType.size()-2) == "**") {
        ptr = LLVMBuildLoad(generator->builder, ptr, "NodeGet_checkStructure_load");
        sType = typeToString(LLVMTypeOf(ptr));
    }
    return ptr;
}

LLVMValueRef NodeGet::checkIn(std::string structure) {
    if(AST::structTable.find(structure) == AST::structTable.end()) {
        generator->error("Structure '"+structure+"' doesn't exist!", loc);
        return nullptr;
    }
    auto member = std::pair<std::string, std::string>(structure,this->field);
    if(AST::structsNumbers.find(member) == AST::structsNumbers.end()) {
        if(AST::methodTable.find(member) != AST::methodTable.end()) return generator->functions[AST::methodTable[member]->name];
        generator->error("Structure '"+structure+"' doesn't contain element '"+this->field+"'!", loc);
        return nullptr;
    }
    if(instanceof<TypeConst>(AST::structsNumbers[member].var->type)) elementIsConst = true;
    return nullptr;
}

void NodeGet::checkIsNull(LLVMValueRef ptr) {
    /*if(LLVMGetTypeKind(LLVMTypeOf(ptr)) == LLVMPointerTypeKind && Generator.opts.runtimeChecks && !FuncTable[currScope.func].isNoChecks) {
        LLVMValueRef isNull = LLVMBuildICmp(
            Generator.Builder,
            LLVMIntNE,
            LLVMBuildPtrToInt(Generator.Builder,ptr,LLVMInt32TypeInContext(Generator.Context),toStringz("ptoi_")),
            LLVMBuildPtrToInt(Generator.Builder,new NodeNull().generate(),LLVMInt32TypeInContext(Generator.Context),toStringz("ptoi_")),
            toStringz("assert(p==null)_")
        );
        LLVMBuildCall(Generator.Builder,Generator.Functions[NeededFunctions["assert"]],[isNull,new NodeString("Runtime error in '"~Generator.file~"' file on "~to!string(loc)~" line: attempt to get an element from a null pointer to a structure!\n",false).generate()].ptr,2,toStringz(""));
    }*/
}

LLVMValueRef NodeGet::generate() {
    if(instanceof<NodeIden>(this->base)) {
        LLVMValueRef ptr = checkStructure(currScope->getWithoutLoad(((NodeIden*)this->base)->name,loc));
        std::string structName = std::string(LLVMGetStructName(LLVMGetElementType(LLVMTypeOf(ptr))));
        LLVMValueRef f = this->checkIn(structName);
        if(f != nullptr) return f;
        if(this->isMustBePtr) return LLVMBuildStructGEP(
            generator->builder,
            ptr,
            AST::structsNumbers[std::pair<std::string, std::string>(structName, this->field)].number,
            "NodeGet_generate_Iden_ptr"
        );
        return LLVMBuildLoad(generator->builder, LLVMBuildStructGEP(
            generator->builder,
            ptr,
            AST::structsNumbers[std::pair<std::string, std::string>(structName, this->field)].number,
            "NodeGet_generate_Iden_preload"
        ), "NodeGet_generate_Iden_load");
    }
    if(instanceof<NodeIndex>(this->base) || instanceof<NodeGet>(this->base)) {
        if(instanceof<NodeIndex>(this->base)) ((NodeIndex*)this->base)->isMustBePtr = true;
        else ((NodeGet*)this->base)->isMustBePtr = true;

        LLVMValueRef ptr = checkStructure(this->base->generate());
        std::string structName = std::string(LLVMGetStructName(LLVMGetElementType(LLVMTypeOf(ptr))));
        LLVMValueRef f = checkIn(structName);

        if(f != nullptr) return f;
        if(this->isMustBePtr) return LLVMBuildStructGEP(
            generator->builder,
            ptr,
            AST::structsNumbers[std::pair<std::string, std::string>(structName, this->field)].number,
            "NodeGet_generate_Index_ptr"
        );
        return LLVMBuildLoad(generator->builder, LLVMBuildStructGEP(
            generator->builder,
            ptr,
            AST::structsNumbers[std::pair<std::string, std::string>(structName, this->field)].number,
            "NodeGet_generate_Index_preload"
        ), "NodeGet_generate_Index_load");
    }
    if(instanceof<NodeCall>(this->base)) {
        NodeCall* ncall = (NodeCall*)this->base;
        LLVMValueRef ptr = checkStructure(ncall->generate());
        std::string structName = std::string(LLVMGetStructName(LLVMGetElementType(LLVMTypeOf(ptr))));
        LLVMValueRef f = checkIn(structName);

        if(f != nullptr) return f;
        if(this->isMustBePtr) return LLVMBuildStructGEP(
            generator->builder,
            ptr,
            AST::structsNumbers[std::pair<std::string, std::string>(structName, this->field)].number,
            "NodeGet_generate_Index_ptr"
        );
        return LLVMBuildLoad(generator->builder, LLVMBuildStructGEP(
            generator->builder,
            ptr,
            AST::structsNumbers[std::pair<std::string, std::string>(structName, this->field)].number,
            "NodeGet_generate_Index_preload"
        ), "NodeGet_generate_Index_load");
    }
    /*if(NodeBuiltin nb = base.instanceof!NodeBuiltin) {
        LLVMValueRef v = nb.generate();
        string structName;
        if(v is null) {
            Generator.error(loc, "The built-in function '"~nb.name~"' does not return values!");
        }
        LLVMValueRef ptr = checkStructure(v);
        structName = cast(string)fromStringz(LLVMGetStructName(LLVMGetElementType(LLVMTypeOf(ptr))));
        LLVMValueRef f = checkIn(structName);
        if(f !is null) return f;
        if(isMustBePtr) return LLVMBuildStructGEP(
            Generator.Builder, ptr, cast(uint)structsNumbers[cast(immutable)[structName,field]].number, toStringz("sgep1760_")
        );
        return LLVMBuildLoad(Generator.Builder, LLVMBuildStructGEP(
            Generator.Builder, ptr, cast(uint)structsNumbers[cast(immutable)[structName,field]].number, toStringz("sgep1760_")
        ), toStringz("load1760_"));
    }
    if(NodeDone nd = base.instanceof!NodeDone) {
        LLVMValueRef done = nd.generate();
        LLVMValueRef _nakedDone = done;
        while(LLVMGetTypeKind(LLVMTypeOf(_nakedDone)) == LLVMPointerTypeKind) _nakedDone = LLVMBuildLoad(Generator.Builder, _nakedDone, toStringz("_nakedDone_"));
        string structName = cast(string)fromStringz(LLVMGetStructName(LLVMTypeOf(_nakedDone)));
        LLVMValueRef ptr = checkStructure(done);
        LLVMValueRef f = checkIn(structName);
        if(f !is null) return f;
        if(isMustBePtr) return LLVMBuildStructGEP(
            Generator.Builder, ptr, cast(uint)structsNumbers[cast(immutable)[structName,field]].number, toStringz("sgep1760_")
        );
        return LLVMBuildLoad(Generator.Builder, LLVMBuildStructGEP(
            Generator.Builder, ptr, cast(uint)structsNumbers[cast(immutable)[structName,field]].number, toStringz("sgep1760_")
        ), toStringz("load1780_"));
    }*/
    generator->error("Assert into NodeGet ("+std::string(typeid(this->base[0]).name())+")", this->loc);
    return nullptr;
}

void NodeGet::check() {this->isChecked = true;}
Node* NodeGet::copy() {return new NodeGet(this->base->copy(), this->field, this->isMustBePtr, this->loc);}
Node* NodeGet::comptime() {return this;}