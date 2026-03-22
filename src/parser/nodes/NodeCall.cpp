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
#include "../../include/parser/nodes/NodeInt.hpp"
#include "../../include/parser/nodes/NodeType.hpp"
#include "../../include/parser/Types.hpp"
#include "../../include/parser/ast.hpp"
#include "../../include/parser/FuncRegistry.hpp"
#include "../../include/parser/TypeMatching.hpp"
#include "../../include/lexer/lexer.hpp"
#include "../../include/llvm.hpp"

NodeCall::NodeCall(int loc, Node* func, std::vector<Node*> args)
    : loc(loc), func(func), args(args) {}

NodeCall::~NodeCall() {
    delete func;
    for (Node* arg : args) delete arg;
}

inline void checkAndGenerate(std::string name) {
    if (generator->functions.find(name) == generator->functions.end()) {
        if (AST::funcTable.find(name) == AST::funcTable.end()) {
            return;
        }
        AST::funcTable[name]->generate();
    }
}

bool hasIdenticallyArgs(const std::vector<Type*>& one, const std::vector<FuncArgSet>& two, NodeFunc* fn) {
    if (one.size() != two.size()) return false;

    for (size_t i=0; i<one.size(); i++) {
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
                while (instanceof<TypeConst>(t1)) t1 = t1->getElType();
                while (instanceof<TypeConst>(t2)) t2 = t2->getElType();
                if (instanceof<TypeStruct>(t1) && instanceof<TypePointer>(t2)) continue;
                if (instanceof<TypeStruct>(t2) && instanceof<TypePointer>(t1)) continue;
            }
            return false;
        }
    }
    return true;
}

std::vector<FuncArgSet> tfaToFas(std::vector<TypeFuncArg*> tfa) {
    std::vector<FuncArgSet> fas;
    for (auto* arg : tfa) {
        Type* type = arg->type;
        while (generator->toReplace.find(type->toString()) != generator->toReplace.end())
            type = generator->toReplace[type->toString()];
        fas.push_back(FuncArgSet{.name = arg->name, .type = type});
    }
    return fas;
}

RaveValue checkThis(int loc, const std::string& ifName, std::vector<Node*> arguments, std::vector<int> byVals) {
    NodeVar* _this = currScope->getVar("this", loc);
    if (instanceof<TypeStruct>(_this->type->getElType())) {
        TypeStruct* _struct = (TypeStruct*)_this->type->getElType();

        if (AST::structTable.find(_struct->name) != AST::structTable.end()) {
            for (auto& it : AST::structTable[_struct->name]->variables) {
                if (it->name == ifName && instanceof<TypeFunc>(it->type)) {
                    std::vector<FuncArgSet> fas = tfaToFas(((TypeFunc*)it->type)->args);
                    std::vector<RaveValue> params = Call::genParameters(arguments, byVals, fas, CallSettings{false, ((TypeFunc*)it->type)->isVarArg, loc});
                    return LLVM::call(currScope->getWithoutLoad(it->name), params, (instanceof<TypeVoid>(it->type) ? "" : "callFunc"), byVals);
                }
            }
        }

        arguments.insert(arguments.begin(), new NodeIden("this", loc));
        auto methodf = Call::findMethod(_struct->name, ifName, arguments, loc);
        std::vector<RaveValue> params = Call::genParameters(arguments, byVals, methodf, loc);
        if (instanceof<TypePointer>(params[0].type->getElType())) params[0] = LLVM::load(params[0], "NodeCall_load", loc);
        return LLVM::call(generator->functions[methodf->name], params, (instanceof<TypeVoid>(methodf->type) ? "" : "callFunc"), byVals);
    }
    return {nullptr, nullptr};
}

std::vector<Type*> Call::getTypes(std::vector<Node*>& arguments) {
    std::vector<Type*> array;
    for (size_t i=0; i<arguments.size(); i++) {
        Type* t = arguments[i]->getType();
        Types::replaceTemplates(&t);
        array.push_back(t);
    }
    return array;
}

std::vector<Type*> Call::getParamsTypes(std::vector<RaveValue>& params) {
    std::vector<Type*> array;
    for (size_t i=0; i<params.size(); i++) array.push_back(params[i].type);
    return array;
}

std::vector<RaveValue> Call::genParameters(std::vector<Node*>& arguments, std::vector<int>& byVals, std::vector<FuncArgSet>& fas, CallSettings settings) {
    std::vector<RaveValue> params;

    if (arguments.size() != fas.size()) {
        for (size_t i=0; i<arguments.size(); i++) {
            if (instanceof<NodeIden>(arguments[i])) ((NodeIden*)arguments[i])->isMustBePtr = false;
            else if (instanceof<NodeIndex>(arguments[i])) ((NodeIndex*)arguments[i])->isMustBePtr = false;
            else if (instanceof<NodeGet>(arguments[i])) ((NodeGet*)arguments[i])->isMustBePtr = false;
            params.push_back(arguments[i]->generate());
        }
        return params;
    }

    for (size_t i=0; i<arguments.size(); i++) {
        if (instanceof<TypePointer>(fas[i].type) && instanceof<TypeStruct>(fas[i].type->getElType()) && !instanceof<TypePointer>(arguments[i]->getType())) {
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

    for (size_t i=0; i<params.size(); i++) {
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
            if (instanceof<TypeStruct>(fas[i].type->getElType()) && !instanceof<TypePointer>(params[i].type)) LLVM::makeAsPointer(params[i]);
            else if (isBytePointer(fas[i].type) && fas[i].type->toString() != params[i].type->toString()) LLVM::cast(params[i], new TypePointer(basicTypes[BasicType::Char]));
        }
        else {
            while (instanceof<TypePointer>(params[i].type))
                params[i] = LLVM::load(params[i], "genParameters_load", settings.loc);

            if (instanceof<TypeBasic>(fas[i].type)) {
                if (instanceof<TypeBasic>(params[i].type) && ((TypeBasic*)fas[i].type) != ((TypeBasic*)params[i].type)) LLVM::cast(params[i], fas[i].type);
            }
        }

        if (settings.isCW64) {
            if (instanceof<TypeDivided>(fas[i].internalTypes[0])) {
                if (!instanceof<TypePointer>(params[i].type)) {
                    RaveValue temp = LLVM::alloc(params[i].type, "getParameters_stotd_temp");
                    LLVMBuildStore(generator->builder, params[i].value, temp.value);
                    params[i].value = temp.value;
                }
                params[i].value = LLVMBuildLoad2(generator->builder, generator->genType(((TypeDivided*)fas[i].internalTypes[0])->mainType, settings.loc), params[i].value, "getParameters_stotd");
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
                    params[i].value = LLVMBuildPointerCast(generator->builder, params[i].value, generator->genType(fas[i].type, settings.loc), "castVFtoCF");
                    TypeFunc* tfunc = ((TypeFunc*)params[i].type->getElType());
                    tfunc->main = basicTypes[BasicType::Char];
                    ((TypePointer*)params[i].type)->instance = tfunc;
                }
            }
        }
    }

    return params;
}

inline std::vector<RaveValue> Call::genParameters(std::vector<Node*>& arguments, std::vector<int>& byVals, NodeFunc* function, int loc) {
    return Call::genParameters(arguments, byVals, function->args, CallSettings{function->isCdecl64 || function->isWin64, function->isVararg, loc});
}

std::string Template::fromTypes(std::vector<Type*>& types) {
    std::string sTypes = "<";
    for (size_t i=0; i<types.size(); i++) sTypes += types[i]->toString() + ",";
    sTypes.back() = '>';
    return sTypes;
}

std::vector<Type*> Template::parseTemplateTypes(const std::string& templateStr) {
    Lexer tLexer(templateStr, 1);
    Parser tParser(tLexer.tokens, "(builtin)");
    std::vector<Type*> types;

    while (tParser.peek()->type != TokType::Eof) {
        switch (tParser.peek()->type) {
            case TokType::Number: case TokType::HexNumber: case TokType::FloatNumber: {
                Node* value = tParser.parseExpr();
                types.push_back(new TypeTemplateMember(value->getType(), value));
                break;
            }
            default:
                types.push_back(tParser.parseType(true));
                break;
        }
        if (tParser.peek()->type == TokType::Comma) tParser.next();
    }

    for (size_t i=0; i<types.size(); i++) Types::replaceTemplates(&types[i]);
    return types;
}

Node* Call::resolveAlias(const std::string& name) {
    if (AST::aliasTable.find(name) != AST::aliasTable.end()) return AST::aliasTable[name];
    if (currScope != nullptr && currScope->aliasTable.find(name) != currScope->aliasTable.end()) return currScope->aliasTable[name];
    return nullptr;
}

NodeVar* Call::findVarFunction(std::string structName, std::string varName) {
    auto var = std::pair<std::string, std::string>(structName, varName);
    return (AST::structMembersTable.find(var) != AST::structMembersTable.end()) ? AST::structMembersTable[var].var : nullptr;
}

NodeFunc* Call::findMethod(std::string structName, std::string methodName, std::vector<Node*>& arguments, int loc) {
    std::string templateTypes = "";

    if (methodName.find('<') != std::string::npos) {
        templateTypes = methodName.substr(methodName.find('<') + 1);
        templateTypes.pop_back();
        methodName = methodName.substr(0, methodName.find('<'));
    }

    // Get argument types for signature matching
    std::vector<Type*> argTypes = Call::getTypes(arguments);

    // Create signature for method lookup
    FuncSignature sig(methodName, argTypes, structName);

    // Try FuncRegistry with signature
    NodeFunc* methodFunc = FuncRegistry::instance().findBestMatch(sig);

    // If found but it's a template base (not a specialization), we need to look for a specialization
    if (methodFunc != nullptr && !methodFunc->templateNames.empty() && !methodFunc->isTemplate) {
        methodFunc = nullptr;  // Reset to try legacy lookup
    }

    // Fallback to legacy lookup if not found
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
            for (size_t i=0; i<types.size(); i++) {
                Types::replaceTemplates(&types[i]);
                templateTypes += types[i]->toString() + ",";
            }
            templateTypes.back() = '>';
            methodFunc->generateWithTemplate(types, methodName + templateTypes);
            // Find the newly generated method
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

RaveValue Call::callVarFunction(int loc, const std::string& name, std::vector<Node*>& arguments) {
    std::vector<int> byVals;
    if (!instanceof<TypeFunc>(currScope->getVar(name, loc)->type))
        generator->error("undefined function \033[1m" + name + "\033[22m!", loc);

    TypeFunc* fn = (TypeFunc*)currScope->getVar(name, loc)->type;
    std::vector<FuncArgSet> fas = tfaToFas(fn->args);
    std::vector<RaveValue> params = Call::genParameters(arguments, byVals, fas, CallSettings{false, fn->isVarArg, loc});
    return LLVM::call(currScope->get(name), params, (instanceof<TypeVoid>(fn->main) ? "" : "callFunc"), byVals);
}

RaveValue Call::callTemplateFunction(int loc, const std::string& name, std::vector<Node*>& arguments) {
    std::vector<int> byVals;
    std::string mainName = name.substr(0, name.find('<'));
    std::string sTypes = name.substr(name.find('<') + 1, name.find('>') - name.find('<') - 1);

    std::vector<Type*> pTypes = Call::getTypes(arguments);
    std::string callTypes = typesToString(pTypes);

    if (AST::funcTable.find(name + callTypes) != AST::funcTable.end()) {
        std::vector<RaveValue> params = Call::genParameters(arguments, byVals, AST::funcTable[name + callTypes], loc);
        return LLVM::call(generator->functions[name + callTypes], params, (instanceof<TypeVoid>(AST::funcTable[name + callTypes]->type) ? "" : "callFunc"), byVals);
    }

    bool presenceInFt = AST::funcTable.find(mainName) != AST::funcTable.end();

    if (presenceInFt) {
        if (AST::funcTable.find(mainName + callTypes) != AST::funcTable.end()) {
            if (generator->functions.find(name + callTypes) != generator->functions.end()) {
                std::vector<RaveValue> params = Call::genParameters(arguments, byVals, AST::funcTable[name + callTypes], loc);
                return LLVM::call(generator->functions[name + callTypes], params, (instanceof<TypeVoid>(AST::funcTable[name + callTypes]->type) ? "" : "callFunc"), byVals);
            }
            mainName += callTypes;
        }
        else if ((AST::funcTable[mainName]->args.size() != arguments.size()) && !AST::funcTable[mainName]->isCdecl64 && !AST::funcTable[mainName]->isWin64 && !AST::funcTable[mainName]->isVararg)
            generator->error("wrong number of arguments for calling function \033[1m" + name + "\033[22m (\033[1m" + std::to_string(AST::funcTable[mainName]->args.size()) + "\033[22m expected, \033[1m" + std::to_string(arguments.size()) + "\033[22m provided)!", loc);
    }

    std::vector<Type*> types = Template::parseTemplateTypes(sTypes);

    if (presenceInFt) {
        sTypes = "<";
        for (size_t i=0; i<types.size(); i++) sTypes += types[i]->toString() + ",";
        sTypes = sTypes.substr(0, sTypes.length() - 1) + ">";

        if (AST::funcTable.find(mainName + sTypes) != AST::funcTable.end()) return Call::make(loc, new NodeIden(mainName + sTypes, loc), arguments);

        std::string sTypes2 = sTypes + typesToString(types);
        if (AST::funcTable.find(mainName + sTypes2) != AST::funcTable.end()) return Call::make(loc, new NodeIden(mainName + sTypes2, loc), arguments);

        AST::funcTable[mainName]->generateWithTemplate(types, mainName + sTypes + (mainName.find('[') == std::string::npos ? callTypes : ""));
    }
    else {
        sTypes = "<";
        for (size_t i=0; i<types.size(); i++) sTypes += types[i]->toString() + ",";
        sTypes.back() = '>';

        std::string ifName = mainName + sTypes;
        mainName = ifName.substr(0, ifName.find('<'));

        if (AST::funcTable.find(mainName) == AST::funcTable.end()) {
            if (AST::structTable.find(mainName) != AST::structTable.end()) AST::structTable[mainName]->genWithTemplate(sTypes, types);
            else if (currScope->has("this")) {
                RaveValue result = checkThis(loc, ifName, arguments, byVals);
                if (result.type != nullptr && result.value != nullptr) return result;
            }
            else generator->error("undefined structure \033[1m" + ifName + "\033[22m!", loc);
        }

        return Call::make(loc, AST::funcTable[ifName], arguments);
    }

    return Call::make(loc, new NodeIden(name, loc), arguments);
}

RaveValue Call::callNamedFunction(int loc, const std::string& name, std::vector<Node*>& arguments) {
    std::vector<int> byVals;

    // Get argument types for signature matching
    std::vector<Type*> types = Call::getTypes(arguments);

    // First, check if there's a vararg/cdecl64/win64 function by base name
    // These functions accept any number of args >= declared params
    NodeFunc* targetFunc = nullptr;
    auto overloads = FuncRegistry::instance().findAllOverloads(name);
    for (auto* func : overloads) {
        if ((func->isVararg || func->isCdecl64 || func->isWin64) && func->args.size() <= arguments.size()) {
            targetFunc = func;
            break;
        }
    }

    // Also check legacy funcTable for vararg functions
    if (targetFunc == nullptr && AST::funcTable.find(name) != AST::funcTable.end()) {
        NodeFunc* func = AST::funcTable[name];
        if ((func->isVararg || func->isCdecl64 || func->isWin64) && func->args.size() <= arguments.size()) {
            targetFunc = func;
        }
    }

    // If not vararg, try signature matching
    if (targetFunc == nullptr) {
        FuncSignature sig(name, types);
        targetFunc = FuncRegistry::instance().findBestMatch(sig);
        // If found but it's a template base (not a specialization), skip it
        if (targetFunc != nullptr && !targetFunc->templateNames.empty() && !targetFunc->isTemplate) {
            targetFunc = nullptr;
        }
    }

    // If still not found, try by base name for other cases
    if (targetFunc == nullptr) {
        targetFunc = FuncRegistry::instance().findBestMatch(name, types);
        // If found but it's a template base (not a specialization), skip it
        if (targetFunc != nullptr && !targetFunc->templateNames.empty() && !targetFunc->isTemplate) {
            targetFunc = nullptr;
        }
    }

    // Fallback to legacy lookup if not found in registry
    if (targetFunc == nullptr && AST::funcTable.find(name) != AST::funcTable.end()) {
        targetFunc = AST::funcTable[name];
    }

    if (targetFunc == nullptr) {
        generator->error("undefined function \033[1m" + name + "\033[22m!", loc);
    }

    // Handle ctargs functions
    if (targetFunc->isCtargs) {
        checkAndGenerate(targetFunc->name);
        std::vector<RaveValue> params = Call::genParameters(arguments, byVals, targetFunc, loc);
        std::string sTypes = typesToString(types);

        if (generator->functions.find(targetFunc->name + sTypes) != generator->functions.end())
            return LLVM::call(generator->functions[targetFunc->name + sTypes], params, (instanceof<TypeVoid>(targetFunc->type) ? "" : "callFunc"), byVals);
        return LLVM::call(generator->functions[targetFunc->generateWithCtargs(Call::getParamsTypes(params))], params, (instanceof<TypeVoid>(targetFunc->type) ? "" : "callFunc"));
    }

    // Handle template functions
    if (!targetFunc->templateNames.empty()) {
        std::string sTypes = Template::fromTypes(types);
        if (targetFunc->args.size() != arguments.size() && !targetFunc->isCdecl64 && !targetFunc->isWin64 && !targetFunc->isVararg)
            generator->error("wrong number of arguments for calling function \033[1m" + name + sTypes + "\033[22m (\033[1m" + std::to_string(targetFunc->args.size()) + "\033[22m expected, \033[1m" + std::to_string(arguments.size()) + "\033[22m provided)!", loc);

        // Check if template specialization already exists
        FuncSignature specializedSig(name + sTypes, types);
        NodeFunc* specialized = FuncRegistry::instance().findBySignature(specializedSig);

        if (specialized != nullptr) {
            checkAndGenerate(specialized->name);
            std::vector<RaveValue> params = Call::genParameters(arguments, byVals, specialized, loc);
            return LLVM::call(generator->functions[specialized->name], params, (instanceof<TypeVoid>(specialized->type) ? "" : "callFunc"), byVals);
        }

        // Generate template specialization
        size_t tnSize = targetFunc->templateNames.size();
        std::vector<RaveValue> params = Call::genParameters(arguments, byVals, targetFunc, loc);
        types = Call::getParamsTypes(params);

        while (types.size() > tnSize) types.pop_back();
        sTypes = Template::fromTypes(types);

        if (types.size() == tnSize)
            return LLVM::call(targetFunc->generateWithTemplate(types, name + sTypes), params, (instanceof<TypeVoid>(targetFunc->type) ? "" : "callFunc"), byVals);
        else generator->error("wrong number of template types for calling function \033[1m" + name + "\033[22m (\033[1m" + std::to_string(tnSize) + "\033[22m expected, \033[1m" + std::to_string(types.size()) + "\033[22m provided)!", loc);
    }

    // Handle arrayable flag - transform arguments if needed
    if (targetFunc->isArrayable && arguments.size() > 0) {
        std::vector<Node*> newArgs;
        bool isChanged = false;
        for (size_t i=0; i<arguments.size(); i++) {
            if (instanceof<TypeArray>(arguments[i]->getType())) {
                isChanged = true;
                newArgs.insert(newArgs.end(), {new NodeUnary(loc, TokType::Amp, arguments[i]), ((TypeArray*)arguments[i]->getType())->count->comptime()});
            }
            else newArgs.push_back(arguments[i]);
        }
        if (isChanged) {
            arguments = newArgs;
            types = Call::getTypes(newArgs);
            // Re-resolve with new argument types
            FuncSignature newSig(name, types);
            NodeFunc* newTarget = FuncRegistry::instance().findBestMatch(newSig);
            if (newTarget != nullptr) targetFunc = newTarget;
        }
    }

    // Check argument count for regular functions
    if (!targetFunc->isCdecl64 && !targetFunc->isWin64 && !targetFunc->isVararg && targetFunc->args.size() != arguments.size())
        generator->error("wrong number of arguments for calling function \033[1m" + name + "\033[22m (\033[1m" + std::to_string(targetFunc->args.size()) + "\033[22m expected, \033[1m" + std::to_string(arguments.size()) + "\033[22m provided)!", loc);

    // Generate function call
    checkAndGenerate(targetFunc->name);
    if (generator->functions.find(targetFunc->name) == generator->functions.end()) {
        generator->error("internal error: function \033[1m" + targetFunc->name + "\033[22m not found in generator!", loc);
    }
    if (generator->functions[targetFunc->name].value == nullptr) {
        generator->error("internal error: function \033[1m" + targetFunc->name + "\033[22m has null LLVM value!", loc);
    }
    std::vector<RaveValue> params = Call::genParameters(arguments, byVals, targetFunc, loc);
    return LLVM::call(generator->functions[targetFunc->name], params, (instanceof<TypeVoid>(targetFunc->type) ? "" : "callFunc"), byVals);
}

RaveValue Call::callMethodOnGet(int loc, NodeGet* getFunc, std::vector<Node*>& arguments) {
    std::vector<int> byVals;

    if (instanceof<NodeIden>(getFunc->base)) {
        std::string &ifName = ((NodeIden*)getFunc->base)->name;
        if (!currScope->has(ifName) && !currScope->hasAtThis(ifName))
            generator->error("undefined variable \033[1m" + ifName + "\033[22m!", loc);

        NodeVar* variable = currScope->getVar(ifName, loc);
        variable->isUsed = true;

        TypeStruct* structure = nullptr;
        if (instanceof<TypeStruct>(variable->type)) structure = (TypeStruct*)(variable->type);
        else if (instanceof<TypePointer>(variable->type) && instanceof<TypeStruct>(variable->type->getElType())) structure = (TypeStruct*)(variable->type->getElType());
        else generator->error("type of the variable \033[1m" + ifName + "\033[22m is not a structure!", loc);

        if (AST::structTable.find(structure->name) != AST::structTable.end()) {
            NodeVar* var = Call::findVarFunction(structure->name, getFunc->field);
            if (var != nullptr) {
                std::vector<FuncArgSet> fas = tfaToFas(((TypeFunc*)var->type)->args);
                std::vector<RaveValue> params = Call::genParameters(arguments, byVals, fas, CallSettings{false, ((TypeFunc*)var->type)->isVarArg, loc});
                return LLVM::call((new NodeGet(getFunc->base, var->name, false, loc))->generate(), params, (instanceof<TypeVoid>(var->type->getElType()) ? "" : "callFunc"), byVals);
            }

            arguments.insert(arguments.begin(), getFunc->base);
            auto methodf = Call::findMethod(structure->name, getFunc->field, arguments, loc);
            if (generator->functions.find(methodf->name) == generator->functions.end()) {
                generator->error("internal error: method \033[1m" + methodf->name + "\033[22m not found in generator!", loc);
            }
            std::vector<RaveValue> params = Call::genParameters(arguments, byVals, methodf, loc);
            if (instanceof<TypePointer>(params[0].type->getElType())) params[0] = LLVM::load(params[0], "NodeCall_load", loc);
            return LLVM::call(generator->functions[methodf->name], params, (instanceof<TypeVoid>(methodf->type) ? "" : "callFunc"), byVals);
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
        return Call::make(loc, new NodeGet(new NodeDone(((NodeCall*)getFunc->base)->generate()), getFunc->field, getFunc->isMustBePtr, loc), arguments);
    }
    else if (instanceof<NodeGet>(getFunc->base)) {
        ((NodeGet*)getFunc->base)->isMustBePtr = true;
        value = ((NodeGet*)getFunc->base)->generate();
        structure = value.type;
    }
    else if (instanceof<NodeIndex>(getFunc->base)) {
        ((NodeIndex*)getFunc->base)->isMustBePtr = true;
        return Call::make(loc, new NodeGet(new NodeDone(((NodeIndex*)getFunc->base)->generate()), getFunc->field, getFunc->isMustBePtr, loc), arguments);
    }
    else {
        generator->error("a call of this kind (NodeGet + " + std::string(typeid(getFunc->base[0]).name()) + ") is unavailable!", loc);
    }

    while (instanceof<TypePointer>(structure)) structure = structure->getElType();

    if (AST::structTable.find(structure->toString()) != AST::structTable.end()) {
        arguments.insert(arguments.begin(), getFunc->base);
        auto methodf = Call::findMethod(structure->toString(), getFunc->field, arguments, loc);
        if (generator->functions.find(methodf->name) == generator->functions.end()) {
            generator->error("internal error: method \033[1m" + methodf->name + "\033[22m not found in generator!", loc);
        }
        std::vector<RaveValue> params = Call::genParameters(arguments, byVals, methodf, loc);
        if (instanceof<TypePointer>(params[0].type->getElType())) params[0] = LLVM::load(params[0], "NodeCall_load", loc);
        return LLVM::call(generator->functions[methodf->name], params, (instanceof<TypeVoid>(methodf->type) ? "" : "callFunc"), byVals);
    }

    generator->error("structure \033[1m" + structure->toString() + "\033[22m does not exist!", loc);
    return {nullptr, nullptr};
}

RaveValue Call::make(int loc, Node* function, std::vector<Node*> arguments) {
    std::vector<int> byVals;

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
            RaveValue result = checkThis(loc, name, arguments, byVals);
            if (result.type != nullptr && result.value != nullptr) return result;
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
        else if (currScope != nullptr && currScope->aliasTable.find(niden->name) != currScope->aliasTable.end()) return __getType(currScope->aliasTable[niden->name]);

        std::vector<Type*> types = Call::getTypes(args);

        if (AST::funcTable.find((niden->name + typesToString(types))) != AST::funcTable.end()) return AST::funcTable[(((NodeIden*)_fn)->name + typesToString(types))]->getType();
        if (AST::funcTable.find(niden->name) != AST::funcTable.end()) return AST::funcTable[((NodeIden*)_fn)->name]->type;

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

Type* NodeCall::getType() {
    return __getType(this->func);
}

RaveValue NodeCall::generate() {
    return Call::make(loc, func, args);
}

void NodeCall::check() {isChecked = true;}
Node* NodeCall::comptime() {return this;}
Node* NodeCall::copy() {return new NodeCall(this->loc, this->func, this->args);}

