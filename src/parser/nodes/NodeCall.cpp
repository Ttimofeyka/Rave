/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "../../include/parser/nodes/NodeCall.hpp"
#include "../../include/parser/nodes/NodeIden.hpp"
#include "../../include/parser/nodes/NodeFunc.hpp"
#include "../../include/parser/nodes/NodeVar.hpp"
#include "../../include/parser/nodes/NodeGet.hpp"
#include "../../include/parser/nodes/NodeIndex.hpp"
#include "../../include/parser/nodes/NodeArray.hpp"
#include "../../include/parser/nodes/NodeDone.hpp"
#include "../../include/parser/nodes/NodeStruct.hpp"
#include "../../include/parser/nodes/NodeUnary.hpp"
#include "../../include/parser/nodes/NodeCast.hpp"
#include "../../include/parser/nodes/NodeBuiltin.hpp"
#include "../../include/parser/nodes/NodeBinary.hpp"
#include "../../include/parser/nodes/NodeType.hpp"
#include "../../include/parser/Types.hpp"
#include "../../include/parser/ast.hpp"
#include "../../include/lexer/lexer.hpp"

NodeCall::NodeCall(long loc, Node* func, std::vector<Node*> args) {
    this->loc = loc;
    this->func = func;
    this->args = std::vector<Node*>(args);
}

Type* NodeCall::getType() {
    if(instanceof<NodeIden>(this->func)) {
        if(AST::funcTable.find((((NodeIden*)this->func)->name+typesToString(this->getTypes()))) != AST::funcTable.end()) return AST::funcTable[(((NodeIden*)this->func)->name+typesToString(this->getTypes()))]->getType();
    }
    return this->func->getType();
}

std::vector<Type*> NodeCall::getTypes() {
    std::vector<Type*> arr;
    for(int i=0; i<this->args.size(); i++) arr.push_back(this->args[i]->getType());
    return arr;
}

std::vector<LLVMValueRef> NodeCall::correctByLLVM(std::vector<LLVMValueRef> values, std::vector<FuncArgSet> fas) {
    std::vector<LLVMValueRef> params = std::vector<LLVMValueRef>(values);
    if(fas.empty() || params.size() != fas.size()) return params;
    for(int i=0; i<params.size(); i++) {
        if(fas[i].type->toString() == "void*" || fas[i].type->toString() == "char*") {
            LLVMTypeRef type = LLVMTypeOf(params[i]);
            if(LLVMGetTypeKind(type) != LLVMPointerTypeKind) params[i] = Binary::castValue(params[i], LLVMPointerType(LLVMInt8TypeInContext(generator->context), 0), this->loc);
            else if(LLVMGetTypeKind(LLVMGetElementType(type)) != LLVMIntegerTypeKind) params[i] = Binary::castValue(params[i], LLVMPointerType(LLVMInt8TypeInContext(generator->context), 0), this->loc);
        }
        else if(instanceof<TypePointer>(fas[i].type) && instanceof<TypeStruct>(((TypePointer*)fas[i].type)->instance)) {
            LLVMTypeRef type = LLVMTypeOf(params[i]);
            if(LLVMGetTypeKind(type) != LLVMPointerTypeKind) {
                LLVMValueRef temp = LLVMBuildAlloca(generator->builder, type, "NodeCall_getParameters_temp");
                LLVMBuildStore(generator->builder, params[i], temp);
                params[i] = temp;
            }
        }
        else if(!instanceof<TypePointer>(fas[i].type) && LLVMGetTypeKind(LLVMTypeOf(params[i])) == LLVMPointerTypeKind) params[i] = LLVMBuildLoad(generator->builder, params[i], "correctLoad");
    }
    return params;
}

std::vector<LLVMValueRef> NodeCall::getParameters(bool isVararg, std::vector<FuncArgSet> fas) {
    std::vector<LLVMValueRef> params;
    if(fas.empty() || this->args.size() != fas.size()) {
        for(int i=0; i<this->args.size(); i++) params.push_back(this->args[i]->generate());
        return params;
    }
    for(int i=0; i<this->args.size(); i++) {
        if(instanceof<TypePointer>(fas[i].type) && instanceof<TypeStruct>(((TypePointer*)fas[i].type)->instance) && !instanceof<TypePointer>(this->args[i]->getType())) {
            if(instanceof<NodeIden>(this->args[i])) ((NodeIden*)this->args[i])->isMustBePtr = true;
            else if(instanceof<NodeGet>(this->args[i])) ((NodeGet*)this->args[i])->isMustBePtr = true;
            else if(instanceof<NodeIndex>(this->args[i])) ((NodeIndex*)this->args[i])->isMustBePtr = true;
        }
        params.push_back(this->args[i]->generate());
    }
    for(int i=0; i<params.size(); i++) {
        if(fas[i].type->toString() == "void*" || fas[i].type->toString() == "char*") {
            LLVMTypeRef type = LLVMTypeOf(params[i]);
            if(LLVMGetTypeKind(type) != LLVMPointerTypeKind) params[i] = Binary::castValue(params[i], LLVMPointerType(LLVMInt8TypeInContext(generator->context), 0), this->loc);
            else if(LLVMGetTypeKind(LLVMGetElementType(type)) != LLVMIntegerTypeKind) params[i] = Binary::castValue(params[i], LLVMPointerType(LLVMInt8TypeInContext(generator->context), 0), this->loc);
        }
        else if(instanceof<TypePointer>(fas[i].type) && instanceof<TypeStruct>(((TypePointer*)fas[i].type)->instance)) {
            LLVMTypeRef type = LLVMTypeOf(params[i]);
            if(LLVMGetTypeKind(type) != LLVMPointerTypeKind) {
                LLVMValueRef temp = LLVMBuildAlloca(generator->builder, type, "NodeCall_getParameters_temp");
                LLVMBuildStore(generator->builder, params[i], temp);
                params[i] = temp;
            }
        }
    }
    return params;
}

bool hasIdenticallyArgs(std::vector<Type*> one, std::vector<Type*> two) {
    if(one.size() != two.size()) return false;
    for(int i=0; i<one.size(); i++) {
        if(one[i]->toString() != two[i]->toString()) return false;
    }
    return true;
}

bool hasIdenticallyArgs(std::vector<Type*> one, std::vector<FuncArgSet> two) {
    if(one.size() != two.size()) return false;
    for(int i=0; i<one.size(); i++) {
        if(one[i] == nullptr || two[i].type == nullptr || one[i]->toString() != two[i].type->toString()) return false;
    }
    return true;
}

std::vector<FuncArgSet> tfaToFas(std::vector<TypeFuncArg*> tfa) {
    std::vector<FuncArgSet> fas;
    for(int i=0; i<tfa.size(); i++) fas.push_back(FuncArgSet{.name = tfa[i]->name, .type = tfa[i]->type});
    return fas;
}

LLVMValueRef NodeCall::generate() {
    if(instanceof<NodeIden>(this->func)) {
        NodeIden* idenFunc = (NodeIden*)this->func;
        if(AST::funcTable.find(idenFunc->name) != AST::funcTable.end()) {
            std::string sTypes = typesToString(this->getTypes());
            if(generator->functions.find(idenFunc->name) == generator->functions.end()) {
                // Function with compile-time args (ctargs)
                if(AST::funcTable[idenFunc->name]->isCtargs) {
                    if(generator->functions.find(idenFunc->name+sTypes) != generator->functions.end()) {
                        return LLVMBuildCall(generator->builder, generator->functions[idenFunc->name+sTypes], this->getParameters(false).data(), this->args.size(), (instanceof<TypeVoid>(AST::funcTable[idenFunc->name]->type) ? "" : "callFunc"));
                    }
                    std::vector<LLVMValueRef> params = this->getParameters(false);
                    std::vector<LLVMTypeRef> types;
                    for(int i=0; i<params.size(); i++) types.push_back(LLVMTypeOf(params[i]));
                    return LLVMBuildCall(generator->builder, generator->functions[AST::funcTable[idenFunc->name]->generateWithCtargs(types)], this->getParameters(false).data(), this->args.size(), (instanceof<TypeVoid>(AST::funcTable[idenFunc->name]->type) ? "" : "callFunc"));
                }
                if(AST::debugMode) {
                    std::cout << "DEBUG_MODE: undefined function (generator->functions)!" << std::endl;
                    std::cout << "All functions:" << std::endl;
                    for(auto const& x : AST::funcTable) std::cout << "\t" << x.first << std::endl;
                }
                generator->error("undefined function '"+idenFunc->name+"'!", this->loc);
                return nullptr;
            }
            if(AST::funcTable.find(idenFunc->name+sTypes) != AST::funcTable.end()) {
                return LLVMBuildCall(generator->builder, generator->functions[idenFunc->name+sTypes], this->getParameters(false, AST::funcTable[idenFunc->name+sTypes]->args).data(), this->args.size(), (instanceof<TypeVoid>(AST::funcTable[idenFunc->name+sTypes]->type) ? "" : "callFunc"));
            }
            return LLVMBuildCall(generator->builder, generator->functions[idenFunc->name], this->getParameters(false, AST::funcTable[idenFunc->name]->args).data(), this->args.size(), (instanceof<TypeVoid>(AST::funcTable[idenFunc->name]->type) ? "" : "callFunc"));
        }
        if(currScope->has(idenFunc->name)) {
            TypeFunc* fn = (TypeFunc*)(currScope->getVar(idenFunc->name)->type);
            return LLVMBuildCall(generator->builder, currScope->get(idenFunc->name), this->getParameters(false, tfaToFas(fn->args)).data(), this->args.size(), (instanceof<TypeVoid>(fn->main) ? "" : "callFunc"));
        }
        if(AST::debugMode) {
            std::cout << "DEBUG_MODE: undefined function!" << std::endl;
            std::cout << "All functions:" << std::endl;
            for(auto const& x : AST::funcTable) std::cout << "\t" << x.first << std::endl;
        }
        //for(auto const& x : AST::methodTable) std::cout << "\t" << x.first.first << " : " << x.first.second << std::endl;
        generator->error("undefined function '"+idenFunc->name+"'!", this->loc);
        return nullptr;
    }
    if(instanceof<NodeGet>(this->func)) {
        NodeGet* getFunc = (NodeGet*)this->func;
        if(instanceof<NodeIden>(getFunc->base)) {
            NodeIden* idenFunc = (NodeIden*)getFunc->base;
            if(!currScope->has(idenFunc->name)) {
                generator->error("undefined variable '"+idenFunc->name+"'!", this->loc);
                return nullptr;
            }
            NodeVar* var = currScope->getVar(idenFunc->name, this->loc);
            TypeStruct* structure = nullptr;

            if(instanceof<TypeStruct>(var->type)) structure = (TypeStruct*)var->type;
            else if(instanceof<TypePointer>(var->type) && instanceof<TypeStruct>(((TypePointer*)var->type)->instance)) structure = (TypeStruct*)((TypePointer*)var->type)->instance;
            else {
                generator->error("variable '"+idenFunc->name+"' doesn't have structure as type!", this->loc);
                return nullptr;
            }

            if(AST::structTable.find(structure->name) != AST::structTable.end()) {
                std::vector<LLVMValueRef> params = this->getParameters(false);
                std::vector<Type*> types;
                params.insert(params.begin(), currScope->getWithoutLoad(idenFunc->name));
                for(int i=0; i<params.size(); i++) types.push_back(lTypeToType(LLVMTypeOf(params[i])));

                std::pair<std::string, std::string> method = std::pair<std::string, std::string>(structure->name, getFunc->field);
                if(AST::methodTable.find(method) != AST::methodTable.end() && hasIdenticallyArgs(types, AST::methodTable[method]->args)) {
                    params = this->correctByLLVM(params, AST::methodTable[method]->args);
                    return LLVMBuildCall(generator->builder, generator->functions[AST::methodTable[method]->name], params.data(), params.size(), (instanceof<TypeVoid>(AST::methodTable[method]->type) ? "" : "callFunc"));
                }
                
                method.second += typesToString(types);
                if(AST::methodTable.find(method) != AST::methodTable.end()) {
                    //for(auto const& x : generator->functions) std::cout << "\t\t" << x.first << std::endl;
                    params = this->correctByLLVM(params, AST::methodTable[method]->args);
                    return LLVMBuildCall(generator->builder, generator->functions[AST::methodTable[method]->name], params.data(), params.size(), (instanceof<TypeVoid>(AST::methodTable[method]->type) ? "" : "callFunc"));
                }

                method.second = method.second.substr(0, method.second.find('['));
                if(AST::methodTable.find(method) != AST::methodTable.end()) {
                    params = this->correctByLLVM(params, AST::methodTable[method]->args);
                    return LLVMBuildCall(generator->builder, generator->functions[AST::methodTable[method]->name], params.data(), params.size(), (instanceof<TypeVoid>(AST::methodTable[method]->type) ? "" : "callFunc"));
                }

                generator->error("undefined method '"+method.second.substr(0, method.second.find('['))+"' of structure '"+structure->name+"'!", this->loc);
                return nullptr;
            }

            generator->error("undefined structure '"+structure->name+"'!", this->loc);
            return nullptr;
        }
    }
    if(instanceof<NodeUnary>(this->func)) {
        NodeCall* nc = new NodeCall(this->loc, ((NodeUnary*)this->func)->base, this->args);
        if(((NodeUnary*)this->func)->type == TokType::Ne) nc->isInverted = true;
        return nc->generate();
    }
    generator->error("a call of this kind is temporarily unavailable!", this->loc);
    return nullptr;
}

void NodeCall::check() {this->isChecked = true;}
Node* NodeCall::comptime() {return this;}
Node* NodeCall::copy() {return new NodeCall(this->loc, this->func, this->args);}