/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

// NodeStruct class - Struct definition and checking
// Split for better maintainability - see also NodeStructGen.cpp for generation

#include "../../include/parser/nodes/NodeStruct.hpp"
#include "../../include/parser/nodes/NodeVar.hpp"
#include "../../include/parser/nodes/NodeFunc.hpp"
#include "../../include/parser/nodes/NodeRet.hpp"
#include "../../include/parser/nodes/NodeNone.hpp"
#include "../../include/parser/ast.hpp"
#include "../../include/parser/FuncRegistry.hpp"
#include "../../include/debug.hpp"

NodeStruct::NodeStruct(std::string name, std::vector<Node*> elements, int loc, std::string extends,
                       std::vector<std::string> templateNames, std::vector<DeclarMod> mods) {
    this->name = name;
    this->origname = name;
    this->oldElements = elements;
    this->elements = elements;
    this->loc = loc;
    this->extends = extends;
    this->templateNames = templateNames;
    this->namespacesNames = std::vector<std::string>();
    this->mods = mods;
    this->destructor = nullptr;
    this->dataVar = "";
    this->lengthVar = "";

    for (size_t i = 0; i < elements.size(); i++) {
        if (instanceof<NodeVar>(elements[i])) variables.push_back((NodeVar*)elements[i]);
    }
}

NodeStruct::~NodeStruct() {
    for (size_t i = 0; i < elements.size(); i++) if (elements[i] != nullptr) delete elements[i];
    for (size_t i = 0; i < oldElements.size(); i++) if (oldElements[i] != nullptr) delete oldElements[i];
    if (destructor != nullptr) delete destructor;
}

Node* NodeStruct::comptime() { return this; }

Node* NodeStruct::copy() {
    std::vector<Node*> cElements;
    for (size_t i = 0; i < this->elements.size(); i++) cElements.push_back(this->elements[i]->copy());
    return new NodeStruct(this->origname, cElements, this->loc, this->extends, this->templateNames, this->mods);
}

Type* NodeStruct::getType() { return typeVoid; }

LLVMTypeRef NodeStruct::asConstType() {
    std::vector<LLVMTypeRef> types = this->getParameters(false);
    return LLVMStructTypeInContext(generator->context, types.data(), types.size(), this->isPacked);
}

// Helper: check and resolve templated types
Type* checkForTemplated(Type* type) {
    Type* loaded = type;
    Type* parent = nullptr;

    while (instanceof<TypeConst>(loaded) || instanceof<TypePointer>(loaded) || instanceof<TypeArray>(loaded)) {
        parent = loaded;
        loaded = loaded->getElType();
    }

    if (instanceof<TypeStruct>(loaded)) {
        if (generator->toReplace.find(loaded->toString()) != generator->toReplace.end())
            loaded = generator->toReplace[loaded->toString()];

        if (instanceof<TypeStruct>(loaded)) {
            TypeStruct* ts = (TypeStruct*)loaded;

            if (ts->types.size() > 0) {
                for (size_t i = 0; i < ts->types.size(); i++) ts->types[i] = checkForTemplated(ts->types[i]);
                ts->updateByTypes();
            }
        }

        if (parent != nullptr) {
            if (instanceof<TypePointer>(parent)) ((TypePointer*)parent)->instance = loaded;
            else if (instanceof<TypeArray>(parent)) ((TypeArray*)parent)->element = loaded;
            else if (instanceof<TypeConst>(parent)) ((TypeConst*)parent)->instance = loaded;
            return type;
        }
        return loaded;
    }
    return type;
}

std::vector<LLVMTypeRef> NodeStruct::getParameters(bool isLinkOnce) {
    DEBUG_LOG(Debug::Category::CodeGen, "Getting parameters for struct: " + name);

    std::vector<LLVMTypeRef> values;

    for (size_t i = 0; i < this->elements.size(); i++) {
        if (instanceof<NodeVar>(this->elements[i])) {
            NodeVar* var = (NodeVar*)this->elements[i];
            var->isExtern = (var->isExtern || this->isImported);
            var->isComdat = this->isComdat;
            AST::structMembersTable[std::pair<std::string, std::string>(this->name, var->name)] =
                StructMember{.number = i, .var = var};

            Types::replaceTemplates(&var->type);

            if (var->value != nullptr && !instanceof<NodeNone>(var->value))
                this->predefines[var->name] = StructPredefined{.element = i, .value = var->value, .isStruct = false, .name = var->name};
            else if (instanceof<TypeStruct>(var->type)) {
                TypeStruct* ts = (TypeStruct*)var->type;
                if (AST::structTable.find(ts->name) != AST::structTable.end() && AST::structTable[ts->name]->hasPredefines()) {
                    this->predefines[var->name] = StructPredefined{.element = i, .value = nullptr, .isStruct = true, .name = var->name};
                }
            }

            values.push_back(generator->genType(var->type, this->loc));
        }
        else {
            NodeFunc* func = (NodeFunc*)this->elements[i];
            if (func->isCtargs) continue;

            // Constructor handling
            if (func->origName == "this") {
                if (func->isChecked) {
                    func->isMethod = false;
                    func->isChecked = false;
                }

                func->name = this->origname;
                func->namespacesNames = std::vector<std::string>(this->namespacesNames);
                func->isTemplatePart = this->isLinkOnce;
                func->isComdat = this->isComdat;

                if (this->isImported) {
                    func->isExtern = true;
                    func->check();
                    this->constructors.push_back(func);
                    continue;
                }

                func->check();
                this->constructors.push_back(func);
            }
            // Destructor handling
            else if (func->origName == "~this") {
                this->destructor = func;
                func->name = "~" + this->origname;
                func->structContext = this->name;
                func->namespacesNames = std::vector<std::string>(this->namespacesNames);
                func->isTemplatePart = this->isLinkOnce;
                func->isComdat = this->isComdat;
                func->isChecked = false;

                Type* outType = (!this->constructors.empty() ? this->constructors[0]->type : new TypePointer(new TypeStruct(this->name)));
                if (instanceof<TypeStruct>(outType)) outType = new TypePointer(outType);
                this->destructor->args = std::vector<FuncArgSet>({FuncArgSet{.name = "this", .type = outType, .internalTypes = {outType}}});

                if (isImported) {
                    func->isExtern = true;
                    func->check();
                    continue;
                }
                func->check();
            }
            // Operator handling
            else if (func->origName.find('(') != std::string::npos) {
                func->isTemplatePart = isLinkOnce;
                func->isComdat = isComdat;
                func->isChecked = false;
                if (isImported) func->isExtern = true;

                char oper = 0;
                if (func->origName.find("(+)") != std::string::npos) {oper = TokType::Plus; func->name = this->name + "(+)";}
                else if (func->origName.find("(-)") != std::string::npos) {oper = TokType::Minus; func->name = this->name + "(-)";}
                else if (func->origName.find("(*)") != std::string::npos) {oper = TokType::Multiply; func->name = this->name + "(*)";}
                else if (func->origName.find("(/)") != std::string::npos) {oper = TokType::Divide; func->name = this->name + "(/)";}
                else if (func->origName.find("(<)") != std::string::npos) {oper = TokType::Less; func->name = this->name + "(<)";}
                else if (func->origName.find("(>)") != std::string::npos) {oper = TokType::More; func->name = this->name + "(>)";}
                else if (func->origName.find("(<=)") != std::string::npos) {oper = TokType::LessEqual; func->name = this->name + "(<=)";}
                else if (func->origName.find("(>=)") != std::string::npos) {oper = TokType::MoreEqual; func->name = this->name + "(>=)";}
                else if (func->origName.find("(=)") != std::string::npos) {oper = TokType::Equ; func->name = this->name + "(=)";}
                else if (func->origName.find("(==)") != std::string::npos) {oper = TokType::Equal; func->name = this->name + "(==)";}
                else if (func->origName.find("(!=)") != std::string::npos) {oper = TokType::Nequal; func->name = this->name + "(!=)";}
                else if (func->origName.find("([])") != std::string::npos) {oper = TokType::Rbra; func->name = this->name + "([])";}
                else if (func->origName.find("([]=)") != std::string::npos || func->origName.find("(=[])") != std::string::npos) {oper = TokType::Lbra; func->name = this->name + "([]=)";}
                else if (func->origName.find("([]&)") != std::string::npos || func->origName.find("(&[])") != std::string::npos) {oper = TokType::Amp; func->name = this->name + "([]&)";}
                else if (func->origName.find("(in)") != std::string::npos) {oper = TokType::In; func->name = this->name + "(in)";}

                Types::replaceTemplates(&func->type);
                for (size_t j = 0; j < func->args.size(); j++) Types::replaceTemplates(&func->args[j].type);

                if (oper != TokType::Rbra) func->name = func->name + typesToString(func->args);
                this->operators[oper][(oper != TokType::Rbra ? typesToString(func->args) : "")] = func;
                this->methods.push_back(func);
                FuncRegistry::instance().registerOperator(func, this->name, oper);
                func->check();
            }
            // Regular method handling
            else {
                Type* outType = nullptr;

                if (!this->constructors.empty()) {
                    outType = this->constructors[0]->type;
                    if (instanceof<TypeStruct>(outType)) outType = new TypePointer(outType);
                }
                else outType = new TypePointer(new TypeStruct(this->name));

                if ((func->args.size() > 0 && func->args[0].name != "this") || func->args.size() == 0)
                    func->args.insert(func->args.begin(), FuncArgSet{.name = "this", .type = outType, .internalTypes = {outType}});

                func->isTemplatePart = this->isLinkOnce;
                func->name = this->name + "." + func->origName;
                func->isMethod = true;
                func->structContext = this->name;
                func->isExtern = (func->isExtern || this->isImported);
                func->isComdat = this->isComdat;

                Types::replaceTemplates(&func->type);
                for (size_t j = 0; j < func->args.size(); j++) Types::replaceTemplates(&func->args[j].type);

                if (AST::methodTable.find(std::pair<std::string, std::string>(this->name, func->origName)) != AST::methodTable.end()) {
                    std::string sTypes = typesToString(AST::methodTable[std::pair<std::string, std::string>(this->name, func->origName)]->args);
                    std::string types = typesToString(func->args);
                    if (sTypes != types) {
                        func->origName += types;
                        AST::methodTable[std::pair<std::string, std::string>(this->name, func->origName)] = func;
                    }
                    else generator->error("method \033[1m" + func->origName + "\033[22m has already been declared!", this->loc);
                }
                else AST::methodTable[std::pair<std::string, std::string>(this->name, func->origName)] = func;
                func->check();
                this->methods.push_back(func);
            }
        }
    }
    return values;
}

long NodeStruct::getSize() {
    long result = 0;
    for (size_t i = 0; i < this->elements.size(); i++) {
        if (instanceof<NodeVar>(this->elements[i])) result += ((NodeVar*)this->elements[i])->type->getSize();
    }
    return result;
}

void NodeStruct::check() {
    if (isChecked) return;
    isChecked = true;
    DEBUG_LOG(Debug::Category::Parser, "Checking struct: " + name);

    if (!namespacesNames.empty()) name = namespacesToString(namespacesNames, name);

    if (AST::structTable.find(name) != AST::structTable.end()) {
        noCompile = true;
        return;
    }

    // Handle extends
    if (!extends.empty()) {
        auto extendedIt = AST::structTable.find(extends);
        if (extendedIt == AST::structTable.end()) {
            generator->error("extended struct \033[1m" + extends + "\033[22m not found!", loc);
            return;
        }

        NodeStruct* extended = extendedIt->second;

        for (const auto& element : extended->elements) {
            if (instanceof<NodeVar>(element)) {
                if (!static_cast<NodeVar*>(element)->isNoCopy) elements.push_back(element);
            }
            else elements.push_back(element);
        }

        methods.reserve(methods.size() + extended->methods.size());
        for (const auto& method : extended->methods) {
            if (!method->isNoCopy) methods.push_back(method);
        }
    }

    AST::structTable[name] = this;
}

bool NodeStruct::hasPredefines() {
    for (size_t i = 0; i < this->elements.size(); i++) {
        if (instanceof<NodeVar>(this->elements[i])) {
            if (instanceof<TypeStruct>(((NodeVar*)this->elements[i])->type)) {
                return ((TypeStruct*)((NodeVar*)this->elements[i])->type)->name != this->name &&
                    AST::structTable[((TypeStruct*)((NodeVar*)this->elements[i])->type)->name]->hasPredefines();
            }
            else return ((NodeVar*)this->elements[i])->value != nullptr;
        }
    }
    return false;
}

std::vector<Node*> NodeStruct::copyElements() {
    std::vector<Node*> buffer;
    for (size_t i = 0; i < this->elements.size(); i++) buffer.push_back(this->elements[i]->copy());
    return buffer;
}