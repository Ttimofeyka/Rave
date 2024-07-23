/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <string>
#include <vector>
#include "../../include/parser/ast.hpp"
#include "../../include/parser/nodes/Node.hpp"
#include "../../include/parser/nodes/NodeBlock.hpp"
#include "Type.hpp"

namespace BasicType {
    enum BasicType : char {
        Bool,
        Char,
        Short,
        Int,
        Long,
        Cent,
    
        Half,
        Bhalf,
        Float,
        Double,
    
        Uchar,
        Ushort,
        Uint,
        Ulong,
        Ucent,
    };
}

class TypeBasic : public Type {
public:
    char type;

    TypeBasic(char ty);
    Type* copy() override;
    int getSize() override;
    Type* check(Type* parent) override;
    bool isFloat();
    std::string toString() override;
};

class TypePointer : public Type {
public:
    Type* instance;

    TypePointer(Type* instance);
    Type* check(Type* parent) override;
    Type* copy() override;
    int getSize() override;
    std::string toString() override;
};

class TypeArray : public Type {
public:
    int count;
    Type* element;

    TypeArray(int count, Type* element);
    Type* check(Type* parent) override;
    Type* copy() override;
    int getSize() override;
    std::string toString() override;
};

class TypeAlias : public Type {
public:
    TypeAlias();
    Type* copy() override;
    Type* check(Type* parent) override;
    int getSize() override;
    std::string toString() override;
};

class TypeVoid : public Type {
public:
    TypeVoid();
    Type* copy() override;
    Type* check(Type* parent) override;
    int getSize() override;
    std::string toString() override;
};

class TypeConst : public Type {
public:
    Type* instance;

    TypeConst(Type* instance);
    Type* copy() override;
    Type* check(Type* parent) override;
    int getSize() override;
    std::string toString() override;
};

class TypeStruct : public Type {
public:
    std::string name;
    std::vector<Type*> types;
    
    TypeStruct(std::string name);
    TypeStruct(std::string name, std::vector<Type*> types);
    Type* copy() override;
    Type* check(Type* parent) override;
    void updateByTypes();
    int getSize() override;
    std::string toString() override;
};

class TypeFuncArg : public Type {
public:
    Type* type;
    std::string name;

    TypeFuncArg(Type* type, std::string name);
    Type* copy() override;
    Type* check(Type* parent) override;
    int getSize() override;
    std::string toString() override;
};

class TypeFunc : public Type {
public:
    Type* main;
    std::vector<TypeFuncArg*> args;

    TypeFunc(Type* main, std::vector<TypeFuncArg*> args);
    Type* copy() override;
    int getSize() override;
    std::string toString() override;
    Type* check(Type* parent) override;
};

class TypeBuiltin : public Type {
public:
    std::string name;
    std::vector<Node*> args;
    NodeBlock* block;

    TypeBuiltin(std::string name, std::vector<Node*> args, NodeBlock* block);
    Type* copy() override;
    int getSize() override;
    std::string toString() override;
    Type* check(Type* parent) override;
};

class TypeCall : public Type {
public:
    std::string name;
    std::vector<Node*> args;

    TypeCall(std::string name, std::vector<Node*> args);
    std::string toString() override;
    Type* copy() override;
    int getSize() override;
    Type* check(Type* parent) override;
};

class TypeAuto : public Type {
public:
    TypeAuto();
    Type* copy() override;
    Type* check(Type* parent) override;
    int getSize() override;
    std::string toString() override;
};

class TypeLLVM : public Type {
public:
    LLVMTypeRef tr;
    TypeLLVM(LLVMTypeRef tr);
    Type* copy() override;
    Type* check(Type* parent) override;
    int getSize() override;
    std::string toString() override;
};

class TypeVector : public Type {
public:
    Type* mainType;
    int count;

    TypeVector(Type* mainType, int count);
    Type* copy() override;
    Type* check(Type* parent) override;
    int getSize() override;
    std::string toString() override;
};

Type* getType(std::string id);