/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "../../include/parser/ast.hpp"
#include "../../include/parser/Types.hpp"
#include "../../include/llvm-c/Target.h"
#include "../../include/parser/nodes/NodeBinary.hpp"
#include "../../include/lexer/tokens.hpp"
#include "../../include/parser/nodes/NodeIden.hpp"
#include "../../include/parser/nodes/NodeInt.hpp"
#include "../../include/parser/nodes/NodeFloat.hpp"
#include "../../include/parser/nodes/NodeString.hpp"
#include "../../include/parser/nodes/NodeVar.hpp"
#include "../../include/parser/nodes/NodeNull.hpp"
#include "../../include/parser/nodes/NodeGet.hpp"
#include "../../include/parser/nodes/NodeIndex.hpp"
#include "../../include/parser/nodes/NodeBuiltin.hpp"
#include "../../include/parser/nodes/NodeCast.hpp"
#include "../../include/parser/nodes/NodeBool.hpp"
#include "../../include/parser/nodes/NodeDone.hpp"
#include "../../include/parser/nodes/NodeStruct.hpp"
#include "../../include/parser/nodes/NodeCall.hpp"
#include "../../include/parser/nodes/NodeFunc.hpp"
#include "../../include/parser/nodes/NodeUnary.hpp"
#include <iostream>

LLVMValueRef Binary::castValue(LLVMValueRef from, LLVMTypeRef to, long loc) {
    if(LLVMTypeOf(from) == to) return from;
    LLVMTypeKind kindFrom = LLVMGetTypeKind(LLVMTypeOf(from));
    LLVMTypeKind kindTo = LLVMGetTypeKind(to);
    if(kindFrom == kindTo) {
        switch(kindFrom) {
            case LLVMIntegerTypeKind: return LLVMBuildIntCast(generator->builder, from, to, "castValueItoI");
            case LLVMFloatTypeKind: case LLVMDoubleTypeKind: return LLVMBuildFPCast(generator->builder, from, to, "castValueFtoF");
            case LLVMPointerTypeKind: return LLVMBuildPointerCast(generator->builder, from, to, "castValuePtoP");
            case LLVMArrayTypeKind: generator->error("the ability to use cast to arrays is temporarily impossible!",loc); return nullptr;
            default: return from;
        }
    }
    switch(kindFrom) {
        case LLVMIntegerTypeKind:
            switch(kindTo) {
                case LLVMFloatTypeKind: case LLVMDoubleTypeKind: return LLVMBuildSIToFP(generator->builder, from, to, "castValueItoF");
                case LLVMPointerTypeKind: return LLVMBuildIntToPtr(generator->builder, from, to, "castValueItoP");
                case LLVMArrayTypeKind: generator->error("it is forbidden to cast numbers into arrays!",loc); return nullptr;
                case LLVMStructTypeKind: generator->error("it is forbidden to cast numbers into structures!",loc); return nullptr;
                default: return from;
            } break;
        case LLVMFloatTypeKind: case LLVMDoubleTypeKind:
            switch(kindTo) {
                case LLVMIntegerTypeKind: case LLVMDoubleTypeKind: return LLVMBuildFPToSI(generator->builder, from, to, "castValueFtoI");
                case LLVMPointerTypeKind: generator->error("it is forbidden to cast floating-point numbers into pointers!",loc); return nullptr;
                case LLVMArrayTypeKind: generator->error("it is forbidden to cast numbers into arrays!",loc); return nullptr;
                case LLVMStructTypeKind: generator->error("it is forbidden to cast numbers into structures!",loc); return nullptr;
                default: return from;
            }
        case LLVMPointerTypeKind:
            switch(kindTo) {
                case LLVMIntegerTypeKind: return LLVMBuildPtrToInt(generator->builder, from, to, "castValuePtoI");
                case LLVMFloatTypeKind: case LLVMDoubleTypeKind: generator->error("it is forbidden to cast pointers into floating-point numbers!",loc); return nullptr;
                case LLVMArrayTypeKind: generator->error("it is forbidden to cast pointers into arrays!",loc); return nullptr;
                case LLVMStructTypeKind: generator->error("it is forbidden to cast pointers into structures!",loc); return nullptr;
                default: return from;
            }
        case LLVMArrayTypeKind: generator->error("it is forbidden to casting arrays!",loc); return nullptr;
        case LLVMStructTypeKind: generator->error("it is forbidden to cast structures!",loc); return nullptr;
        default: return from;
    } return from;
}

LLVMValueRef Binary::sum(LLVMValueRef one, LLVMValueRef two, long loc) {
    LLVMValueRef oneCasted = one;
    LLVMValueRef twoCasted = two;
    if(LLVMGetTypeKind(LLVMTypeOf(one)) == LLVMGetTypeKind(LLVMTypeOf(two))) {
        if(LLVMTypeOf(one) != LLVMTypeOf(two)) {
            oneCasted = Binary::castValue(one, LLVMTypeOf(two), loc); twoCasted = two;
        }
    }
    else {oneCasted = Binary::castValue(one, LLVMTypeOf(two), loc); twoCasted = two;}
    if(LLVMGetTypeKind(LLVMTypeOf(oneCasted)) == LLVMIntegerTypeKind) return LLVMBuildAdd(generator->builder, one, two, "sum");
    return LLVMBuildFAdd(generator->builder, one, two, "fsum");
}

LLVMValueRef Binary::sub(LLVMValueRef one, LLVMValueRef two, long loc) {
    LLVMValueRef oneCasted = one;
    LLVMValueRef twoCasted = two;
    if(LLVMGetTypeKind(LLVMTypeOf(one)) == LLVMGetTypeKind(LLVMTypeOf(two))) {
        if(LLVMTypeOf(one) != LLVMTypeOf(two)) {
            oneCasted = Binary::castValue(one, LLVMTypeOf(two), loc); twoCasted = two;
        }
    }
    else {oneCasted = Binary::castValue(one, LLVMTypeOf(two), loc); twoCasted = two;}

    if(LLVMGetTypeKind(LLVMTypeOf(oneCasted)) == LLVMIntegerTypeKind) return LLVMBuildSub(generator->builder, one, two, "sub");
    return LLVMBuildFSub(generator->builder, one, two, "fsub");
}

LLVMValueRef Binary::mul(LLVMValueRef one, LLVMValueRef two, long loc) {
    LLVMValueRef oneCasted = one;
    LLVMValueRef twoCasted = two;
    if(LLVMGetTypeKind(LLVMTypeOf(one)) == LLVMGetTypeKind(LLVMTypeOf(two))) {
        if(LLVMTypeOf(one) != LLVMTypeOf(two)) {
            oneCasted = Binary::castValue(one, LLVMTypeOf(two), loc); twoCasted = two;
        }
    }
    else {oneCasted = Binary::castValue(one, LLVMTypeOf(two), loc); twoCasted = two;}

    if(LLVMGetTypeKind(LLVMTypeOf(oneCasted)) == LLVMIntegerTypeKind) return LLVMBuildMul(generator->builder, one, two, "sum");
    return LLVMBuildFAdd(generator->builder, one, two, "fmul");
}

LLVMValueRef Binary::div(LLVMValueRef one, LLVMValueRef two, long loc) {
    LLVMValueRef oneCasted = one;
    LLVMValueRef twoCasted = two;
    if(LLVMGetTypeKind(LLVMTypeOf(one)) == LLVMGetTypeKind(LLVMTypeOf(two))) {
        if(LLVMTypeOf(one) != LLVMTypeOf(two)) {
            oneCasted = Binary::castValue(one, LLVMTypeOf(two), loc); twoCasted = two;
        }
    }
    else {oneCasted = Binary::castValue(one, LLVMTypeOf(two), loc); twoCasted = two;}

    if(LLVMGetTypeKind(LLVMTypeOf(oneCasted)) == LLVMIntegerTypeKind) return LLVMBuildSDiv(generator->builder, one, two, "div");
    return LLVMBuildFDiv(generator->builder, one, two, "fdiv");
}

LLVMValueRef Binary::compare(LLVMValueRef one, LLVMValueRef two, char op, long loc) {
    LLVMValueRef oneCasted = one;
    LLVMValueRef twoCasted = two;
    if(LLVMGetTypeKind(LLVMTypeOf(one)) == LLVMGetTypeKind(LLVMTypeOf(two))) {
        if(LLVMTypeOf(one) != LLVMTypeOf(two)) {
            oneCasted = Binary::castValue(one, LLVMTypeOf(two), loc); twoCasted = two;
        }
    }
    else {oneCasted = Binary::castValue(one, LLVMTypeOf(two), loc); twoCasted = two;}

    if(LLVMGetTypeKind(LLVMTypeOf(oneCasted)) == LLVMPointerTypeKind) {
        oneCasted = LLVMBuildPtrToInt(generator->builder, oneCasted, LLVMInt64TypeInContext(generator->context), "comparePtoIOne");
        twoCasted = LLVMBuildPtrToInt(generator->builder, twoCasted, LLVMInt64TypeInContext(generator->context), "comparePtoITwo");
    }
    if(LLVMGetTypeKind(LLVMTypeOf(oneCasted)) == LLVMIntegerTypeKind) switch(op) {
        case TokType::Equal: return LLVMBuildICmp(generator->builder, LLVMIntEQ, one, two, "compareIEQ");
        case TokType::Nequal: return LLVMBuildICmp(generator->builder, LLVMIntNE, one, two, "compareINEQ");
        case TokType::More: return LLVMBuildICmp(generator->builder, LLVMIntSGT, one, two, "compareIMR");
        case TokType::Less: return LLVMBuildICmp(generator->builder, LLVMIntSLT, one, two, "compareILS");
        case TokType::MoreEqual: return LLVMBuildICmp(generator->builder, LLVMIntSGE, one, two, "compareIME");
        case TokType::LessEqual: return LLVMBuildICmp(generator->builder, LLVMIntSLE, one, two, "compareILE");
        default: return nullptr;
    }
    switch(op) {
        case TokType::Equal: return LLVMBuildFCmp(generator->builder, LLVMRealOEQ, one, two, "compareIEQ");
        case TokType::Nequal: return LLVMBuildFCmp(generator->builder, LLVMRealONE, one, two, "compareINEQ");
        case TokType::More: return LLVMBuildFCmp(generator->builder, LLVMRealOGT, one, two, "compareIMR");
        case TokType::Less: return LLVMBuildFCmp(generator->builder, LLVMRealOLT, one, two, "compareILS");
        case TokType::MoreEqual: return LLVMBuildFCmp(generator->builder, LLVMRealOGE, one, two, "IcompareME");
        case TokType::LessEqual: return LLVMBuildFCmp(generator->builder, LLVMRealOLE, one, two, "IcompareLE");
        default: return nullptr;
    } return nullptr;
}

NodeBinary::NodeBinary(char op, Node* first, Node* second, long loc, bool isStatic) {
    this->op = op;
    this->first = first;
    this->second = second;
    this->loc = loc;
    this->isStatic = isStatic;
}

std::pair<std::string, std::string> isOperatorOverload(LLVMValueRef first, LLVMValueRef second, char op) {
    LLVMTypeRef type = LLVMTypeOf(first);
    if(LLVMGetTypeKind(type) == LLVMStructTypeKind || (LLVMGetTypeKind(type) == LLVMPointerTypeKind && LLVMGetTypeKind(LLVMGetElementType(type)) == LLVMStructTypeKind)) {
        std::string structName = std::string(LLVMGetStructName(type));
        if(AST::structTable.find(structName) != AST::structTable.end()) {
            //std::cout << "StructName = " << structName << ", op = " << (int)op << std::endl;
            if(AST::structTable[structName]->operators.find(op) != AST::structTable[structName]->operators.end()) {
                std::vector<Type*> types;
                types.push_back(lTypeToType(type));
                types.push_back(lTypeToType(LLVMTypeOf(second)));
                std::string sTypes = typesToString(types);
                return ((AST::structTable[structName]->operators[op].find(sTypes) != AST::structTable[structName]->operators[op].end())
                    ? std::pair<std::string, std::string>(structName, sTypes) : std::pair<std::string, std::string>("", ""));
            }
            else if(op == TokType::Nequal && AST::structTable[structName]->operators.find(TokType::Equal) != AST::structTable[structName]->operators.end()) {
                std::vector<Type*> types;
                types.push_back(lTypeToType(type));
                types.push_back(lTypeToType(LLVMTypeOf(second)));
                std::string sTypes = typesToString(types);
                return ((AST::structTable[structName]->operators[TokType::Equal].find(sTypes) != AST::structTable[structName]->operators[TokType::Equal].end())
                    ? std::pair<std::string, std::string>("!"+structName, sTypes) : std::pair<std::string, std::string>("", ""));
            }
        }
    }
    return std::pair<std::string, std::string>("", "");
}

Node* NodeBinary::copy() {return new NodeBinary(this->op, this->first->copy(), this->second->copy(), this->loc, this->isStatic);}
void NodeBinary::check() {this->isChecked = true;}
Node* NodeBinary::comptime() {
    Node* first = this->first;
    Node* second = this->second;

    while(instanceof<NodeIden>(first) && AST::aliasTable.find(((NodeIden*)first)->name) != AST::aliasTable.end()) first = AST::aliasTable[((NodeIden*)first)->name];
    while(instanceof<NodeIden>(second) && AST::aliasTable.find(((NodeIden*)second)->name) != AST::aliasTable.end()) second = AST::aliasTable[((NodeIden*)second)->name];

    switch(this->op) {
        case TokType::Equal:
            if(instanceof<NodeString>(first) && instanceof<NodeString>(second)) return new NodeBool(((NodeString*)first)->value == ((NodeString*)second)->value);
            return new NodeBool(false);
        case TokType::Nequal:
            if(instanceof<NodeString>(first) && instanceof<NodeString>(second)) return new NodeBool(((NodeString*)first)->value != ((NodeString*)second)->value);
            return new NodeBool(false);
        case TokType::And: return new NodeBool(((NodeBool*)first->comptime())->value && ((NodeBool*)second->comptime())->value);
        case TokType::Or: return new NodeBool(((NodeBool*)first->comptime())->value || ((NodeBool*)second->comptime())->value);
        default: return new NodeBool(false);
    }
}

Type* NodeBinary::getType() {
    switch(this->op) {
        case TokType::Equ: case TokType::PluEqu: case TokType::MinEqu: case TokType::DivEqu: case TokType::MulEqu: return new TypeVoid();
        case TokType::Equal: case TokType::Nequal: case TokType::More: case TokType::Less: case TokType::MoreEqual: case TokType::LessEqual:
        case TokType::And: case TokType::Or: return new TypeBasic(BasicType::Bool);
        default:
            Type* firstType = this->first->getType();
            Type* secondType = this->second->getType();
            if(firstType == nullptr) return secondType;
            if(secondType == nullptr) return firstType;
            return (firstType->getSize() >= secondType->getSize()) ? firstType : secondType;
    } return nullptr;
}

LLVMValueRef NodeBinary::generate() {
    bool isAliasIden = (this->first != nullptr && instanceof<NodeIden>(this->first) && AST::aliasTable.find(((NodeIden*)this->first)->name) != AST::aliasTable.end());
    if(this->op == TokType::PluEqu || this->op == TokType::MinEqu || this->op == TokType::MulEqu || this->op == TokType::DivEqu) {
        LLVMValueRef value;
        NodeBinary* bin;
        switch(this->op) {
            case TokType::PluEqu:
                bin = new NodeBinary(TokType::Equ, this->first->copy(), new NodeBinary(TokType::Plus, this->first->copy(), this->second->copy(), loc), loc);
                bin->check(); value = bin->generate(); delete bin; return value;
            case TokType::MinEqu:
                bin = new NodeBinary(TokType::Equ, this->first->copy(), new NodeBinary(TokType::Minus, this->first->copy(), this->second->copy(), loc), loc);
                bin->check(); value = bin->generate(); delete bin; return value;
            case TokType::MulEqu:
                bin = new NodeBinary(TokType::Equ, this->first->copy(), new NodeBinary(TokType::Multiply, this->first->copy(), this->second->copy(), loc), loc);
                bin->check(); value = bin->generate(); delete bin; return value;
            case TokType::DivEqu:
                bin = new NodeBinary(TokType::Equ, this->first->copy(), new NodeBinary(TokType::Divide, this->first->copy(), this->second->copy(), loc), loc);
                bin->check(); value = bin->generate(); delete bin; return value;
            default: return nullptr;
        }
    }

    if(this->op == TokType::Equ) {
        if(instanceof<NodeIden>(first)) {
            NodeIden* id = ((NodeIden*)first);
            if(currScope->getVar(id->name, this->loc)->isConst && id->name != "this" && currScope->getVar(id->name, this->loc)->isChanged) {
                generator->error("an attempt to change the value of a constant variable!", this->loc);
            }
            if(!currScope->getVar(id->name, this->loc)->isChanged) currScope->hasChanged(id->name);
            if(isAliasIden) {AST::aliasTable[id->name] = this->second->copy(); return nullptr;}
            if(instanceof<TypeStruct>(currScope->getVar(id->name, this->loc)->type)) {
                LLVMValueRef vFirst = this->first->generate();
                LLVMValueRef vSecond = this->second->generate();
                std::pair<std::string, std::string> opOverload = isOperatorOverload(vFirst, vSecond, this->op);
                if(opOverload.first != "") {
                    if(opOverload.first[0] == '!') return (new NodeUnary(this->loc, TokType::Ne, (new NodeCall(
                        this->loc, new NodeIden(AST::structTable[opOverload.first.substr(1)]->operators[this->op][opOverload.second]->name, this->loc),
                        std::vector<Node*>({new NodeDone(vFirst), new NodeDone(vSecond)})))))->generate();
                    return (new NodeCall(
                        this->loc, new NodeIden(AST::structTable[opOverload.first]->operators[this->op][opOverload.second]->name, this->loc),
                        std::vector<Node*>({new NodeDone(vFirst), new NodeDone(vSecond)})))->generate();
                }
                else if(instanceof<TypeBasic>(this->second->getType())) {
                    generator->error("an attempt to change value of the structure as the variable '"+id->name+"' without overloading!", this->loc);
                    return nullptr;
                }
            }
            
            LLVMTypeRef lType = LLVMTypeOf(currScope->get(id->name, this->loc));
            if(instanceof<NodeNull>(this->second)) {((NodeNull*)(this->second))->type = nullptr; ((NodeNull*)this->second)->lType = lType;}

            LLVMValueRef value = this->second->generate();
            if(LLVMGetTypeKind(lType) == LLVMStructTypeKind
            || (LLVMGetTypeKind(lType) == LLVMPointerTypeKind && LLVMGetTypeKind(LLVMGetElementType(lType)) == LLVMStructTypeKind)) {
                std::string structName;
                if(LLVMGetTypeKind(lType) == LLVMPointerTypeKind) structName = std::string(LLVMGetStructName(LLVMGetElementType(lType)));
                else structName = std::string(LLVMGetStructName(lType));
                /*if(TokType.Equ.into(StructTable[structName].operators)) {
                    Type[] types; types ~= llvmTypeToType(LLVMTypeOf(currScope.getWithoutLoad(i.name,loc))); types ~= llvmTypeToType(LLVMTypeOf(val));
                        if(typesToString(types).into(StructTable[structName].operators[TokType.Equ])) {
                        return LLVMBuildCall(
                            Generator.Builder,
                            Generator.Functions[StructTable[structName].operators[TokType.Equ][typesToString(types)].name],
                            [currScope.getWithoutLoad(i.name,loc),val].ptr,
                            2,
                            toStringz(StructTable[structName].operators[TokType.Equ][typesToString(types)].type.instanceof!TypeVoid ? "" : "call")
                        );
                    }
                }*/
            }

            if(LLVMGetTypeKind(LLVMTypeOf(value)) == LLVMGetTypeKind(lType) && LLVMGetTypeKind(lType) == LLVMIntegerTypeKind && LLVMTypeOf(value) != lType) {
                value = LLVMBuildIntCast(generator->builder,value,lType,"binary_intc_");
            }
            else if(LLVMGetTypeKind(lType) == LLVMDoubleTypeKind && LLVMGetTypeKind(LLVMTypeOf(value)) == LLVMFloatTypeKind) {
                value = LLVMBuildFPCast(generator->builder,value,lType,"binary_floatc1_");
            }
            else if(LLVMGetTypeKind(lType) == LLVMFloatTypeKind && LLVMGetTypeKind(LLVMTypeOf(value)) == LLVMDoubleTypeKind) {
                value = LLVMBuildFPCast(generator->builder,value,lType,"binary_floatc2_");
            }
            else if(lType != LLVMTypeOf(value)) {
                if(LLVMGetTypeKind(lType) == LLVMPointerTypeKind && LLVMTypeOf(value) == LLVMPointerType(LLVMInt8TypeInContext(generator->context),0)) {
                    value = LLVMBuildPointerCast(generator->builder, value, lType, "binary_ptrc_");
                }
                else {
                    generator->error(
                        "The variable '"+id->name+"' with type '"+std::string(LLVMPrintTypeToString(lType))+"' is incompatible with value type '"+
                        LLVMPrintTypeToString(LLVMTypeOf(value))+"'!", this->loc
                    );
                    return nullptr;
                }
            }
            return LLVMBuildStore(generator->builder, value, currScope->getWithoutLoad(id->name, this->loc));
        }
        else if(instanceof<NodeGet>(this->first)) {
            NodeGet* nget = (NodeGet*)this->first;
            LLVMValueRef fValue = nget->generate();
            std::string fValueStr = std::string(LLVMPrintTypeToString(LLVMTypeOf(fValue)));
            if(instanceof<NodeNull>(this->second)) {
                ((NodeNull*)this->second)->type = nullptr;
                ((NodeNull*)this->second)->lType = LLVMTypeOf(fValue);
            }
            LLVMValueRef sValue = this->second->generate();
            std::string sValueStr = std::string(LLVMPrintTypeToString(LLVMTypeOf(sValue)));

            if(sValueStr.substr(0,sValueStr.size()-1) == fValueStr) sValue = LLVMBuildLoad(generator->builder, sValue, "NodeBinary_NodeGet_load");

            nget->isMustBePtr = true;
            fValue = nget->generate();

            if(LLVMTypeOf(sValue) == LLVMTypeOf(fValue)) {
                if(instanceof<NodeNull>(this->second)) sValue = LLVMBuildLoad(generator->builder, sValue, "NodeBinary_NodeGet_loadnull");
            }

            if(nget->elementIsConst) {
                generator->error("An attempt to change the constant element!",loc);
                return nullptr;
            }

            if(LLVMGetElementType(LLVMTypeOf(fValue)) != LLVMTypeOf(sValue) && LLVMGetTypeKind(LLVMGetElementType(LLVMTypeOf(fValue))) == LLVMGetTypeKind(LLVMTypeOf(sValue))) {
                if(LLVMGetTypeKind(LLVMTypeOf(sValue)) == LLVMIntegerTypeKind) sValue = LLVMBuildIntCast(generator->builder, sValue, LLVMGetElementType(LLVMTypeOf(fValue)), "NodeBinary_NodeGet_intc");
            }

            return LLVMBuildStore(generator->builder, sValue, fValue);
        }
        else if(instanceof<NodeIndex>(this->first)) {
            NodeIndex* ind = (NodeIndex*)this->first;
            ind->isMustBePtr = true;

            LLVMValueRef ptr = ind->generate();
            if(ind->elementIsConst) generator->error("An attempt to change the constant element!", loc);

            if(instanceof<NodeNull>(this->second)) {
                ((NodeNull*)this->second)->type = nullptr;
                ((NodeNull*)this->second)->lType = LLVMGetElementType(LLVMTypeOf(ptr));
            }
            LLVMValueRef value = this->second->generate();

            if(LLVMTypeOf(ptr) == LLVMPointerType(LLVMPointerType(LLVMTypeOf(value),0),0)) ptr = LLVMBuildLoad(generator->builder, ptr, "NodeBinary_NodeIndex_load");

            if(LLVMGetElementType(LLVMTypeOf(ptr)) != LLVMTypeOf(value) && LLVMGetTypeKind(LLVMGetElementType(LLVMTypeOf(ptr))) == LLVMGetTypeKind(LLVMTypeOf(value))) {
                if(LLVMGetTypeKind(LLVMTypeOf(value)) == LLVMIntegerTypeKind) value = LLVMBuildIntCast(generator->builder, value, LLVMGetElementType(LLVMTypeOf(ptr)), "NodeBinary_NodeIndex_intc");
            }

            return LLVMBuildStore(generator->builder, value, ptr);
        }
        else if(instanceof<NodeDone>(this->first)) return LLVMBuildStore(generator->builder, this->second->generate(), this->first->generate());
        //else if(NodeSlice ns = first.instanceof!NodeSlice) {
        //    return ns.binSet(second);
        //}
    }

    if(this->first->getType() != nullptr && this->second->getType() != nullptr && this->first->getType()->toString() != this->second->getType()->toString()) {
        if(instanceof<NodeNull>(this->first)) ((NodeNull*)this->first)->type = this->second->getType();
        else if(instanceof<NodeNull>(this->second)) ((NodeNull*)this->second)->type = this->first->getType();
    }

    if(this->op == TokType::Rem) {
        if(instanceof<TypeBasic>(this->first->getType())) {
            TypeBasic* type = (TypeBasic*)this->first->getType();
            if(type->type == BasicType::Float || type->type == BasicType::Double) return (new NodeBuiltin("fmodf", {this->first, this->second}, this->loc, nullptr))->generate();
            return (new NodeCast(type, new NodeBuiltin("fmodf", {this->first, this->second}, this->loc, nullptr), this->loc))->generate();
        }
        generator->error("NodeBinary: TokType::Rem assert!", loc);
        return nullptr;
    }

    LLVMValueRef vFirst = this->first->generate();
    std::string vFirstStr = std::string(LLVMPrintTypeToString(LLVMTypeOf(vFirst)));
    LLVMValueRef vSecond = this->second->generate();
    std::string vSecondStr = std::string(LLVMPrintTypeToString(LLVMTypeOf(vSecond)));

    if(vFirstStr.substr(0, vFirstStr.size()-1) == vSecondStr) vFirst = LLVMBuildLoad(generator->builder, vFirst, "NodeBinary_firstload");
    if(vSecondStr.substr(0, vSecondStr.size()-1) == vFirstStr) vSecond = LLVMBuildLoad(generator->builder, vSecond, "NodeBinary_secondload");

    if(LLVMGetTypeKind(LLVMTypeOf(vFirst)) == LLVMGetTypeKind(LLVMTypeOf(vSecond)) && LLVMTypeOf(vFirst) != LLVMTypeOf(vSecond)) {
        if(LLVMGetTypeKind(LLVMTypeOf(vFirst)) == LLVMIntegerTypeKind) vFirst = LLVMBuildIntCast(
            generator->builder,
            vFirst,
            LLVMTypeOf(vSecond),
            "NodeBinary_intc"
        );
    }
    else if(LLVMGetTypeKind(LLVMTypeOf(vFirst)) != LLVMGetTypeKind(LLVMTypeOf(vSecond))) {
        if(LLVMGetTypeKind(LLVMTypeOf(vFirst)) == LLVMFloatTypeKind || LLVMGetTypeKind(LLVMTypeOf(vFirst)) == LLVMDoubleTypeKind) {
            if(LLVMGetTypeKind(LLVMTypeOf(vSecond)) == LLVMFloatTypeKind || LLVMGetTypeKind(LLVMTypeOf(vSecond)) == LLVMDoubleTypeKind
             ||LLVMGetTypeKind(LLVMTypeOf(vSecond)) == LLVMIntegerTypeKind) vSecond = Binary::castValue(vSecond, LLVMTypeOf(vFirst), this->loc);
        }
        else if(LLVMGetTypeKind(LLVMTypeOf(vSecond)) == LLVMFloatTypeKind || LLVMGetTypeKind(LLVMTypeOf(vSecond)) == LLVMDoubleTypeKind) {
            if(LLVMGetTypeKind(LLVMTypeOf(vFirst)) == LLVMFloatTypeKind || LLVMGetTypeKind(LLVMTypeOf(vFirst)) == LLVMDoubleTypeKind
             ||LLVMGetTypeKind(LLVMTypeOf(vFirst)) == LLVMIntegerTypeKind) vFirst = Binary::castValue(vFirst, LLVMTypeOf(vSecond), this->loc);
        }
        else {
            generator->error("value types are incompatible!", loc);
            return nullptr;
        }
    }
    else if(LLVMTypeOf(vFirst) != LLVMTypeOf(vSecond)) {
        if(LLVMABISizeOfType(generator->targetData, LLVMTypeOf(vFirst)) > LLVMABISizeOfType(generator->targetData, LLVMTypeOf(vSecond))) vSecond = Binary::castValue(vSecond, LLVMTypeOf(vFirst), this->loc);
        else vFirst = Binary::castValue(vFirst, LLVMTypeOf(vSecond), this->loc);
    }
    
    switch(this->op) {
        case TokType::Plus: return Binary::sum(vFirst, vSecond, this->loc);
        case TokType::Minus: return Binary::sub(vFirst, vSecond, this->loc);
        case TokType::Multiply: return Binary::mul(vFirst, vSecond, this->loc);
        case TokType::Divide: return Binary::div(vFirst, vSecond, this->loc);
        case TokType::Equal: case TokType::Nequal:
        case TokType::Less: case TokType::More:
        case TokType::LessEqual: case TokType::MoreEqual: return Binary::compare(vFirst, vSecond, this->op, this->loc);
        case TokType::And: return LLVMBuildAnd(generator->builder, vFirst, vSecond, "NodeBinary_and");
        case TokType::Or: return LLVMBuildOr(generator->builder, vFirst, vSecond, "NodeBinary_or");
        case TokType::BitXor: return LLVMBuildXor(generator->builder, vFirst, vSecond, "NodeBinary_xor");
        case TokType::BitLeft: return LLVMBuildShl(generator->builder, vFirst, vSecond, "NodeBinary_and");
        case TokType::BitRight: return LLVMBuildLShr(generator->builder, vFirst, vSecond, "NodeBinary_and");
        default: generator->error("undefined operator "+std::to_string(this->op)+"!", this->loc); return nullptr;
    }
}