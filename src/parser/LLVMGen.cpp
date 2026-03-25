/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

// LLVMGen class implementation - Type generation and LLVM utilities
// Split from ast.cpp for better maintainability

#include "../include/parser/ast.hpp"
#include "../include/parser/Types.hpp"
#include "../include/parser/nodes/NodeStruct.hpp"
#include "../include/parser/nodes/NodeInt.hpp"
#include "../include/parser/nodes/NodeRet.hpp"
#include "../include/compiler.hpp"
#include "../include/debug.hpp"
#include <iostream>

LLVMGen::~LLVMGen() {
    if (debugInfo != nullptr) delete debugInfo;
}

LLVMGen::LLVMGen(std::string file, genSettings settings, nlohmann::json options) {
    DEBUG_LOG(Debug::Category::CodeGen, "LLVMGen: Initializing for file " + file);

    this->file = file;
    this->settings = settings;
    this->options = options;
    this->context = LLVMContextCreate();
    this->lModule = LLVMModuleCreateWithNameInContext("rave", this->context);

    if (this->file.find('/') == std::string::npos && this->file.find('\\') == std::string::npos)
        this->file = "./" + this->file;

    // Add SSE2, SSSE3 internal functions
    if (Compiler::features.find("+sse2") != std::string::npos &&
        Compiler::features.find("+ssse3") != std::string::npos) {

        functions["llvm.x86.ssse3.phadd.d.128"] = {LLVMAddFunction(lModule, "llvm.x86.ssse3.phadd.d.128",
            LLVMFunctionType(LLVMVectorType(LLVMInt32TypeInContext(context), 4),
                std::vector<LLVMTypeRef>({LLVMVectorType(LLVMInt32TypeInContext(context), 4),
                    LLVMVectorType(LLVMInt32TypeInContext(context), 4)}).data(), 2, false)),
            new TypeFunc(new TypeVector(basicTypes[BasicType::Int], 4),
                {new TypeFuncArg(new TypeVector(basicTypes[BasicType::Int], 4), "v1"),
                 new TypeFuncArg(new TypeVector(basicTypes[BasicType::Int], 4), "v2")}, false)};

        functions["llvm.x86.ssse3.phadd.sw.128"] = {LLVMAddFunction(lModule, "llvm.x86.ssse3.phadd.sw.128",
            LLVMFunctionType(LLVMVectorType(LLVMInt16TypeInContext(context), 8),
                std::vector<LLVMTypeRef>({LLVMVectorType(LLVMInt16TypeInContext(context), 8),
                    LLVMVectorType(LLVMInt16TypeInContext(context), 8)}).data(), 2, false)),
            new TypeFunc(new TypeVector(basicTypes[BasicType::Short], 8),
                {new TypeFuncArg(new TypeVector(basicTypes[BasicType::Short], 8), "v1"),
                 new TypeFuncArg(new TypeVector(basicTypes[BasicType::Short], 8), "v2")}, false)};
    }

    // Add SSE3 internal functions
    if (Compiler::features.find("+sse3") != std::string::npos) {
        functions["llvm.x86.sse3.hadd.ps"] = {LLVMAddFunction(lModule, "llvm.x86.sse3.hadd.ps",
            LLVMFunctionType(LLVMVectorType(LLVMFloatTypeInContext(context), 4),
                std::vector<LLVMTypeRef>({LLVMVectorType(LLVMFloatTypeInContext(context), 4),
                    LLVMVectorType(LLVMFloatTypeInContext(context), 4)}).data(), 2, false)),
            new TypeFunc(new TypeVector(basicTypes[BasicType::Float], 4),
                {new TypeFuncArg(new TypeVector(basicTypes[BasicType::Float], 4), "v1"),
                 new TypeFuncArg(new TypeVector(basicTypes[BasicType::Float], 4), "v2")}, false)};
    }
}

void LLVMGen::error(std::string msg, int line) {
    std::cout << "\033[0;31mError in \033[1m" + this->file + "\033[22m file at \033[1m" +
        std::to_string(line) + "\033[22m line: " + msg + "\033[0;0m" << std::endl;
    std::exit(1);
}

void LLVMGen::warning(std::string msg, int line) {
    std::cout << "\033[0;33mWarning in \033[1m" + this->file + "\033[22m file at \033[1m" +
        std::to_string(line) + "\033[22m line: " + msg + "\033[0;0m" << std::endl;
}

std::string LLVMGen::mangle(std::string name, bool isFunc, bool isMethod) {
    if (isFunc) {
        if (isMethod) return "_RaveM" + std::to_string(name.size()) + name;
        return "_RaveF" + std::to_string(name.size()) + name;
    }
    return "_RaveG" + name;
}

// Helper: Get true struct type by resolving template replacements
Type* getTrueStructType(TypeStruct* ts) {
    Type* t = ts->copy();
    while (generator->toReplace.find(t->toString()) != generator->toReplace.end())
        t = generator->toReplace[t->toString()];
    return t;
}

// Helper: Get type list from nested pointer/array types
std::vector<Type*> getTrueTypeList(Type* t) {
    std::vector<Type*> buffer;
    while (instanceof<TypePointer>(t) || instanceof<TypeArray>(t)) {
        buffer.push_back(t);
        if (instanceof<TypePointer>(t)) t = ((TypePointer*)t)->instance;
        else t = ((TypeArray*)t)->element;
    }
    buffer.push_back(t);
    return buffer;
}

Type* LLVMGen::setByTypeList(std::vector<Type*> list) {
    std::vector<Type*> buffer = std::vector<Type*>(list);
    if (buffer.size() == 1) {
        if (instanceof<TypeStruct>(buffer[0])) return getTrueStructType((TypeStruct*)list[0]);
        return buffer[0];
    }
    if (instanceof<TypeStruct>(buffer[buffer.size()-1]))
        buffer[buffer.size()-1] = getTrueStructType((TypeStruct*)buffer[buffer.size()-1]);

    for (int i = buffer.size()-1; i > 0; i--) {
        if (instanceof<TypePointer>(buffer[i-1])) ((TypePointer*)buffer[i-1])->instance = buffer[i];
        else if (instanceof<TypeArray>(buffer[i-1])) ((TypeArray*)buffer[i-1])->element = buffer[i];
    }
    return buffer[0];
}

// Type generation methods

LLVMTypeRef LLVMGen::genBasicType(TypeBasic* basicType) {
    switch (basicType->type) {
        case BasicType::Bool: return LLVMInt1TypeInContext(context);
        case BasicType::Char: case BasicType::Uchar: return LLVMInt8TypeInContext(context);
        case BasicType::Short: case BasicType::Ushort: return LLVMInt16TypeInContext(context);
        case BasicType::Half: return LLVMHalfTypeInContext(context);
        case BasicType::Bhalf: return LLVMBFloatTypeInContext(context);
        case BasicType::Int: case BasicType::Uint: return LLVMInt32TypeInContext(context);
        case BasicType::Long: case BasicType::Ulong: return LLVMInt64TypeInContext(context);
        case BasicType::Cent: case BasicType::Ucent: return LLVMInt128TypeInContext(context);
        case BasicType::Float: return LLVMFloatTypeInContext(context);
        case BasicType::Double: return LLVMDoubleTypeInContext(context);
        case BasicType::Real: return LLVMFP128TypeInContext(context);
        default: return nullptr;
    }
}

LLVMTypeRef LLVMGen::genPointerType(Type* instance, int loc) {
    if (instanceof<TypeVoid>(instance)) return LLVMPointerType(LLVMInt8TypeInContext(context), 0);
    if (instanceof<TypeAlias>(instance)) error("cannot generate \033[1malias\033[22m as the part of another type!", loc);
    return LLVMPointerType(genType(instance, loc), 0);
}

LLVMTypeRef LLVMGen::genArrayType(TypeArray* arrayType, int loc) {
    Type* element = arrayType->getElType();
    if (instanceof<TypeAlias>(element)) error("cannot generate \033[1malias\033[22m as the part of another type!", loc);
    return LLVMArrayType(genType(element, loc), ((NodeInt*)arrayType->count->comptime())->value.to_int());
}

Type* LLVMGen::resolveTemplateType(TypeStruct* s) {
    if (s->name.find('<') == std::string::npos) return s;

    TypeStruct* sCopy = (TypeStruct*)s->copy();
    if (!sCopy->types.empty()) {
        for (size_t i = 0; i < sCopy->types.size(); i++) {
            Type* ty = sCopy->types[i];
            while (instanceof<TypeConst>(ty) || instanceof<TypeByval>(ty) ||
                   instanceof<TypeArray>(ty) || instanceof<TypePointer>(ty))
                ty = ty->getElType();

            auto it = toReplace.find(ty->toString());
            if (it != toReplace.end()) sCopy->types[i] = it->second;
        }
        sCopy->updateByTypes();
    }
    return sCopy;
}

LLVMTypeRef LLVMGen::genStructType(TypeStruct* s, int loc) {
    DEBUG_LOG(Debug::Category::TypeGen, "Generating struct type: " + s->name);

    auto cacheIt = structures.find(s->name);
    if (cacheIt != structures.end()) return cacheIt->second;

    auto replaceIt = toReplace.find(s->name);
    if (replaceIt != toReplace.end()) return genType(replaceIt->second, loc);

    if (s->name.find('<') != std::string::npos) {
        TypeStruct* resolvedStruct = (TypeStruct*)resolveTemplateType(s);

        cacheIt = structures.find(resolvedStruct->name);
        if (cacheIt != structures.end()) return cacheIt->second;

        size_t templateStart = resolvedStruct->name.find('<');
        std::string origStruct = resolvedStruct->name.substr(0, templateStart);

        auto structIt = AST::structTable.find(origStruct);
        if (structIt != AST::structTable.end()) {
            return structIt->second->genWithTemplate(
                resolvedStruct->name.substr(templateStart), resolvedStruct->types);
        }
    }

    auto structIt = AST::structTable.find(s->name);
    if (structIt != AST::structTable.end()) {
        structIt->second->check();
        structIt->second->generate();
        return genType(s, loc);
    }

    auto aliasIt = AST::aliasTypes.find(s->name);
    if (aliasIt != AST::aliasTypes.end()) return genType(aliasIt->second, loc);

    error("unknown structure \033[1m" + s->name + "\033[22m!", loc);
    return nullptr;
}

LLVMTypeRef LLVMGen::genFuncType(TypeFunc* tf, int loc) {
    if (instanceof<TypeVoid>(tf->main)) tf->main = basicTypes[BasicType::Char];

    std::vector<LLVMTypeRef> types;
    types.reserve(tf->args.size());
    for (size_t i = 0; i < tf->args.size(); i++)
        types.push_back(genType(tf->args[i]->type, loc));

    return LLVMPointerType(
        LLVMFunctionType(genType(tf->main, loc), types.data(), types.size(), false), 0);
}

LLVMTypeRef LLVMGen::genType(Type* type, int loc) {
    if (!type) return LLVMPointerType(LLVMInt8TypeInContext(context), 0);

    // Check cache first
    std::string typeStr = type->toString();
    auto cacheIt = typeCache.find(typeStr);
    if (cacheIt != typeCache.end()) return cacheIt->second;

    LLVMTypeRef result = nullptr;

    if (instanceof<TypeByval>(type)) result = LLVMPointerType(genType(type->getElType(), loc), 0);
    else if (instanceof<TypeAlias>(type)) return nullptr;
    else if (instanceof<TypeBasic>(type)) result = genBasicType((TypeBasic*)type);
    else if (instanceof<TypePointer>(type)) result = genPointerType(type->getElType(), loc);
    else if (instanceof<TypeArray>(type)) result = genArrayType((TypeArray*)type, loc);
    else if (instanceof<TypeStruct>(type)) result = genStructType((TypeStruct*)type, loc);
    else if (instanceof<TypeVoid>(type)) result = LLVMVoidTypeInContext(context);
    else if (instanceof<TypeFunc>(type)) result = genFuncType((TypeFunc*)type, loc);
    else if (instanceof<TypeFuncArg>(type)) result = genType(((TypeFuncArg*)type)->type, loc);
    else if (instanceof<TypeConst>(type)) {
        Type* instance = type->getElType();
        if (instanceof<TypeAlias>(instance)) error("cannot generate \033[1malias\033[22m as the part of another type!", loc);
        result = genType(instance, loc);
    }
    else if (instanceof<TypeVector>(type))
        result = LLVMVectorType(genType(((TypeVector*)type)->mainType, loc), ((TypeVector*)type)->count);
    else if (instanceof<TypeLLVM>(type)) result = ((TypeLLVM*)type)->tr;
    else { error("undefined type!", loc); return nullptr; }

    if (result) typeCache[typeStr] = result;
    return result;
}

// Utility methods

RaveValue LLVMGen::byIndex(RaveValue value, std::vector<LLVMValueRef> indexes) {
    if (instanceof<TypeArray>(value.type))
        return byIndex(LLVM::gep(value, std::vector<LLVMValueRef>(
            {LLVM::makeInt(pointerSize, 0, true)}).data(), 2, "gep_byIndex"), indexes);

    if (instanceof<TypeArray>(value.type->getElType())) {
        Type* newType = new TypePointer(value.type->getElType()->getElType());
        value.type = newType;
    }

    if (indexes.size() > 1) {
        RaveValue oneGep = LLVM::gep(value, std::vector<LLVMValueRef>({indexes[0]}).data(), 1, "gep2_byIndex");
        return byIndex(oneGep, std::vector<LLVMValueRef>(indexes.begin() + 1, indexes.end()));
    }

    return LLVM::gep(value, indexes.data(), indexes.size(), "gep3_byIndex");
}

void LLVMGen::addAttr(std::string name, LLVMAttributeIndex index, LLVMValueRef ptr, int loc, unsigned long value) {
    int id = LLVMGetEnumAttributeKindForName(name.c_str(), name.size());
    if (id == 0) this->error("unknown attribute \033[1m" + name + "\033[22m!", loc);
    LLVMAddAttributeAtIndex(ptr, index, LLVMCreateEnumAttribute(generator->context, id, value));
}

void LLVMGen::addTypeAttr(std::string name, LLVMAttributeIndex index, LLVMValueRef ptr, int loc, LLVMTypeRef value) {
    LLVMAttributeRef attr = LLVMCreateTypeAttribute(generator->context,
        LLVMGetEnumAttributeKindForName(name.c_str(), name.size()), value);
    if (attr == nullptr) this->error("unknown attribute \033[1m" + name + "\033[22m!", loc);
    LLVMAddAttributeAtIndex(ptr, index, attr);
}

void LLVMGen::addStrAttr(std::string name, LLVMAttributeIndex index, LLVMValueRef ptr, int loc, std::string value) {
    LLVMAttributeRef attr = LLVMCreateStringAttribute(generator->context, name.c_str(), name.size(), value.c_str(), value.size());
    if (attr == nullptr) this->error("unknown attribute \033[1m" + name + "\033[22m!", loc);
    LLVMAddAttributeAtIndex(ptr, index, attr);
}

int LLVMGen::getAlignment(Type* type) {
    if (instanceof<TypeBasic>(type)) {
        switch (((TypeBasic*)type)->type) {
            case BasicType::Bool: case BasicType::Char: case BasicType::Uchar: return 1;
            case BasicType::Short: case BasicType::Ushort: case BasicType::Half: case BasicType::Bhalf: return 2;
            case BasicType::Int: case BasicType::Uint: case BasicType::Float: return 4;
            case BasicType::Long: case BasicType::Ulong: case BasicType::Double: return 8;
            case BasicType::Cent: case BasicType::Ucent: case BasicType::Real: return 16;
            default: return 0;
        }
    }
    else if (instanceof<TypePointer>(type)) return 8;
    else if (instanceof<TypeArray>(type)) return this->getAlignment(((TypeArray*)type)->element);
    else if (instanceof<TypeStruct>(type)) return 8;
    else if (instanceof<TypeFunc>(type)) return this->getAlignment(((TypeFunc*)type)->main);
    else if (instanceof<TypeConst>(type)) return this->getAlignment(((TypeConst*)type)->instance);
    return 0;
}