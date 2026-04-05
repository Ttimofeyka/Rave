/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

// NodeCall class and Call helpers - Function call handling
// Split for better maintainability - see also CallResolver.cpp and CallTemplate.cpp

#include "../../include/parser/nodes/NodeCall.hpp"
#include "../../include/parser/nodes/NodeIden.hpp"
#include "../../include/parser/nodes/NodeFunc.hpp"
#include "../../include/parser/nodes/NodeVar.hpp"
#include "../../include/parser/nodes/NodeGet.hpp"
#include "../../include/parser/nodes/NodeIndex.hpp"
#include "../../include/parser/nodes/NodeUnary.hpp"
#include "../../include/parser/nodes/NodeDone.hpp"
#include "../../include/parser/nodes/NodeStruct.hpp"
#include "../../include/parser/ast.hpp"
#include "../../include/parser/FuncRegistry.hpp"
#include "../../include/debug.hpp"

NodeCall::NodeCall(int loc, Node* func, std::vector<Node*> args)
    : loc(loc), func(func), args(args) {}

NodeCall::~NodeCall() {
    delete func;
    for (Node* arg : args) delete arg;
}

// Helper: check and generate function if needed
inline void checkAndGenerate(std::string name) {
    if (generator->functions.find(name) == generator->functions.end()) {
        if (AST::funcTable.find(name) == AST::funcTable.end()) return;
        AST::funcTable[name]->generate();
    }
}

// Helper: convert TypeFuncArg to FuncArgSet (shared across files)
inline std::vector<FuncArgSet> tfaToFas(std::vector<TypeFuncArg*> tfa) {
    std::vector<FuncArgSet> fas;
    for (auto* arg : tfa) {
        Type* type = arg->type;
        while (generator->toReplace.find(type->toString()) != generator->toReplace.end())
            type = generator->toReplace[type->toString()];
        fas.push_back(FuncArgSet{.name = arg->name, .type = type});
    }
    return fas;
}

std::vector<Type*> Call::getTypes(std::vector<Node*>& arguments) {
    std::vector<Type*> array;
    for (size_t i = 0; i < arguments.size(); i++) {
        Type* t = arguments[i]->getType();
        Types::replaceTemplates(&t);
        array.push_back(t);
    }
    return array;
}

std::vector<Type*> Call::getParamsTypes(std::vector<RaveValue>& params) {
    std::vector<Type*> array;
    for (size_t i = 0; i < params.size(); i++) array.push_back(params[i].type);
    return array;
}

std::vector<RaveValue> Call::genParameters(std::vector<Node*>& arguments, std::vector<int>& byVals,
                                            std::vector<FuncArgSet>& fas, CallSettings settings) {
    DEBUG_LOG(Debug::Category::FuncCall, "Generating parameters for call");

    std::vector<RaveValue> params;

    if (arguments.size() != fas.size()) {
        for (size_t i = 0; i < arguments.size(); i++) {
            if (instanceof<NodeIden>(arguments[i])) ((NodeIden*)arguments[i])->isMustBePtr = false;
            else if (instanceof<NodeIndex>(arguments[i])) ((NodeIndex*)arguments[i])->isMustBePtr = false;
            else if (instanceof<NodeGet>(arguments[i])) ((NodeGet*)arguments[i])->isMustBePtr = false;
            params.push_back(arguments[i]->generate());
        }
        return params;
    }

    for (size_t i = 0; i < arguments.size(); i++) {
        if (instanceof<TypePointer>(fas[i].type) && instanceof<TypeStruct>(fas[i].type->getElType()) &&
            !instanceof<TypePointer>(arguments[i]->getType())) {
            if (instanceof<NodeIden>(arguments[i])) ((NodeIden*)arguments[i])->isMustBePtr = true;
            else if (instanceof<NodeGet>(arguments[i])) ((NodeGet*)arguments[i])->isMustBePtr = true;
            else if (instanceof<NodeIndex>(arguments[i])) ((NodeIndex*)arguments[i])->isMustBePtr = true;
        }

        if (instanceof<TypeStruct>(fas[i].type) && instanceof<TypePointer>(arguments[i]->getType())) {
            if (instanceof<NodeIden>(arguments[i])) ((NodeIden*)arguments[i])->isMustBePtr = false;
            else if (instanceof<NodeGet>(arguments[i])) ((NodeGet*)arguments[i])->isMustBePtr = false;
            else if (instanceof<NodeIndex>(arguments[i])) ((NodeIndex*)arguments[i])->isMustBePtr = false;
        }

        params.push_back(arguments[i]->generate());
    }

    // Handle implicit conversions and type adjustments
    for (size_t i = 0; i < params.size(); i++) {
        if (instanceof<TypeStruct>(fas[i].type) && !instanceof<TypeStruct>(params[i].type)) {
            std::string name = fas[i].type->toString();
            if (AST::structTable.find(name) != AST::structTable.end()) {
                auto& operators = AST::structTable[name]->operators;
                if (operators.find(TokType::Equ) != operators.end()) {
                    Type* ty = operators[TokType::Equ].begin()->second->args[1].type;
                    if ((isBytePointer(ty) && isBytePointer(params[i].type)) || ty->toString() == params[i].type->toString()) {
                        RaveValue tempVariable = LLVM::alloc(fas[i].type, "getParameters_implicit_op_tempvar");
                        Predefines::handle(fas[i].type, new NodeDone(tempVariable), -1);
                        checkAndGenerate(operators[TokType::Equ].begin()->second->name);
                        LLVM::call(generator->functions[operators[TokType::Equ].begin()->second->name], {tempVariable, params[i]}, "");
                        params[i] = tempVariable;
                    }
                }
            }
        }

        if (instanceof<TypePointer>(fas[i].type)) {
            if (instanceof<TypeStruct>(fas[i].type->getElType()) && !instanceof<TypePointer>(params[i].type))
                LLVM::makeAsPointer(params[i]);
            else if (isBytePointer(fas[i].type) && fas[i].type->toString() != params[i].type->toString())
                LLVM::cast(params[i], new TypePointer(basicTypes[BasicType::Char]), settings.loc);
        }
        else {
            while (instanceof<TypePointer>(params[i].type))
                params[i] = LLVM::load(params[i], "genParameters_load", settings.loc);

            if (instanceof<TypeBasic>(fas[i].type)) {
                if (instanceof<TypeBasic>(params[i].type) && ((TypeBasic*)fas[i].type) != ((TypeBasic*)params[i].type))
                    LLVM::cast(params[i], fas[i].type, settings.loc);
            }
        }

        if (settings.isCW64) {
            if (instanceof<TypeDivided>(fas[i].internalTypes[0])) {
                if (!instanceof<TypePointer>(params[i].type)) {
                    RaveValue temp = LLVM::alloc(params[i].type, "getParameters_stotd_temp");
                    LLVMBuildStore(generator->builder, params[i].value, temp.value);
                    params[i].value = temp.value;
                }
                params[i].value = LLVMBuildLoad2(generator->builder,
                    generator->genType(((TypeDivided*)fas[i].internalTypes[0])->mainType, settings.loc),
                    params[i].value, "getParameters_stotd");
            }
            else if (instanceof<TypeByval>(fas[i].internalTypes[0])) {
                if (!instanceof<TypePointer>(params[i].type)) LLVM::makeAsPointer(params[i]);
                byVals.push_back(i);
            }
        }
        else if (instanceof<TypeFunc>(fas[i].type)) {
            TypeFunc* tfunc = (TypeFunc*)(fas[i].type);
            if (instanceof<TypeBasic>(tfunc->main) && ((TypeBasic*)(tfunc->main))->type == BasicType::Char) {
                if (instanceof<TypePointer>(params[i].type) && instanceof<TypeFunc>(params[i].type->getElType())
                    && instanceof<TypeVoid>(((TypeFunc*)params[i].type->getElType())->main)) {
                    params[i].value = LLVMBuildPointerCast(generator->builder, params[i].value,
                        generator->genType(fas[i].type, settings.loc), "castVFtoCF");
                    TypeFunc* tfunc = ((TypeFunc*)params[i].type->getElType());
                    tfunc->main = basicTypes[BasicType::Char];
                    ((TypePointer*)params[i].type)->instance = tfunc;
                }
            }
        }
    }

    return params;
}

std::vector<RaveValue> Call::genParameters(std::vector<Node*>& arguments, std::vector<int>& byVals,
                                                   NodeFunc* function, int loc) {
    return Call::genParameters(arguments, byVals, function->args,
        CallSettings{function->isCdecl64 || function->isWin64, function->isVararg, loc});
}

Node* Call::resolveAlias(const std::string& name) {
    if (AST::aliasTable.find(name) != AST::aliasTable.end()) return AST::aliasTable[name];
    if (currScope != nullptr && currScope->aliasTable.find(name) != currScope->aliasTable.end())
        return currScope->aliasTable[name];
    return nullptr;
}

// External declaration for callNamedFunction (in CallResolver.cpp)
RaveValue Call::callNamedFunction(int loc, const std::string& name, std::vector<Node*>& arguments) {
    DEBUG_LOG(Debug::Category::FuncCall, "Calling named function: " + name);

    std::vector<int> byVals;
    std::vector<Type*> types = Call::getTypes(arguments);

    // Check for vararg/cdecl64/win64 functions
    NodeFunc* targetFunc = nullptr;
    auto overloads = FuncRegistry::instance().findAllOverloads(name);
    for (auto* func : overloads) {
        if ((func->isVararg || func->isCdecl64 || func->isWin64) && func->args.size() <= arguments.size()) {
            targetFunc = func;
            break;
        }
    }

    if (targetFunc == nullptr && AST::funcTable.find(name) != AST::funcTable.end()) {
        NodeFunc* func = AST::funcTable[name];
        if ((func->isVararg || func->isCdecl64 || func->isWin64) && func->args.size() <= arguments.size())
            targetFunc = func;
    }

    // Try signature matching
    if (targetFunc == nullptr) {
        FuncSignature sig(name, types);
        targetFunc = FuncRegistry::instance().findBestMatch(sig);
        if (targetFunc != nullptr && !targetFunc->templateNames.empty() && !targetFunc->isTemplate)
            targetFunc = nullptr;
        if (targetFunc != nullptr && targetFunc->isCtargsPart) {
            if (targetFunc->args.size() != types.size()) {
                targetFunc = nullptr;
            } else {
                for (size_t i = 0; i < types.size(); i++) {
                    if (targetFunc->args[i].type->toString() != types[i]->toString()) {
                        targetFunc = nullptr;
                        break;
                    }
                }
            }
        }
    }

    if (targetFunc == nullptr) {
        targetFunc = FuncRegistry::instance().findBestMatch(name, types);
        if (targetFunc != nullptr && !targetFunc->templateNames.empty() && !targetFunc->isTemplate)
            targetFunc = nullptr;
        if (targetFunc != nullptr && targetFunc->isCtargsPart) {
            if (targetFunc->args.size() != types.size()) {
                targetFunc = nullptr;
            } else {
                for (size_t i = 0; i < types.size(); i++) {
                    if (targetFunc->args[i].type->toString() != types[i]->toString()) {
                        targetFunc = nullptr;
                        break;
                    }
                }
            }
        }
    }

    if (targetFunc == nullptr && AST::funcTable.find(name) != AST::funcTable.end())
        targetFunc = AST::funcTable[name];

    if (targetFunc == nullptr)
        generator->error("undefined function \033[1m" + name + "\033[22m!", loc);

    // Handle ctargs functions
    if (targetFunc->isCtargs) {
        checkAndGenerate(targetFunc->name);
        std::vector<RaveValue> params = Call::genParameters(arguments, byVals, targetFunc, loc);
        std::string sTypes = typesToString(types);

        if (generator->functions.find(targetFunc->name + sTypes) != generator->functions.end())
            return LLVM::call(generator->functions[targetFunc->name + sTypes], params,
                (instanceof<TypeVoid>(targetFunc->type) ? "" : "callFunc"), byVals);
        return LLVM::call(generator->functions[targetFunc->generateWithCtargs(Call::getParamsTypes(params))], params,
            (instanceof<TypeVoid>(targetFunc->type) ? "" : "callFunc"));
    }

    // Handle template functions
    if (!targetFunc->templateNames.empty()) {
        std::string sTypes = Template::fromTypes(types);
        if (targetFunc->args.size() != arguments.size() && !targetFunc->isCdecl64 && !targetFunc->isWin64 && !targetFunc->isVararg)
            generator->error("wrong number of arguments for calling function \033[1m" + name + sTypes + "\033[22m!", loc);

        FuncSignature specializedSig(name + sTypes, types);
        NodeFunc* specialized = FuncRegistry::instance().findBySignature(specializedSig);

        if (specialized != nullptr) {
            checkAndGenerate(specialized->name);
            std::vector<RaveValue> params = Call::genParameters(arguments, byVals, specialized, loc);
            return LLVM::call(generator->functions[specialized->name], params,
                (instanceof<TypeVoid>(specialized->type) ? "" : "callFunc"), byVals);
        }

        size_t tnSize = targetFunc->templateNames.size();
        std::vector<RaveValue> params = Call::genParameters(arguments, byVals, targetFunc, loc);
        types = Call::getParamsTypes(params);

        while (types.size() > tnSize) types.pop_back();
        sTypes = Template::fromTypes(types);

        if (types.size() == tnSize)
            return LLVM::call(targetFunc->generateWithTemplate(types, name + sTypes), params,
                (instanceof<TypeVoid>(targetFunc->type) ? "" : "callFunc"), byVals);
        else generator->error("wrong number of template types for calling function \033[1m" + name + "\033[22m!", loc);
    }

    // Handle arrayable flag
    if (targetFunc->isArrayable && arguments.size() > 0) {
        std::vector<Node*> newArgs;
        bool isChanged = false;
        for (size_t i = 0; i < arguments.size(); i++) {
            if (instanceof<TypeArray>(arguments[i]->getType())) {
                isChanged = true;
                newArgs.insert(newArgs.end(), {new NodeUnary(loc, TokType::Amp, arguments[i]),
                    ((TypeArray*)arguments[i]->getType())->count->comptime()});
            }
            else newArgs.push_back(arguments[i]);
        }
        if (isChanged) {
            arguments = newArgs;
            types = Call::getTypes(newArgs);
            FuncSignature newSig(name, types);
            NodeFunc* newTarget = FuncRegistry::instance().findBestMatch(newSig);
            if (newTarget != nullptr) targetFunc = newTarget;
        }
    }

    // Check argument count
    if (!targetFunc->isCdecl64 && !targetFunc->isWin64 && !targetFunc->isVararg &&
        targetFunc->args.size() != arguments.size())
        generator->error("wrong number of arguments for calling function \033[1m" + name + "\033[22m!", loc);

    checkAndGenerate(targetFunc->name);
    std::vector<RaveValue> params = Call::genParameters(arguments, byVals, targetFunc, loc);
    return LLVM::call(generator->functions[targetFunc->name], params,
        (instanceof<TypeVoid>(targetFunc->type) ? "" : "callFunc"), byVals);
}

RaveValue Call::make(int loc, Node* function, std::vector<Node*> arguments) {
    DEBUG_LOG(Debug::Category::FuncCall, "Call::make - resolving call");

    if (instanceof<NodeIden>(function)) {
        std::string& name = ((NodeIden*)function)->name;

        Node* alias = Call::resolveAlias(name);
        if (alias != nullptr) return Call::make(loc, alias, arguments);

        if (AST::funcTable.find(name) != AST::funcTable.end())
            return Call::callNamedFunction(loc, name, arguments);

        if (currScope->has(name))
            return Call::callVarFunction(loc, name, arguments);

        if (name.find('<') != std::string::npos)
            return Call::callTemplateFunction(loc, name, arguments);

        if (currScope->has("this")) {
            // Check for method on this
            std::vector<int> byVals;
            NodeVar* _this = currScope->getVar("this", loc);
            if (instanceof<TypeStruct>(_this->type->getElType())) {
                TypeStruct* _struct = (TypeStruct*)_this->type->getElType();
                if (AST::structTable.find(_struct->name) != AST::structTable.end()) {
                    for (auto& it : AST::structTable[_struct->name]->variables) {
                        if (it->name == name && instanceof<TypeFunc>(it->type)) {
                            std::vector<FuncArgSet> fas = tfaToFas(((TypeFunc*)it->type)->args);
                            std::vector<RaveValue> params = Call::genParameters(arguments, byVals, fas,
                                CallSettings{false, ((TypeFunc*)it->type)->isVarArg, loc});
                            return LLVM::call(currScope->getWithoutLoad(it->name, loc), params,
                                (instanceof<TypeVoid>(it->type) ? "" : "callFunc"), byVals);
                        }
                    }
                }
                arguments.insert(arguments.begin(), new NodeIden("this", loc));
                auto methodf = Call::findMethod(_struct->name, name, arguments, loc);
                std::vector<RaveValue> params = Call::genParameters(arguments, byVals, methodf, loc);
                if (instanceof<TypePointer>(params[0].type->getElType()))
                    params[0] = LLVM::load(params[0], "NodeCall_load", loc);
                return LLVM::call(generator->functions[methodf->name], params,
                    (instanceof<TypeVoid>(methodf->type) ? "" : "callFunc"), byVals);
            }
        }

        if (AST::structTable.find(name) != AST::structTable.end()) {
            AST::structTable[name]->generate();
            if (AST::funcTable.find(name) == AST::funcTable.end())
                generator->error("undefined function \033[1m" + name + "\033[22m!", loc);
            return Call::make(loc, function, arguments);
        }

        generator->error("undefined function \033[1m" + name + "\033[22m!", loc);
    }

    if (instanceof<NodeGet>(function))
        return Call::callMethodOnGet(loc, (NodeGet*)function, arguments);

    if (instanceof<NodeUnary>(function)) {
        ((NodeUnary*)function)->base = new NodeDone(Call::make(loc, ((NodeUnary*)function)->base, arguments));
        return ((NodeUnary*)function)->generate();
    }

    if (instanceof<NodeFunc>(function))
        return Call::make(loc, new NodeIden(((NodeFunc*)function)->name, loc), arguments);

    generator->error("a call of this kind is unavailable!", loc);
    return {nullptr, nullptr};
}

Type* NodeCall::__getType(Node* _fn) {
    if (instanceof<NodeIden>(_fn)) {
        NodeIden* niden = (NodeIden*)_fn;

        if (AST::aliasTable.find(niden->name) != AST::aliasTable.end()) return __getType(AST::aliasTable[niden->name]);
        else if (currScope != nullptr && currScope->aliasTable.find(niden->name) != currScope->aliasTable.end())
            return __getType(currScope->aliasTable[niden->name]);

        std::vector<Type*> types = Call::getTypes(args);

        if (AST::funcTable.find((niden->name + typesToString(types))) != AST::funcTable.end())
            return AST::funcTable[(((NodeIden*)_fn)->name + typesToString(types))]->getType();
        if (AST::funcTable.find(niden->name) != AST::funcTable.end())
            return AST::funcTable[((NodeIden*)_fn)->name]->type;

        if (currScope->has(niden->name)) {
            if (!instanceof<TypeFunc>(currScope->getVar(niden->name, this->loc)->type)) {
                generator->error("undefined function \033[1m" + niden->name + "\033[22m!", this->loc);
                return nullptr;
            }
            TypeFunc* fn = (TypeFunc*)currScope->getVar(niden->name, this->loc)->type;
            return fn->main;
        }
        return new TypePointer(typeVoid);
    }
    return _fn->getType();
}

Type* NodeCall::getType() { return __getType(this->func); }
RaveValue NodeCall::generate() { return Call::make(loc, func, args); }
void NodeCall::check() { isChecked = true; }
Node* NodeCall::comptime() { return this; }
Node* NodeCall::copy() { return new NodeCall(this->loc, this->func, this->args); }