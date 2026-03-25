/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

// Function and method resolution for Call operations
// Split from NodeCall.cpp for better maintainability

#include "../../include/parser/nodes/NodeCall.hpp"
#include "../../include/parser/nodes/NodeIden.hpp"
#include "../../include/parser/nodes/NodeFunc.hpp"
#include "../../include/parser/nodes/NodeVar.hpp"
#include "../../include/parser/nodes/NodeGet.hpp"
#include "../../include/parser/nodes/NodeIndex.hpp"
#include "../../include/parser/nodes/NodeUnary.hpp"
#include "../../include/parser/nodes/NodeArray.hpp"
#include "../../include/parser/nodes/NodeDone.hpp"
#include "../../include/parser/nodes/NodeStruct.hpp"
#include "../../include/parser/ast.hpp"
#include "../../include/parser/FuncRegistry.hpp"
#include "../../include/parser/TypeMatching.hpp"
#include "../../include/debug.hpp"

// Helper: check if arguments match types
bool hasIdenticallyArgs(const std::vector<Type*>& one, const std::vector<FuncArgSet>& two, NodeFunc* fn) {
    if (one.size() != two.size()) return false;

    for (size_t i = 0; i < one.size(); i++) {
        Type* t1 = one[i];
        Type* t2 = two[i].type;

        if (instanceof<TypeStruct>(t2) && !instanceof<TypeStruct>(t1)) {
            if (fn->isExplicit) return false;
            std::string name = t1->toString();
            if (AST::structTable.find(name) != AST::structTable.end()) {
                auto& operators = AST::structTable[name]->operators;
                if (operators.find(TokType::Equ) != operators.end()) {
                    Type* ty = operators[TokType::Equ].begin()->second->args[1].type;
                    if ((isBytePointer(ty) && isBytePointer(t1)) || ty->toString() == t1->toString()) continue;
                }
            }
        }

        if (instanceof<TypeBasic>(t1) && instanceof<TypeBasic>(t2)) {
            auto* tb1 = (TypeBasic*)t1;
            auto* tb2 = (TypeBasic*)t2;
            if (tb1->type != tb2->type && (isFloatType(tb1) || isFloatType(tb2))) return false;
            continue;
        }

        if (t1 == nullptr || t2 == nullptr || t1->toString() != t2->toString()) {
            if (t1 != nullptr && t2 != nullptr) {
                t1 = Types::stripConst(t1);
                t2 = Types::stripConst(t2);
                if (instanceof<TypeStruct>(t1) && instanceof<TypePointer>(t2)) continue;
                if (instanceof<TypeStruct>(t2) && instanceof<TypePointer>(t1)) continue;
            }
            return false;
        }
    }
    return true;
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

NodeFunc* Call::findMethod(std::string structName, std::string methodName, std::vector<Node*>& arguments, int loc) {
    DEBUG_LOG(Debug::Category::FuncCall, "Finding method: " + structName + "." + methodName);

    std::string templateTypes = "";

    if (methodName.find('<') != std::string::npos) {
        templateTypes = methodName.substr(methodName.find('<') + 1);
        templateTypes.pop_back();
        methodName = methodName.substr(0, methodName.find('<'));
    }

    std::vector<Type*> argTypes = Call::getTypes(arguments);
    FuncSignature sig(methodName, argTypes, structName);

    // Try FuncRegistry with signature
    NodeFunc* methodFunc = FuncRegistry::instance().findBestMatch(sig);

    if (methodFunc != nullptr && !methodFunc->templateNames.empty() && !methodFunc->isTemplate) {
        methodFunc = nullptr;
    }

    // Fallback to legacy lookup
    if (methodFunc == nullptr) {
        auto method = std::pair<std::string, std::string>(structName, methodName);
        auto methodf = AST::methodTable.find(method);

        if (methodf == AST::methodTable.end() || !hasIdenticallyArgs(argTypes, methodf->second->args, methodf->second)) {
            method.second += typesToString(argTypes);
            methodf = AST::methodTable.find(method);
        }

        if (methodf == AST::methodTable.end()) {
            method.second = method.second.substr(0, method.second.find('['));
            methodf = AST::methodTable.find(method);
        }

        if (methodf == AST::methodTable.end() || methodf->second->args.size() != arguments.size()) {
            for (auto& it : AST::methodTable) {
                if (it.first.first != structName || it.second->args.size() != arguments.size()) continue;
                if (it.first.second != methodName) {
                    if (it.first.second.find(methodName + "[") != 0 && it.first.second.find(methodName + "<") != 0) continue;
                }
                method.second = it.first.second;
                methodf = AST::methodTable.find(method);
                break;
            }
        }

        if (methodf == AST::methodTable.end())
            generator->error("undefined method \033[1m" + methodName + "\033[22m of the structure \033[1m" + structName + "\033[22m!", loc);

        methodFunc = methodf->second;
    }

    if (templateTypes.length() > 0) {
        if (generator->functions.find(methodFunc->name + "<" + templateTypes + ">") == generator->functions.end()) {
            std::vector<Type*> types = Template::parseTemplateTypes(templateTypes);
            templateTypes = "<";
            for (size_t i = 0; i < types.size(); i++) {
                Types::replaceTemplates(&types[i]);
                templateTypes += types[i]->toString() + ",";
            }
            templateTypes.back() = '>';
            methodFunc->generateWithTemplate(types, methodName + templateTypes);
            FuncSignature newSig(methodName + templateTypes, argTypes, structName);
            NodeFunc* newMethod = FuncRegistry::instance().findBestMatch(newSig);
            if (newMethod != nullptr) methodFunc = newMethod;
        }
    }
    else if (generator->functions.find(methodFunc->name) == generator->functions.end()) {
        methodFunc->generate();
    }

    return methodFunc;
}

NodeVar* Call::findVarFunction(std::string structName, std::string varName) {
    auto var = std::pair<std::string, std::string>(structName, varName);
    return (AST::structMembersTable.find(var) != AST::structMembersTable.end())
        ? AST::structMembersTable[var].var : nullptr;
}

RaveValue Call::callVarFunction(int loc, const std::string& name, std::vector<Node*>& arguments) {
    DEBUG_LOG(Debug::Category::FuncCall, "Calling variable function: " + name);

    std::vector<int> byVals;
    if (!instanceof<TypeFunc>(currScope->getVar(name, loc)->type))
        generator->error("undefined function \033[1m" + name + "\033[22m!", loc);

    TypeFunc* fn = (TypeFunc*)currScope->getVar(name, loc)->type;
    std::vector<FuncArgSet> fas = tfaToFas(fn->args);
    std::vector<RaveValue> params = Call::genParameters(arguments, byVals, fas, CallSettings{false, fn->isVarArg, loc});
    return LLVM::call(currScope->get(name), params, (instanceof<TypeVoid>(fn->main) ? "" : "callFunc"), byVals);
}

RaveValue Call::callMethodOnGet(int loc, NodeGet* getFunc, std::vector<Node*>& arguments) {
    DEBUG_LOG(Debug::Category::FuncCall, "Calling method via NodeGet");

    std::vector<int> byVals;

    if (instanceof<NodeIden>(getFunc->base)) {
        std::string& ifName = ((NodeIden*)getFunc->base)->name;
        if (!currScope->has(ifName) && !currScope->hasAtThis(ifName))
            generator->error("undefined variable \033[1m" + ifName + "\033[22m!", loc);

        NodeVar* variable = currScope->getVar(ifName, loc);
        variable->isUsed = true;

        TypeStruct* structure = nullptr;
        if (instanceof<TypeStruct>(variable->type)) structure = (TypeStruct*)(variable->type);
        else if (instanceof<TypePointer>(variable->type) && instanceof<TypeStruct>(variable->type->getElType()))
            structure = (TypeStruct*)(variable->type->getElType());
        else generator->error("type of the variable \033[1m" + ifName + "\033[22m is not a structure!", loc);

        if (AST::structTable.find(structure->name) != AST::structTable.end()) {
            NodeVar* var = Call::findVarFunction(structure->name, getFunc->field);
            if (var != nullptr) {
                std::vector<FuncArgSet> fas = tfaToFas(((TypeFunc*)var->type)->args);
                std::vector<RaveValue> params = Call::genParameters(arguments, byVals, fas, CallSettings{false, ((TypeFunc*)var->type)->isVarArg, loc});
                return LLVM::call((new NodeGet(getFunc->base, var->name, false, loc))->generate(), params,
                    (instanceof<TypeVoid>(var->type->getElType()) ? "" : "callFunc"), byVals);
            }

            arguments.insert(arguments.begin(), getFunc->base);
            auto methodf = Call::findMethod(structure->name, getFunc->field, arguments, loc);
            std::vector<RaveValue> params = Call::genParameters(arguments, byVals, methodf, loc);
            if (instanceof<TypePointer>(params[0].type->getElType()))
                params[0] = LLVM::load(params[0], "NodeCall_load", loc);
            return LLVM::call(generator->functions[methodf->name], params,
                (instanceof<TypeVoid>(methodf->type) ? "" : "callFunc"), byVals);
        }
        generator->error("structure \033[1m" + structure->name + "\033[22m does not exist!", loc);
    }

    RaveValue value;
    Type* structure = nullptr;

    if (instanceof<NodeDone>(getFunc->base)) {
        value = ((NodeDone*)getFunc->base)->value;
        structure = value.type;
    }
    else if (instanceof<NodeCall>(getFunc->base)) {
        return Call::make(loc, new NodeGet(new NodeDone(((NodeCall*)getFunc->base)->generate()),
            getFunc->field, getFunc->isMustBePtr, loc), arguments);
    }
    else if (instanceof<NodeGet>(getFunc->base)) {
        ((NodeGet*)getFunc->base)->isMustBePtr = true;
        value = ((NodeGet*)getFunc->base)->generate();
        structure = value.type;
    }
    else if (instanceof<NodeIndex>(getFunc->base)) {
        ((NodeIndex*)getFunc->base)->isMustBePtr = true;
        return Call::make(loc, new NodeGet(new NodeDone(((NodeIndex*)getFunc->base)->generate()),
            getFunc->field, getFunc->isMustBePtr, loc), arguments);
    }
    else {
        generator->error("a call of this kind is unavailable!", loc);
    }

    while (instanceof<TypePointer>(structure)) structure = structure->getElType();

    if (AST::structTable.find(structure->toString()) != AST::structTable.end()) {
        arguments.insert(arguments.begin(), getFunc->base);
        auto methodf = Call::findMethod(structure->toString(), getFunc->field, arguments, loc);
        std::vector<RaveValue> params = Call::genParameters(arguments, byVals, methodf, loc);
        if (instanceof<TypePointer>(params[0].type->getElType()))
            params[0] = LLVM::load(params[0], "NodeCall_load", loc);
        return LLVM::call(generator->functions[methodf->name], params,
            (instanceof<TypeVoid>(methodf->type) ? "" : "callFunc"), byVals);
    }

    generator->error("structure \033[1m" + structure->toString() + "\033[22m does not exist!", loc);
    return {nullptr, nullptr};
}