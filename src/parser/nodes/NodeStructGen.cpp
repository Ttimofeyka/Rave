/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

// NodeStruct generation logic - LLVM IR generation for structs
// Split from NodeStruct.cpp for better maintainability

#include "../../include/parser/nodes/NodeStruct.hpp"
#include "../../include/parser/nodes/NodeVar.hpp"
#include "../../include/parser/nodes/NodeFunc.hpp"
#include "../../include/parser/nodes/NodeRet.hpp"
#include "../../include/parser/nodes/NodeArray.hpp"
#include "../../include/parser/nodes/NodeString.hpp"
#include "../../include/parser/nodes/NodeBool.hpp"
#include "../../include/parser/ast.hpp"
#include "../../include/debug.hpp"

RaveValue NodeStruct::generate() {
    DEBUG_LOG(Debug::Category::CodeGen, "Generating struct: " + name);

    if (this->templateNames.size() > 0 || this->noCompile) return {};

    NodeArray* conditions = nullptr;

    // Process modifiers
    for (size_t i = 0; i < this->mods.size(); i++) {
        while (AST::aliasTable.find(this->mods[i].name) != AST::aliasTable.end()) {
            if (instanceof<NodeArray>(AST::aliasTable[this->mods[i].name])) {
                this->mods[i].name = ((NodeString*)(((NodeArray*)AST::aliasTable[this->mods[i].name])->values[0]))->value;
                this->mods[i].value = ((NodeString*)(((NodeArray*)AST::aliasTable[this->mods[i].name])->values[1]));
            }
            else this->mods[i].name = ((NodeString*)((NodeArray*)AST::aliasTable[this->mods[i].name]))->value;
        }
        if (this->mods[i].name == "packed") this->isPacked = true;
        else if (this->mods[i].name == "data") this->dataVar = ((NodeString*)this->mods[i].value->comptime())->value;
        else if (this->mods[i].name == "length") this->lengthVar = ((NodeString*)this->mods[i].value->comptime())->value;
        else if (this->mods[i].name == "conditions") conditions = (NodeArray*)this->mods[i].value;
    }

    // Check conditions
    if (conditions != nullptr) for (Node* n : conditions->values) {
        Node* result = n->comptime();
        if (instanceof<NodeBool>(result) && !((NodeBool*)result)->value) {
            generator->error("The conditions were failed when generating the structure \033[1m" + this->name + "\033[22m!", this->loc);
            return {};
        }
    }

    // Create struct type
    generator->structures[this->name] = LLVMStructCreateNamed(generator->context, this->name.c_str());

    std::vector<LLVMTypeRef> params = this->getParameters(this->isTemplated);
    LLVMStructSetBody(generator->structures[this->name], params.data(), params.size(), this->isPacked);

    // Generate constructors
    if (!this->constructors.empty()) {
        for (size_t i = 0; i < this->constructors.size(); i++) {
            if (!this->constructors[i]->isChecked) this->constructors[i]->check();
            this->constructors[i]->generate();
        }
    }

    // Generate destructor
    if (this->destructor == nullptr) {
        // Creating default (empty) destructor
        this->destructor = new NodeFunc(
            "~" + this->origname, std::vector<FuncArgSet>(),
            new NodeBlock({}), false,
            std::vector<DeclarMod>(), this->loc, typeVoid, std::vector<std::string>()
        );
        this->destructor->namespacesNames = std::vector<std::string>(this->namespacesNames);
        this->destructor->isComdat = this->isComdat;

        if (this->constructors.size() > 0 && instanceof<TypePointer>(this->constructors[0]->type)) {
            if (isImported) {
                this->destructor->isExtern = true;
                this->destructor->check();
            }
            else {
                Type* thisType = new TypePointer(this->constructors[0]->type);
                this->destructor->args = {FuncArgSet{.name = "this", .type = thisType, .internalTypes = {thisType}}};
                this->destructor->check();
            }
            this->destructor->generate();
        }
    }
    else {
        if (isImported) {
            this->destructor->isExtern = true;
            this->destructor->check();
        }
        else {
            Type* thisType = new TypePointer(!this->constructors.empty() ? this->constructors[0]->type : new TypeStruct(this->name));
            this->destructor->args = {FuncArgSet{.name = "this", .type = thisType, .internalTypes = {thisType}}};
            this->destructor->check();
        }
        this->destructor->generate();
    }

    // Generate methods
    for (size_t i = 0; i < this->methods.size(); i++) {
        this->methods[i]->check();
        if (!isImported && !this->methods[i]->isTemplate) this->methods[i]->generate();
    }

    return {};
}

LLVMTypeRef NodeStruct::genWithTemplate(std::string sTypes, std::vector<Type*> types) {
    DEBUG_LOG(Debug::Category::Template, "Generating struct with template: " + name + sTypes);

    if (templateNames.size() == 0) return nullptr;

    // Save current state
    std::unordered_map<int32_t, Loop> activeLoops = std::unordered_map<int32_t, Loop>(generator->activeLoops);
    LLVMBuilderRef builder = generator->builder;
    LLVMBasicBlockRef currBB = generator->currBB;
    Scope* _scope = currScope;
    std::unordered_map<std::string, Type*> toReplace = std::unordered_map<std::string, Type*>(generator->toReplace);
    std::unordered_map<std::string, Node*> toReplaceValues = std::unordered_map<std::string, Node*>(generator->toReplaceValues);

    std::string _fn = "<";

    if (types.size() != this->templateNames.size()) {
        generator->error("the count of template types is not equal!", this->loc);
        return nullptr;
    }

    // Setup template type replacements
    for (size_t i = 0; i < types.size(); i++) {
        if (instanceof<TypeStruct>(types[i])) {
            if (AST::structTable.find(((TypeStruct*)types[i])->name) == AST::structTable.end() &&
                !((TypeStruct*)types[i])->types.empty())
                generator->genType(types[i], this->loc);
        }

        if (instanceof<TypeTemplateMember>(types[i])) {
            // Value instead type
            generator->toReplaceValues[templateNames[i]] = ((TypeTemplateMember*)types[i])->value;
            generator->toReplace[templateNames[i] + "@"] = types[i];
            _fn += templateNames[i] + ",";
        }
        else {
            generator->toReplace[templateNames[i]] = types[i];
            _fn += templateNames[i] + ",";
        }
    }

    generator->toReplace[name + _fn.substr(0, _fn.size() - 1) + ">"] = new TypeStruct(name + sTypes);

    // Create and generate the templated struct
    NodeStruct* _struct = new NodeStruct(name + sTypes, this->copyElements(), this->loc, "", {}, this->mods);
    _struct->isTemplated = true;
    _struct->isComdat = true;

    _struct->check();
    _struct->generate();

    // Restore state
    generator->activeLoops = std::unordered_map<int32_t, Loop>(activeLoops);
    generator->builder = builder;
    generator->currBB = currBB;
    currScope = _scope;
    generator->toReplace = std::unordered_map<std::string, Type*>(toReplace);
    generator->toReplaceValues = std::unordered_map<std::string, Node*>(toReplaceValues);

    return generator->structures[_struct->name];
}