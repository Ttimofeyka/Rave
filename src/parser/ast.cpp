/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

// AST globals and DebugGen class implementation
// Split for better maintainability - see also LLVMGen.cpp and Scope.cpp

#include "../include/parser/ast.hpp"
#include "../include/parser/Types.hpp"
#include "../include/parser/nodes/NodeInt.hpp"
#include "../include/debug.hpp"
#include <iostream>
#include <cstring>

#if LLVM_VERSION > 18
#define LLVMDIBuilderInsertDeclareAtEnd LLVMDIBuilderInsertDeclareRecordAtEnd
#endif

// AST namespace globals
std::map<std::string, Type*> AST::aliasTypes;
std::map<std::string, Node*> AST::aliasTable;
std::map<std::string, NodeVar*> AST::varTable;
std::map<std::string, NodeFunc*> AST::funcTable;
std::map<std::string, std::vector<NodeFunc*>> AST::funcVersionsTable;
std::map<std::string, NodeLambda*> AST::lambdaTable;
std::map<std::string, NodeStruct*> AST::structTable;
std::map<std::pair<std::string, std::string>, NodeFunc*> AST::methodTable;
std::map<std::pair<std::string, std::string>, StructMember> AST::structMembersTable;
std::vector<std::string> AST::importedFiles;
std::map<std::string, std::vector<Node*>> AST::parsed;
std::string AST::mainFile;
std::string AST::currentFile;
std::vector<std::string> AST::addToImport;
bool AST::debugMode;

// Global generator and scope pointers
LLVMGen* generator = nullptr;
DebugGen* debugInfo;
Scope* currScope;
LLVMTargetDataRef dataLayout;

void AST::checkError(std::string message, int line) {
    std::string lineStr = (line == -1) ? "unknown line" : std::to_string(line);
    std::cout << "\033[0;31mError in \033[1m" + AST::mainFile + "\033[22m file at \033[1m" +
        lineStr + "\033[22m: " + message + "\033[0;0m" << std::endl;
    std::exit(1);
}

// DebugGen implementation - Debug info generation for DWARF

DebugGen::DebugGen(genSettings settings, std::string file, LLVMModuleRef module) {
    DEBUG_LOG(Debug::Category::CodeGen, "DebugGen: Creating debug info for " + file);

    this->diBuilder = LLVMCreateDIBuilder(module);
    this->diFile = LLVMDIBuilderCreateFile(diBuilder, file.c_str(), file.length(), "", 0);

    this->diScope = LLVMDIBuilderCreateCompileUnit(
        diBuilder, LLVMDWARFSourceLanguageC,
        LLVMDIBuilderCreateFile(diBuilder, file.c_str(), file.length(), "", 0),
        "rave", 4, settings.optLevel > 0, nullptr, 0, 1, "", 0,
        LLVMDWARFEmissionFull, 0, true, true, "", 0, "", 0
    );
}

DebugGen::~DebugGen() {
    LLVMDIBuilderFinalize(diBuilder);
}

void DebugGen::pushScope(LLVMMetadataRef scope) {
    scopeStack.push_back(scope);
}

void DebugGen::popScope() {
    if (!scopeStack.empty()) scopeStack.pop_back();
}

LLVMMetadataRef DebugGen::currentScope() {
    if (scopeStack.empty()) return diScope;
    return scopeStack.back();
}

void DebugGen::setInstrLoc(int line) {
    if (!generator->settings.outDebugInfo) return;
    if (scopeStack.empty()) return;

    LLVMValueRef lastInstr = LLVMGetLastInstruction(generator->currBB);
    if (lastInstr == nullptr) return;

    LLVMMetadataRef loc = LLVMDIBuilderCreateDebugLocation(
        generator->context, line, 0, currentScope(), nullptr);
    LLVMInstructionSetDebugLoc(lastInstr, loc);
}

void DebugGen::genGlobalVariable(const char* name, LLVMMetadataRef type, LLVMValueRef variable, int line) {
    if (!generator->settings.outDebugInfo) return;

    LLVMMetadataRef expr = LLVMDIBuilderCreateExpression(diBuilder, nullptr, 0);
    LLVMMetadataRef gv = LLVMDIBuilderCreateGlobalVariableExpression(
        diBuilder, diScope, name, strlen(name), nullptr, 0, diFile, line, type,
        0, expr, nullptr, 0);
}

LLVMMetadataRef DebugGen::genBasicType(TypeBasic* type, int loc) {
    switch (type->type) {
        case BasicType::Bool: return LLVMDIBuilderCreateBasicType(diBuilder, "bool", 4, 1, 0, LLVMDIFlagZero);
        case BasicType::Char: return LLVMDIBuilderCreateBasicType(diBuilder, "char", 4, 8, 0, LLVMDIFlagZero);
        case BasicType::Uchar: return LLVMDIBuilderCreateBasicType(diBuilder, "uchar", 5, 8, 0, LLVMDIFlagZero);
        case BasicType::Short: return LLVMDIBuilderCreateBasicType(diBuilder, "short", 5, 16, 0, LLVMDIFlagZero);
        case BasicType::Ushort: return LLVMDIBuilderCreateBasicType(diBuilder, "ushort", 6, 16, 0, LLVMDIFlagZero);
        case BasicType::Int: return LLVMDIBuilderCreateBasicType(diBuilder, "int", 3, 32, 0, LLVMDIFlagZero);
        case BasicType::Uint: return LLVMDIBuilderCreateBasicType(diBuilder, "uint", 4, 32, 0, LLVMDIFlagZero);
        case BasicType::Long: return LLVMDIBuilderCreateBasicType(diBuilder, "long", 4, 64, 0, LLVMDIFlagZero);
        case BasicType::Ulong: return LLVMDIBuilderCreateBasicType(diBuilder, "ulong", 5, 64, 0, LLVMDIFlagZero);
        case BasicType::Cent: return LLVMDIBuilderCreateBasicType(diBuilder, "cent", 4, 128, 0, LLVMDIFlagZero);
        case BasicType::Ucent: return LLVMDIBuilderCreateBasicType(diBuilder, "ucent", 5, 128, 0, LLVMDIFlagZero);
        case BasicType::Half: return LLVMDIBuilderCreateBasicType(diBuilder, "half", 4, 16, 0, LLVMDIFlagZero);
        case BasicType::Bhalf: return LLVMDIBuilderCreateBasicType(diBuilder, "bhalf", 5, 16, 0, LLVMDIFlagZero);
        case BasicType::Float: return LLVMDIBuilderCreateBasicType(diBuilder, "float", 5, 32, 0, LLVMDIFlagZero);
        case BasicType::Double: return LLVMDIBuilderCreateBasicType(diBuilder, "double", 6, 64, 0, LLVMDIFlagZero);
        case BasicType::Real: return LLVMDIBuilderCreateBasicType(diBuilder, "real", 4, 128, 0, LLVMDIFlagZero);
        default: return nullptr;
    }
}

LLVMMetadataRef DebugGen::genPointer(std::string previousName, LLVMMetadataRef type, int loc) {
    return LLVMDIBuilderCreatePointerType(diBuilder, type, pointerSize, 0, 0,
        (previousName + "*").c_str(), previousName.length() + 1);
}

LLVMMetadataRef DebugGen::genArray(int size, LLVMMetadataRef type, int loc) {
    return LLVMDIBuilderCreateArrayType(diBuilder, size, 0, type, nullptr, 0);
}

LLVMMetadataRef DebugGen::genStruct(TypeStruct* type, int loc) {
    return LLVMDIBuilderCreateStructType(diBuilder, diScope, type->toString().c_str(),
        type->toString().length(), diFile, loc, type->getSize(), 0, LLVMDIFlagZero,
        nullptr, nullptr, 0, 0, nullptr, nullptr, 0);
}

LLVMMetadataRef DebugGen::genType(Type* type, int loc) {
    DEBUG_LOG(Debug::Category::TypeGen, "DebugGen::genType: " + (type ? type->toString() : "null"));

    if (type == nullptr) return genPointer("char", genBasicType(basicTypes[BasicType::Char], loc), loc);

    if (instanceof<TypeBasic>(type)) return genBasicType((TypeBasic*)type, loc);
    if (instanceof<TypePointer>(type)) return genPointer(type->toString(), genType(type->getElType(), loc), loc);
    if (instanceof<TypeConst>(type)) return genType(type->getElType(), loc);
    if (instanceof<TypeVector>(type)) {
        LLVMMetadataRef subrange = LLVMDIBuilderGetOrCreateSubrange(diBuilder,
            ((TypeVector*)type)->count, ((TypeVector*)type)->count);
        return LLVMDIBuilderCreateVectorType(diBuilder, type->getSize(), 0,
            genType(type->getElType(), loc), &subrange, 1);
    }

    if (instanceof<TypeArray>(type)) {
        NodeInt* size = (NodeInt*)(((TypeArray*)type)->count->comptime());
        return genArray(size->value.to_int(), genType(type->getElType(), loc), loc);
    }

    if (instanceof<TypeStruct>(type)) {
        Types::replaceTemplates(&type);
        return genStruct((TypeStruct*)type, loc);
    }

    if (instanceof<TypeVoid>(type)) return LLVMDIBuilderCreateBasicType(diBuilder, "void", 4, 0, 0, LLVMDIFlagZero);

    if (instanceof<TypeFunc>(type)) {
        TypeFunc* tf = (TypeFunc*)type;

        std::vector<LLVMMetadataRef> types;
        types.push_back(genType(tf->main, loc));
        for (size_t i = 0; i < tf->args.size(); i++)
            types.push_back(genType(tf->args[i]->type, loc));

        LLVMMetadataRef funcType = LLVMDIBuilderCreateSubroutineType(diBuilder,
            diFile, types.data(), types.size(), LLVMDIFlagZero);
        return genPointer(type->toString(), funcType, loc);
    }

    generator->error("TODO: Add more allowed debug types! Input type: \033[1m" +
        (std::string(typeid(*type).name())) + "\033[22m!", loc);
    return nullptr;
}