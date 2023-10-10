/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "../../include/parser/nodes/NodeIndex.hpp"
#include "../../include/parser/nodes/NodeVar.hpp"
#include "../../include/parser/nodes/NodeIden.hpp"
#include "../../include/parser/nodes/NodeCast.hpp"
#include "../../include/parser/nodes/NodeCall.hpp"
#include "../../include/parser/nodes/NodeGet.hpp"
#include "../../include/parser/nodes/NodeUnary.hpp"
#include "../../include/parser/ast.hpp"
#include <vector>
#include <string>
#include "../../include/utils.hpp"
#include "../../include/parser/Types.hpp"

NodeIndex::NodeIndex(Node* element, std::vector<Node*> indexes, long loc) {
    this->element = element;
    this->indexes = indexes;
    this->loc = loc;
}

Type* NodeIndex::getType() {
    Type* type = this->element->getType();
    while(instanceof<TypeConst>(type)) type = ((TypeConst*)type)->instance;
    if(!instanceof<TypePointer>(type) && !instanceof<TypeArray>(type) && !instanceof<TypeConst>(type)) {
        if(instanceof<TypeStruct>(type)) {
            //if(TokType.Rbra.into(StructTable[ts.name].operators)) return StructTable[ts.name].operators[TokType.Rbra][""].type;
        }
        generator->error("Assert into NodeIndex::getType!",this->loc);
        return nullptr;
    }
    if(instanceof<TypePointer>(type)) return ((TypePointer*)type)->instance;
    else if(instanceof<TypeArray>(type)) return ((TypeArray*)type)->element;
    generator->error("Assert into NodeIndex::getType!",this->loc);
    return nullptr;
}

void NodeIndex::check() {
    bool oldCheck = this->isChecked;
    this->isChecked = true;
    if(!oldCheck) this->element->check();
}

Node* NodeIndex::copy() {return new NodeIndex(this->element->copy(), this->indexes, this->loc);}
Node* NodeIndex::comptime() {return this;}

std::vector<LLVMValueRef> NodeIndex::generateIndexes() {
    std::vector<LLVMValueRef> buffer;
    for(int i=0; i<this->indexes.size(); i++) buffer.push_back(this->indexes[i]->generate());
    return buffer;
}

bool NodeIndex::isElementConst(Type* type) {
    Type* currType = type;

    while(instanceof<TypeConst>(currType)) currType = ((TypeConst*)currType)->instance;

    if(instanceof<TypePointer>(currType)) return (instanceof<TypeConst>(((TypePointer*)currType)->instance));
    if(instanceof<TypeArray>(currType)) return (instanceof<TypeConst>(((TypeArray*)currType)->element));

    return false;
}

LLVMValueRef NodeIndex::generate() {
    if(instanceof<NodeIden>(this->element)) {
        NodeIden* id = (NodeIden*)this->element;
        Type* _t = currScope->getVar(id->name, this->loc)->type;
        if(instanceof<TypePointer>(_t)) {
            if(instanceof<TypeConst>(((TypePointer*)_t)->instance)) this->elementIsConst = true;
        }
        else if(instanceof<TypeArray>(_t)) {
            if(instanceof<TypeConst>(((TypeArray*)_t)->element)) this->elementIsConst = true;
        }
        else if(instanceof<TypeConst>(_t)) this->elementIsConst = this->isElementConst(((TypeConst*)_t)->instance);
        LLVMValueRef ptr = currScope->get(id->name, this->loc);
        /*if(Generator.opts.runtimeChecks && LLVMGetTypeKind(LLVMTypeOf(ptr)) == LLVMPointerTypeKind && !FuncTable[currScope.func].isNoChecks) {
            LLVMValueRef isNull = LLVMBuildICmp(
                generator->builder,
                LLVMIntNE,
                LLVMBuildPtrToInt(generator->builder,ptr,LLVMInt32TypeInContext(Generator.Context),toStringz("ptoi_")),
                LLVMBuildPtrToInt(generator->builder,new NodeNull().generate(),LLVMInt32TypeInContext(Generator.Context),toStringz("ptoi_")),
                toStringz("assert(p==null)_")
            );
            if(NeededFunctions["assert"].into(Generator.Functions)) LLVMBuildCall(generator->builder,Generator.Functions[NeededFunctions["assert"]],[isNull,new NodeString("Runtime error in '"~Generator.file~"' file on "~to!string(loc)~" line: attempt to use a null pointer in ptoi!\n",false).generate()].ptr,2,toStringz(""));
        }*/
        if(LLVMGetTypeKind(LLVMTypeOf(ptr)) == LLVMArrayTypeKind && LLVMGetTypeKind(LLVMTypeOf(currScope->getWithoutLoad(id->name, this->loc))) == LLVMArrayTypeKind) {
            LLVMValueRef copyVal = LLVMBuildAlloca(generator->builder, LLVMTypeOf(ptr), "NodeIndex_copyVal_");
            LLVMBuildStore(generator->builder, ptr, copyVal);
            ptr = copyVal;
        }
        else if(LLVMGetTypeKind(LLVMTypeOf(ptr)) != LLVMPointerTypeKind) ptr = currScope->getWithoutLoad(id->name, this->loc);
        std::string structName = "";
        if(LLVMGetTypeKind(LLVMTypeOf(ptr)) == LLVMStructTypeKind) structName = std::string(LLVMGetStructName(LLVMTypeOf(ptr)));
        else if(LLVMGetTypeKind(LLVMTypeOf(ptr)) == LLVMPointerTypeKind) {
            if(LLVMGetTypeKind(LLVMGetElementType(LLVMTypeOf(ptr))) == LLVMStructTypeKind) structName = std::string(LLVMGetStructName(LLVMGetElementType(LLVMTypeOf(ptr))));
            else if(LLVMGetTypeKind(LLVMGetElementType(LLVMTypeOf(ptr))) == LLVMPointerTypeKind) {
                if(LLVMGetTypeKind(LLVMGetElementType(LLVMTypeOf(ptr))) == LLVMStructTypeKind) structName = std::string(LLVMGetStructName(LLVMGetElementType(LLVMGetElementType(LLVMTypeOf(ptr)))));
            }
        }
        /*if(structName != "" && TokType.Rbra.into(StructTable[structName].operators)) {
            NodeFunc _f = StructTable[structName].operators[TokType.Rbra][""];
            if(_f.args[0].type.instanceof!TypeStruct) {
                if(LLVMGetTypeKind(LLVMTypeOf(ptr)) == LLVMPointerTypeKind) ptr = LLVMBuildLoad(generator->builder,ptr,toStringz("load_"));
            }
            return LLVMBuildCall(
                generator->builder,
                Generator.Functions[_f.name],
                [ptr, indexs[0].generate()].ptr,
                2,
                toStringz("call")
            );
        }*/
        LLVMValueRef index = generator->byIndex(ptr, this->generateIndexes());
        if(isMustBePtr) return index;
        return LLVMBuildLoad(generator->builder, index, "NodeIndex_NodeIden_load_");
    }
    if(instanceof<NodeGet>(this->element)) {
        NodeGet* nget = (NodeGet*)this->element;
        nget->isMustBePtr = true;
        LLVMValueRef ptr = nget->generate();
        this->elementIsConst = nget->elementIsConst;
        if(LLVMGetTypeKind(LLVMGetElementType(LLVMTypeOf(ptr))) != LLVMArrayTypeKind) {ptr = LLVMBuildLoad(generator->builder, ptr, "NodeIndex_NodeGet_load");}
        LLVMValueRef index = generator->byIndex(ptr, this->generateIndexes());
        if(isMustBePtr) return index;
        return LLVMBuildLoad(generator->builder,index, "NodeIndex_NodeGet");
    }
    if(instanceof<NodeCall>(this->element)) {
        NodeCall* ncall = (NodeCall*)this->element;
        LLVMValueRef vr = ncall->generate();
        LLVMValueRef index = generator->byIndex(vr, this->generateIndexes());
        if(isMustBePtr) return index;
        return LLVMBuildLoad(generator->builder, index, "NodeIndex_NodeCall_load");
    }
    /*if(NodeDone nd = element.instanceof!NodeDone) {
        LLVMValueRef val = nd.generate();
        LLVMValueRef index = Generator.byIndex(val, this->generateIndexes());
        if(isMustBePtr) return index;
        return LLVMBuildLoad(generator->builder,index,toStringz("load3013_"));
    }*/
    if(instanceof<NodeCast>(this->element)) {
        NodeCast* ncast = (NodeCast*)this->element;
        LLVMValueRef val = ncast->generate();
        LLVMValueRef index = generator->byIndex(val, this->generateIndexes());
        if(isMustBePtr) return index;
        return LLVMBuildLoad(generator->builder, index, "NodeIndex_NodeCast_load");
    }
    if(instanceof<NodeUnary>(this->element)) {
        NodeUnary* nunary = (NodeUnary*)this->element;
        LLVMValueRef val = nunary->generate();
        LLVMValueRef index = generator->byIndex(val, this->generateIndexes());
        return index;
    }
    generator->error("NodeIndex::generate assert!",this->loc);
    return nullptr;
}