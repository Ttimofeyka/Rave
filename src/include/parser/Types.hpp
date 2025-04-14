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
        Real,
    
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
    bool isUnsigned();
    std::string toString() override;
    Type* getElType() override;
};

extern std::map<char, TypeBasic*> basicTypes;

class TypePointer : public Type {
public:
    Type* instance;

    TypePointer(Type* instance);
    Type* check(Type* parent) override;
    Type* copy() override;
    int getSize() override;
    std::string toString() override;
    Type* getElType() override;
    ~TypePointer() override;
};

class TypeArray : public Type {
public:
    Node* count;
    Type* element;

    TypeArray(Node* count, Type* element);
    Type* check(Type* parent) override;
    Type* copy() override;
    int getSize() override;
    std::string toString() override;
    Type* getElType() override;
    ~TypeArray() override;
};

class TypeAlias : public Type {
public:
    TypeAlias();
    Type* copy() override;
    Type* check(Type* parent) override;
    int getSize() override;
    std::string toString() override;
    Type* getElType() override;
};

class TypeVoid : public Type {
public:
    TypeVoid();
    Type* copy() override;
    Type* check(Type* parent) override;
    int getSize() override;
    std::string toString() override;
    Type* getElType() override;
};

class TypeConst : public Type {
public:
    Type* instance;

    TypeConst(Type* instance);
    Type* copy() override;
    Type* check(Type* parent) override;
    int getSize() override;
    std::string toString() override;
    Type* getElType() override;
    ~TypeConst() override;
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
    bool isSimple();
    int getElCount();
    int getSize() override;
    std::string toString() override;
    Type* getElType() override;
};

class TypeByval : public Type {
public:
    Type* type;
    TypeByval(Type* type);
    Type* copy() override;
    Type* check(Type* parent) override;
    int getSize() override;
    std::string toString() override;
    Type* getElType() override;
};

class TypeTemplateMember : public Type {
public:
    Type* type;
    Node* value;

    TypeTemplateMember(Type* type, Node* value);
    Type* copy() override;
    Type* check(Type* parent) override;
    int getSize() override;
    std::string toString() override;
    Type* getElType() override;
    ~TypeTemplateMember() override;
};

class TypeTemplateMemberDefinition : public Type {
public:
    std::string name;
    Type* type;

    TypeTemplateMemberDefinition(Type* type, std::string name);
    Type* copy() override;
    Type* check(Type* parent) override;
    int getSize() override;
    std::string toString() override;
    Type* getElType() override;
    ~TypeTemplateMemberDefinition() override;
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
    Type* getElType() override;
    ~TypeFuncArg() override;
};

class TypeFunc : public Type {
public:
    Type* main;
    std::vector<TypeFuncArg*> args;
    bool isVarArg;

    TypeFunc(Type* main, std::vector<TypeFuncArg*> args, bool isVarArg);
    Type* copy() override;
    int getSize() override;
    std::string toString() override;
    Type* check(Type* parent) override;
    Type* getElType() override;
    ~TypeFunc() override;
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
    Type* getElType() override;
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
    Type* getElType() override;
};

class TypeAuto : public Type {
public:
    TypeAuto();
    Type* copy() override;
    Type* check(Type* parent) override;
    int getSize() override;
    std::string toString() override;
    Type* getElType() override;
};

class TypeLLVM : public Type {
public:
    LLVMTypeRef tr;
    TypeLLVM(LLVMTypeRef tr);
    Type* copy() override;
    Type* check(Type* parent) override;
    int getSize() override;
    std::string toString() override;
    Type* getElType() override;
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
    Type* getElType() override;
};

class TypeDivided : public Type {
public:
    Type* mainType;
    std::vector<Type*> divided;

    TypeDivided(Type* mainType, std::vector<Type*> divided);
    Type* copy() override;
    Type* check(Type* parent) override;
    int getSize() override;
    std::string toString() override;
    Type* getElType() override;
};

extern Type* getType(std::string id);
extern bool isFloatType(Type* type);
extern bool isBytePointer(Type* type);
extern TypeVoid* typeVoid;

namespace Template {
    extern bool replaceTemplates(Type** type);
}
