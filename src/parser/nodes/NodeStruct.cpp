/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "../../include/parser/nodes/NodeStruct.hpp"
#include "../../include/utils.hpp"
#include "../../include/parser/ast.hpp"
#include "../../include/parser/nodes/NodeVar.hpp"
#include "../../include/parser/nodes/NodeFunc.hpp"
#include "../../include/parser/nodes/NodeBinary.hpp"
#include "../../include/parser/nodes/NodeIden.hpp"
#include "../../include/parser/nodes/NodeCast.hpp"
#include "../../include/parser/nodes/NodeCall.hpp"
#include "../../include/parser/nodes/NodeArray.hpp"
#include "../../include/parser/nodes/NodeType.hpp"
#include "../../include/parser/nodes/NodeSizeof.hpp"
#include "../../include/parser/nodes/NodeString.hpp"
#include "../../include/parser/nodes/NodeNone.hpp"
#include "../../include/parser/nodes/NodeGet.hpp"
#include "../../include/parser/nodes/NodeWhile.hpp"
#include "../../include/parser/nodes/NodeRet.hpp"
#include "../../include/parser/nodes/NodeBuiltin.hpp"
#include "../../include/parser/nodes/NodeBool.hpp"
#include "../../include/parser/ast.hpp"
#include <algorithm>

NodeStruct::NodeStruct(std::string name, std::vector<Node*> elements, int loc, std::string extends, std::vector<std::string> templateNames, std::vector<DeclarMod> mods) {
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
}

Node* NodeStruct::comptime() {return this;}

Node* NodeStruct::copy() {
    std::vector<Node*> cElements;
    for(int i=0; i<this->elements.size(); i++) cElements.push_back(this->elements[i]->copy());
    return new NodeStruct(this->origname, cElements, this->loc, this->extends, this->templateNames, this->mods);
}

Type* NodeStruct::getType() {return new TypeVoid();}

LLVMTypeRef NodeStruct::asConstType() {
    std::vector<LLVMTypeRef> types = this->getParameters(false);
    return LLVMStructTypeInContext(generator->context, types.data(), types.size(), this->isPacked);
}

std::vector<LLVMTypeRef> NodeStruct::getParameters(bool isLinkOnce) {
    std::vector<LLVMTypeRef> values;
    for(int i=0; i<this->elements.size(); i++) {
        if(instanceof<NodeVar>(this->elements[i])) {
            NodeVar* var = (NodeVar*)this->elements[i];
            var->isExtern = (var->isExtern || this->isImported);
            var->isComdat = this->isComdat;
            AST::structsNumbers[std::pair<std::string, std::string>(this->name, var->name)] = StructMember{.number = i, .var = var};
            if(var->value != nullptr && !instanceof<NodeNone>(var->value)) this->predefines.push_back(StructPredefined{.element = i, .value = var->value, .isStruct = false, .name = var->name});
            else if(instanceof<TypeStruct>(var->type)) {
                TypeStruct* ts = (TypeStruct*)var->type;
                if(AST::structTable.find(ts->name) != AST::structTable.end() && AST::structTable[ts->name]->hasPredefines()) {
                    this->predefines.push_back(StructPredefined{.element = i, .value = var->value, .isStruct = true, .name = var->name});
                }
            }
            values.push_back(generator->genType(var->type, this->loc));
        }
        else {
            NodeFunc* func = (NodeFunc*)this->elements[i];
            if(func->isCtargs) continue;
            if(func->origName == "this") {
                if(func->isChecked) {
                    func->isMethod = false;
                    func->isChecked = false;
                }

                func->name = this->origname;
                func->namespacesNames = std::vector<std::string>(this->namespacesNames);
                func->isTemplatePart = this->isLinkOnce;
                func->isComdat = this->isComdat;
                if(this->isImported) {
                    func->isExtern = true;
                    func->check();
                    this->constructors.push_back(func);
                    continue;
                }
                std::vector<Node*> toAdd;
                for(int i=0; i<toAdd.size(); i++) func->block->nodes.insert(func->block->nodes.begin(), (toAdd[i]));
                func->check();
                this->constructors.push_back(func);
            }
            else if(func->origName == "~this") {
                this->destructor = func;
                func->name = "~" + this->origname;
                func->namespacesNames = std::vector<std::string>(this->namespacesNames);
                func->isTemplatePart = this->isLinkOnce;
                func->isComdat = this->isComdat;
                func->isChecked = false;

                Type* outType = (!this->constructors.empty() ? this->constructors[0]->type : new TypePointer(new TypeStruct(this->name)));
                if(instanceof<TypeStruct>(outType)) outType = new TypePointer(outType);

                this->destructor->args = std::vector<FuncArgSet>({FuncArgSet{.name = "this", .type = outType, .internalTypes = {outType}}});

                if(isImported) {
                    func->isExtern = true;
                    func->check();
                    continue;
                }

                func->check();
            }
            else if(func->origName.find('(') != std::string::npos) {
                func->isTemplatePart = isLinkOnce;
                func->isComdat = isComdat;
                func->isChecked = false;
                if(isImported) func->isExtern = true;

                char oper;
                if(func->origName.find("(+)") != std::string::npos) {oper = TokType::Plus; func->name = this->name + "(+)";}
                else if(func->origName.find("(=)") != std::string::npos) {oper = TokType::Equ; func->name = this->name + "(=)";}
                else if(func->origName.find("(==)") != std::string::npos) {oper = TokType::Equal; func->name = this->name + "(==)";}
                else if(func->origName.find("(!=)") != std::string::npos) {oper = TokType::Nequal; func->name = this->name + "(!=)";}
                else if(func->origName.find("([])") != std::string::npos) {oper = TokType::Rbra; func->name = this->name + "([])";}
                else if((func->origName.find("([]=)") != std::string::npos) || (func->origName.find("(=[])") != std::string::npos)) {oper = TokType::Lbra; func->name = this->name + "([]=)";}
                else if((func->origName.find("([]&)") != std::string::npos) || (func->origName.find("(&[])") != std::string::npos)) {oper = TokType::GetPtr; func->name = this->name + "([]&)";}
                
                if(oper != TokType::Rbra) func->name = func->name + typesToString(func->args);
                this->operators[oper][(oper != TokType::Rbra ? typesToString(func->args) : "")] = func;
                this->methods.push_back(func);
                func->check();
            }
            else if(func->origName == "~with") {
                func->isMethod = true;
                func->isTemplatePart = isLinkOnce;
                func->name = "~with."+this->name;
                Type* outType = this->constructors[0]->type;
                if(instanceof<TypeStruct>(outType)) outType = new TypePointer(outType);
                func->args = {FuncArgSet{.name = "this", .type = outType, .internalTypes = {outType}}};
                this->with = func;
                if(this->isImported) {
                    func->isExtern = true;
                    func->check();
                }
            }
            else {
                Type* outType = nullptr;
                func->isTemplatePart = this->isLinkOnce;
                if(!this->constructors.empty()) {
                    outType = this->constructors[0]->type;
                    if(instanceof<TypeStruct>(outType)) outType = new TypePointer(outType);
                }
                else outType = new TypePointer(new TypeStruct(this->name));
                if((func->args.size() > 0 && func->args[0].name != "this") || func->args.size() == 0) func->args.insert(func->args.begin(), FuncArgSet{.name = "this", .type = outType, .internalTypes = {outType}});
                func->name = this->name + "." + func->origName;
                func->isMethod = true;
                func->isExtern = (func->isExtern || this->isImported);
                func->isComdat = this->isComdat;
                if(AST::methodTable.find(std::pair<std::string, std::string>(this->name, func->origName)) != AST::methodTable.end()) {
                    std::string sTypes = typesToString(AST::methodTable[std::pair<std::string, std::string>(this->name, func->origName)]->args);
                    if(sTypes != typesToString(func->args)) AST::methodTable[std::pair<std::string, std::string>(this->name, func->origName + typesToString(func->args))] = func;
                    else generator->error("method '" + func->origName + "' has already been declared on " + std::to_string(AST::methodTable[std::pair<std::string, std::string>(this->name, func->origName)]->loc) + " line!", this->loc);
                }
                else AST::methodTable[std::pair<std::string, std::string>(this->name, func->origName)] = func;
                this->methods.push_back(func);
            }
        }
    }
    return values;
}

long NodeStruct::getSize() {
    long result = 0;
    for(int i=0; i<this->elements.size(); i++) {
        if(instanceof<NodeVar>(this->elements[i])) result += ((NodeVar*)this->elements[i])->type->getSize();
    }
    return result;
}

void NodeStruct::check() {
    if(this->isChecked) return;
    this->isChecked = true;

    if(!this->namespacesNames.empty()) this->name = namespacesToString(this->namespacesNames, this->name);

    if(AST::structTable.find(this->name) != AST::structTable.end()) {
        noCompile = true;
        return;
    }

    if(!this->extends.empty()) {
        auto extendedIt = AST::structTable.find(this->extends);
        if(extendedIt == AST::structTable.end()) {
            generator->error("extended struct '" + this->extends + "' not found!", this->loc);
            return;
        }

        NodeStruct* extended = extendedIt->second;
        
        for(const auto& element : extended->elements) {
            if(instanceof<NodeVar>(element)) {
                if(!static_cast<NodeVar*>(element)->isNoCopy) this->elements.push_back(element);
            }
            else this->elements.push_back(element);
        }

        this->methods.reserve(this->methods.size() + extended->methods.size());
        for(const auto& method : extended->methods) {
            if(!method->isNoCopy) this->methods.push_back(method);
        }

        this->predefines.insert(this->predefines.end(), extended->predefines.begin(), extended->predefines.end());
    }

    AST::structTable[this->name] = this;
}

bool NodeStruct::hasPredefines() {
    for(int i=0; i<this->elements.size(); i++) {
        if(instanceof<NodeVar>(this->elements[i])) {
            if(instanceof<TypeStruct>(((NodeVar*)this->elements[i])->type)) {
                return ((TypeStruct*)((NodeVar*)this->elements[i])->type)->name != this->name && AST::structTable[((TypeStruct*)((NodeVar*)this->elements[i])->type)->name]->hasPredefines();
            }
            else return ((NodeVar*)this->elements[i])->value != nullptr;
        }
    }
    return false;
}

std::vector<Node*> NodeStruct::copyElements() {
    std::vector<Node*> buffer;
    for(int i=0; i<this->elements.size(); i++) buffer.push_back(this->elements[i]->copy());
    return buffer;
}

RaveValue NodeStruct::generate() {
    if(this->templateNames.size() > 0 || this->noCompile) return {};

    std::map<std::string, NodeBuiltin*> builtins;

    for(int i=0; i<this->mods.size(); i++) {
        while(AST::aliasTable.find(this->mods[i].name) != AST::aliasTable.end()) {
            if(instanceof<NodeArray>(AST::aliasTable[this->mods[i].name])) {
                this->mods[i].name = ((NodeString*)(((NodeArray*)AST::aliasTable[this->mods[i].name])->values[0]))->value;
                this->mods[i].value = ((NodeString*)(((NodeArray*)AST::aliasTable[this->mods[i].name])->values[1]))->value;
            }
            else this->mods[i].name = ((NodeString*)((NodeArray*)AST::aliasTable[this->mods[i].name]))->value;
        }
        if(this->mods[i].name == "packed") this->isPacked = true;
        else if(this->mods[i].name == "data") this->dataVar = this->mods[i].value;
        else if(this->mods[i].name == "length") this->lengthVar = this->mods[i].value;
        else if(this->mods[i].name.size() > 0 && this->mods[i].name[0] == '@') builtins[this->mods[i].name.substr(1)] = ((NodeBuiltin*)this->mods[i].genValue);
    }

    for(const auto &data : builtins) {
        Node* result = data.second->comptime();
        if(result == nullptr || (instanceof<NodeBool>(result) && !((NodeBool*)result)->value)) {
            generator->error("The '" + data.first+  "' builtin failed when generating the structure '" + this->name + "'!", this->loc);
            return {};
        }
    }

    generator->structures[this->name] = LLVMStructCreateNamed(generator->context, this->name.c_str());

    std::vector<LLVMTypeRef> params = this->getParameters(this->isTemplated);
    LLVMStructSetBody(generator->structures[this->name], params.data(), params.size(), this->isPacked);

    if(this->constructors.empty() && this->predefines.size() > 0) {
        // Creating the default constructor
        Type* thisType = new TypePointer(new TypeStruct(this->name));
        NodeBlock* constructorBlock = new NodeBlock({});

        for(int i=0; i<this->predefines.size(); i++) {
            if(!this->predefines[i].isStruct) constructorBlock->nodes.push_back(new NodeBinary(TokType::Equ, new NodeGet(new NodeIden("this", this->loc), this->predefines[i].name, true, this->loc), this->predefines[i].value, this->loc));
        }
        NodeFunc* constructor = new NodeFunc(
            this->origname, std::vector<FuncArgSet>({FuncArgSet{.name = "this", .type = thisType, .internalTypes = {thisType}}}),
            constructorBlock, false, std::vector<DeclarMod>(), this->loc,
            new TypeStruct(this->name), std::vector<std::string>()
        );
        constructor->namespacesNames = std::vector<std::string>(this->namespacesNames);
        constructor->isTemplatePart = this->isLinkOnce;
        constructor->isComdat = this->isComdat;
        if(this->isImported) constructor->isExtern = true;
        constructor->check();
        constructor->generate();
        this->constructors.push_back(constructor);
    }
    else {
        for(int i=0; i<this->constructors.size(); i++) {
            if(!this->constructors[i]->isChecked) this->constructors[i]->check();
            this->constructors[i]->generate();
        }
    }
    if(this->destructor == nullptr) {
        // Creating default destructor
        this->destructor = new NodeFunc(
            "~" + this->origname, std::vector<FuncArgSet>(),
            new NodeBlock({new NodeCall(this->loc, new NodeIden("std::free", this->loc), {new NodeIden("this", this->loc)})}), false,
            std::vector<DeclarMod>(), this->loc, new TypeVoid(), std::vector<std::string>()
        );
        this->destructor->namespacesNames = std::vector<std::string>(this->namespacesNames);
        this->destructor->isComdat = this->isComdat;

        if(this->constructors.size() > 0 && instanceof<TypePointer>(this->constructors[0]->type)) {
            if(isImported) {
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
        if(isImported) {
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
    for(int i=0; i<this->methods.size(); i++) {
        this->methods[i]->check();
        this->methods[i]->generate();
    }
    return {};
}

LLVMTypeRef NodeStruct::genWithTemplate(std::string sTypes, std::vector<Type*> types) {
    std::map<int32_t, Loop> activeLoops = std::map<int32_t, Loop>(generator->activeLoops);
    LLVMBuilderRef builder = generator->builder;
    LLVMBasicBlockRef currBB = generator->currBB;
    Scope* _scope = currScope;
    std::map<std::string, Type*> toReplace = std::map<std::string, Type*>(generator->toReplace);
    std::map<std::string, Node*> toReplaceValues = std::map<std::string, Node*>(generator->toReplaceValues);

    std::string _fn = "<";

    if(types.size() != this->templateNames.size()) {
        generator->error("the count of template types is not equal!", this->loc);
        return nullptr;
    }

    for(int i=0; i<types.size(); i++) {
        if(instanceof<TypeStruct>(types[i])) {
            if(AST::structTable.find(((TypeStruct*)types[i])->name) == AST::structTable.end() && !((TypeStruct*)types[i])->types.empty()) generator->genType(types[i], this->loc);
        }

        if(instanceof<TypeTemplateMember>(types[i])) {
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

    generator->toReplace[name + _fn.substr(0, _fn.size()-1) + ">"] = new TypeStruct(name + sTypes);

    NodeStruct* _struct = new NodeStruct(name + sTypes, this->copyElements(), this->loc, "", {}, this->mods);
    _struct->isTemplated = true;
    _struct->isComdat = true;

    for(int i=0; i<_struct->elements.size(); i+=1) {
        if(instanceof<NodeVar>(_struct->elements[i])) {
            Type* parent = nullptr;
            Type* ty = ((NodeVar*)_struct->elements[i])->type;

            while(instanceof<TypeConst>(ty) || instanceof<TypePointer>(ty) || instanceof<TypeArray>(ty)) {
                parent = ty;
                ty = ty->getElType();
            }

            if(instanceof<TypeStruct>(ty)) {
                while(generator->toReplace.find(ty->toString()) != generator->toReplace.end()) ty = generator->toReplace[ty->toString()];

                if(parent == nullptr) ((NodeVar*)_struct->elements[i])->type = ty;
                else if(instanceof<TypeConst>(parent)) ((TypeConst*)parent)->instance = ty;
                else if(instanceof<TypePointer>(parent)) ((TypePointer*)parent)->instance = ty;
                else if(instanceof<TypeArray>(parent)) ((TypeArray*)parent)->element = ty;
            }
        }
    }
    _struct->check();
    _struct->generate();

    generator->activeLoops = std::map<int32_t, Loop>(activeLoops);
    generator->builder = builder;
    generator->currBB = currBB;
    currScope = _scope;
    generator->toReplace = std::map<std::string, Type*>(toReplace);
    generator->toReplaceValues = std::map<std::string, Node*>(toReplaceValues);

    return generator->structures[_struct->name];
}

std::vector<NodeVar*> NodeStruct::getVariables() {
    std::vector<NodeVar*> data;
    for(int i=0; i<elements.size(); i++) {
        if(instanceof<NodeVar>(elements[i])) data.push_back(((NodeVar*)elements[i]));
    }
    return data;
}