/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "../include/parser/ast.hpp"
#include "../include/parser/Types.hpp"
#include "../include/utils.hpp"
#include "../include/parser/nodes/NodeStruct.hpp"
#include <iostream>

// Type

// TypeBasic
TypeBasic::TypeBasic(char ty) {
    this->type = ty;
}
Type* TypeBasic::copy() {
    return new TypeBasic(this->type);
}
int TypeBasic::getSize() {
    switch(this->type) {
        case BasicType::Bool: return 1;
        case BasicType::Char: case BasicType::Uchar: return 8;
        case BasicType::Short: case BasicType::Ushort: return 16;
        case BasicType::Int: case BasicType::Uint: case BasicType::Float: return 32;
        case BasicType::Long: case BasicType::Ulong: case BasicType::Double: return 64;
        case BasicType::Cent: case BasicType::Ucent: return 128;
        default: return 0;
    }
}
bool TypeBasic::isFloat() {return (this->type == BasicType::Float) || (this->type == BasicType::Double);}
std::string TypeBasic::toString() {
    switch(this->type) {
        case BasicType::Bool: return "bool";
        case BasicType::Char: return "char"; case BasicType::Uchar: return "uchar";
        case BasicType::Short: return "short"; case BasicType::Ushort: return "ushort";
        case BasicType::Int: return "int"; case BasicType::Uint: return "uint"; 
        case BasicType::Float: return "float";
        case BasicType::Long: return "long"; case BasicType::Ulong: return "ulong";
        case BasicType::Double: return "double";
        case BasicType::Cent: return "cent"; case BasicType::Ucent: return "cent";
        default: return "BasicUnknown";
    }
}
Type* TypeBasic::check(Type* parent) {return nullptr;}

// TypePointer
TypePointer::TypePointer(Type* instance) {
    this->instance = instance;
}
Type* TypePointer::copy() {
    return new TypePointer(instance->copy());
}
Type* TypePointer::check(Type* parent) {
    if(!instanceof<TypeBasic>(this->instance)) this->instance->check(this);
    return nullptr; 
}
int TypePointer::getSize() {return 8;}
std::string TypePointer::toString() {return instance->toString()+"*";}

// TypeArray
TypeArray::TypeArray(int count, Type* element) {
    this->count = count;
    this->element = element;
}
Type* TypeArray::copy() {
    return new TypeArray(count, element->copy());
}
Type* TypeArray::check(Type* parent) {
    if(!instanceof<TypeBasic>(this->element)) this->element->check(this);
    return nullptr;
}
int TypeArray::getSize() {return this->count * this->element->getSize();}
std::string TypeArray::toString() {return this->element->toString()+"["+std::to_string(this->count)+"]";}

// TypeAlias
TypeAlias::TypeAlias() {}
Type* TypeAlias::copy() {return new TypeAlias();}
std::string TypeAlias::toString() {return "alias";}
Type* TypeAlias::check(Type* parent) {return nullptr;}
int TypeAlias::getSize() {return 0;}

// TypeVoid
TypeVoid::TypeVoid() {}
Type* TypeVoid::check(Type* parent) {return nullptr;}
Type* TypeVoid::copy() {return new TypeVoid();}
int TypeVoid::getSize() {return 0;}
std::string TypeVoid::toString() {return "void";}

// TypeConst
TypeConst::TypeConst(Type* instance) {this->instance = instance;}
Type* TypeConst::copy() {return new TypeConst(this->instance->copy());}
Type* TypeConst::check(Type* parent) {this->instance->check(nullptr); return nullptr;}
int TypeConst::getSize() {return this->instance->getSize();}
std::string TypeConst::toString() {return "const("+this->instance->toString()+")";}

// TypeStruct
TypeStruct::TypeStruct(std::string name) {
    this->name = name;
}
TypeStruct::TypeStruct(std::string name, std::vector<Type*> types) {
    this->name = name;
    this->types = types;
}
Type* TypeStruct::copy() {
    std::vector<Type*> typesCopy;
    for(int i=0; i<this->types.size(); i++) typesCopy.push_back(this->types[i]->copy());
    return new TypeStruct(this->name, typesCopy);
}
void TypeStruct::updateByTypes() {
    this->name = this->name.substr(0, this->name.find('<'))+"<";
    for(int i=0; i<this->types.size(); i++) this->name += this->types[i]->toString()+",";
    this->name = this->name.substr(0, this->name.size()-1)+">";
}
int TypeStruct::getSize() {
    Type* t = this;
    while(generator->toReplace.find(t->toString()) == generator->toReplace.end()) t = generator->toReplace[t->toString()];
    if(instanceof<TypeStruct>(t)) {
        TypeStruct* ts = (TypeStruct*)t;
        if(!ts->types.empty() && AST::structTable.find(ts->name) == AST::structTable.end()) {
            AST::structTable[ts->name]->genWithTemplate(ts->name.substr(ts->name.find('<')), ts->types);
        }
        return AST::structTable[ts->name]->getSize();
    }
    return t->getSize();
}
Type* TypeStruct::check(Type* parent) {
    if(AST::aliasTypes.find(this->name) != AST::aliasTypes.end()) {
        Type* _t = AST::aliasTypes[name];
        while(AST::aliasTypes.find(_t->toString()) != AST::aliasTypes.end()) {
            _t = AST::aliasTypes[_t->toString()];
        }
            
        if(parent == nullptr) return _t;

        if(instanceof<TypePointer>(parent)) {
            TypePointer* tp = (TypePointer*) parent;
            tp->instance = _t;
        }
        else if(instanceof<TypeArray>(parent)) {
            TypeArray* ta = (TypeArray*) parent;
            ta->element = _t;
        }
        else if(instanceof<TypeConst>(parent)) {
            TypeConst* tc = (TypeConst*) parent;
            tc->instance = _t;
        }
    }
    return nullptr;
}
std::string TypeStruct::toString() {return this->name;}

// TypeFuncArg
TypeFuncArg::TypeFuncArg(Type* type, std::string name) {
    this->type = type;
    this->name = name;
}
Type* TypeFuncArg::copy() {return new TypeFuncArg(this->type->copy(), this->name);}
Type* TypeFuncArg::check(Type* parent) {return nullptr;}
std::string TypeFuncArg::toString() {return this->name;}
int TypeFuncArg::getSize() {return this->type->getSize();}

// TypeFunc
TypeFunc::TypeFunc(Type* main, std::vector<TypeFuncArg*> args) {
    this->main = main;
    this->args = args;
}
int TypeFunc::getSize() {return 8;}
Type* TypeFunc::copy() {
    std::vector<TypeFuncArg*> _copied;

    for(int i=0; i<this->args.size(); i++) _copied.push_back((TypeFuncArg*)this->args[i]->copy());
    return new TypeFunc(this->main->copy(),_copied);
}
std::string TypeFunc::toString() {return "NotImplemented";}
Type* TypeFunc::check(Type* parent) {return nullptr;}

// TypeBuiltin
TypeBuiltin::TypeBuiltin(std::string name, std::vector<Node*> args, NodeBlock* block) {
    this->name = name;
    this->args = args;
    this->block = block;
}
int TypeBuiltin::getSize() {return 0;}
Type* TypeBuiltin::copy() {return new TypeBuiltin(this->name, this->args, (NodeBlock*)(this->block->copy()));}
std::string TypeBuiltin::toString() {return this->name;}
Type* TypeBuiltin::check(Type* parent) {return nullptr;}

// TypeCall
TypeCall::TypeCall(std::string name, std::vector<Node*> args) {
    this->name = name;
    this->args = args;
}
std::string TypeCall::toString() {return "FuncCall";}
Type* TypeCall::copy() {return new TypeCall(this->name, this->args);}
int TypeCall::getSize() {return 0;}
Type* TypeCall::check(Type* parent) {return nullptr;}

// TypeAuto
TypeAuto::TypeAuto() {}
Type* TypeAuto::copy() {return new TypeAuto();}
int TypeAuto::getSize() {return 0;}
Type* TypeAuto::check(Type* parent) {return nullptr;}
std::string TypeAuto::toString() {return "";}

// TypeLLVM
TypeLLVM::TypeLLVM(LLVMTypeRef tr) {this->tr = tr;}
Type* TypeLLVM::copy() {return new TypeLLVM(this->tr);}
int TypeLLVM::getSize() {return 0;}
Type* TypeLLVM::check(Type* parent) {return nullptr;}
std::string TypeLLVM::toString() {return "";}

Type* getType(std::string id) {
    if(id == "bool") return new TypeBasic(BasicType::Bool);
    else if(id == "char") return new TypeBasic(BasicType::Char); 
    else if(id == "uchar") return new TypeBasic(BasicType::Uchar);
    else if(id == "short") return new TypeBasic(BasicType::Short);
    else if(id == "ushort") return new TypeBasic(BasicType::Ushort);
    else if(id == "int") return new TypeBasic(BasicType::Int);
    else if(id == "uint") return new TypeBasic(BasicType::Uint);
    else if(id == "long") return new TypeBasic(BasicType::Long);
    else if(id == "ulong") return new TypeBasic(BasicType::Ulong);
    else if(id == "cent") return new TypeBasic(BasicType::Cent);
    else if(id == "ucent") return new TypeBasic(BasicType::Ucent);
    else if(id == "float") return new TypeBasic(BasicType::Float);
    else if(id == "double") return new TypeBasic(BasicType::Double);
    else if(id == "void") return new TypeVoid();
    else if(id == "alias") return new TypeAlias();
    else return new TypeStruct(id);
}