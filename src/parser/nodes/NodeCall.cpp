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
#include "../../include/lexer/lexer.hpp"
#include "../../include/llvm.hpp"

NodeCall::NodeCall(int loc, Node* func, std::vector<Node*> args) {
    this->loc = loc;
    this->func = func;
    this->args = std::vector<Node*>(args);
}

NodeCall::~NodeCall() {
    if(func != nullptr) delete func;
    for(int i=0; i<args.size(); i++) if(args[i] != nullptr) delete args[i];
}

inline void checkAndGenerate(std::string name) {
    if(generator->functions.find(name) == generator->functions.end()) AST::funcTable[name]->generate();
}

bool hasIdenticallyArgs(const std::vector<Type*>& one, const std::vector<Type*>& two) {
    if(one.size() != two.size()) return false;

    for(size_t i = 0; i < one.size(); ++i) {
        Type* t1 = one[i];
        Type* t2 = two[i];
        
        if(instanceof<TypeBasic>(t1) && instanceof<TypeBasic>(t2)) {
            auto* tb1 = (TypeBasic*)t1;
            auto* tb2 = (TypeBasic*)t2;
            
            if(tb1->type != tb2->type) {
                if(isFloatType(tb1) || isFloatType(tb2)) return false;
            }

            continue;
        }

        if(t1->toString() != t2->toString()) return false;
    }

    return true;
}

bool hasIdenticallyArgs(const std::vector<Type*>& one, const std::vector<FuncArgSet>& two) {
    if(one.size() != two.size()) return false;

    for(size_t i = 0; i < one.size(); ++i) {
        Type* t1 = one[i];
        Type* t2 = two[i].type;
        
        if(instanceof<TypeBasic>(t1) && instanceof<TypeBasic>(t2)) {
            auto* tb1 = (TypeBasic*)t1;
            auto* tb2 = (TypeBasic*)t2;
            
            if(tb1->type != tb2->type) {
                if(isFloatType(tb1) || isFloatType(tb2)) return false;
            }

            continue;
        }

        if(t1 == nullptr || t2 == nullptr || t1->toString() != t2->toString()) {
            if(t1 != nullptr && t2 != nullptr) {
                while(instanceof<TypeConst>(t1)) t1 = t1->getElType();
                while(instanceof<TypeConst>(t2)) t2 = t2->getElType();

                if(instanceof<TypeStruct>(t1) && instanceof<TypePointer>(t2)) continue;
                if(instanceof<TypeStruct>(t2) && instanceof<TypePointer>(t1)) continue;
            }

            return false;
        }
    }

    return true;
}

std::vector<FuncArgSet> tfaToFas(std::vector<TypeFuncArg*> tfa) {
    std::vector<FuncArgSet> fas;
    for(int i=0; i<tfa.size(); i++) {
        Type* type = tfa[i]->type;
        while(generator->toReplace.find(type->toString()) != generator->toReplace.end()) type = generator->toReplace[type->toString()];

        fas.push_back(FuncArgSet{.name = tfa[i]->name, .type = type});
    }

    return fas;
}

Type* NodeCall::__getType(Node* _fn) {
    if(instanceof<NodeIden>(_fn)) {
        NodeIden* niden = (NodeIden*)_fn;

        if(AST::aliasTable.find(niden->name) != AST::aliasTable.end()) return __getType(AST::aliasTable[niden->name]);
        else if(currScope != nullptr && currScope->aliasTable.find(niden->name) != currScope->aliasTable.end()) return __getType(currScope->aliasTable[niden->name]);

        std::vector<Type*> types = Call::getTypes(args);

        if(AST::funcTable.find((niden->name + typesToString(types))) != AST::funcTable.end()) return AST::funcTable[(((NodeIden*)_fn)->name + typesToString(types))]->getType();
        if(AST::funcTable.find(niden->name) != AST::funcTable.end()) return AST::funcTable[((NodeIden*)_fn)->name]->type;
        if(currScope->has(niden->name)) {
            if(!instanceof<TypeFunc>(currScope->getVar(niden->name, this->loc)->type)) {
                generator->error("undefined function '" + niden->name + "'!", this->loc);
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

std::vector<Type*> Call::getTypes(std::vector<Node*>& arguments) {
    std::vector<Type*> array;

    for(int i=0; i<arguments.size(); i++) {
        Type* t = arguments[i]->getType();
        Template::replaceTemplates(&t);
        array.push_back(t);
    }
    
    return array;
}

std::vector<Type*> Call::getParamsTypes(std::vector<RaveValue>& params) {
    std::vector<Type*> array;
    for(int i=0; i<params.size(); i++) array.push_back(params[i].type);
    return array;
}

std::vector<RaveValue> Call::genParameters(std::vector<Node*>& arguments, std::vector<int>& byVals, std::vector<FuncArgSet>& fas, CallSettings settings) {
    std::vector<RaveValue> params;

    if(arguments.size() != fas.size()) {
        for(int i=0; i<arguments.size(); i++) {
            // If this is a NodeIden/NodeIndex/NodeGet - set the 'isMustBePtr' to false
            if(instanceof<NodeIden>(arguments[i])) ((NodeIden*)arguments[i])->isMustBePtr = false;
            else if(instanceof<NodeIndex>(arguments[i])) ((NodeIndex*)arguments[i])->isMustBePtr = false;
            else if(instanceof<NodeGet>(arguments[i])) ((NodeGet*)arguments[i])->isMustBePtr = false;
            params.push_back(arguments[i]->generate());
        }

        return params;
    }

    for(int i=0; i<arguments.size(); i++) {
        if(instanceof<TypePointer>(fas[i].type) && instanceof<TypeStruct>(fas[i].type->getElType()) && !instanceof<TypePointer>(arguments[i]->getType())) {
            if(instanceof<NodeIden>(arguments[i])) ((NodeIden*)arguments[i])->isMustBePtr = true;
            else if(instanceof<NodeGet>(arguments[i])) ((NodeGet*)arguments[i])->isMustBePtr = true;
            else if(instanceof<NodeIndex>(arguments[i])) ((NodeIndex*)arguments[i])->isMustBePtr = true;
        }

        if(instanceof<TypeStruct>(fas[i].type) && instanceof<TypePointer>(arguments[i]->getType())) {
            if(instanceof<NodeIden>(arguments[i])) ((NodeIden*)arguments[i])->isMustBePtr = false;
            else if(instanceof<NodeGet>(arguments[i])) ((NodeGet*)arguments[i])->isMustBePtr = false;
            else if(instanceof<NodeIndex>(arguments[i])) ((NodeIndex*)arguments[i])->isMustBePtr = false;
        }
        
        params.push_back(arguments[i]->generate());
    }

    for(int i=0; i<params.size(); i++) {
        if(instanceof<TypePointer>(fas[i].type)) {
            if(instanceof<TypeStruct>(fas[i].type->getElType()) && !instanceof<TypePointer>(params[i].type)) LLVM::makeAsPointer(params[i]);
            else if(isBytePointer(fas[i].type) && fas[i].type->toString() != params[i].type->toString()) LLVM::cast(params[i], new TypePointer(basicTypes[BasicType::Char]));
        }
        else {
            while(instanceof<TypePointer>(params[i].type)) {
                params[i] = LLVM::load(params[i], "genParameters_load", settings.loc);
            }

            if(instanceof<TypeBasic>(fas[i].type)) {
                if(instanceof<TypeBasic>(params[i].type) && ((TypeBasic*)fas[i].type) != ((TypeBasic*)params[i].type)) LLVM::cast(params[i], fas[i].type);
            }
        }

        if(settings.isCW64) {
            if(instanceof<TypeDivided>(fas[i].internalTypes[0])) {
                if(!instanceof<TypePointer>(params[i].type)) {
                    RaveValue temp = LLVM::alloc(params[i].type, "getParameters_stotd_temp");
                    LLVMBuildStore(generator->builder, params[i].value, temp.value);
                    params[i].value = temp.value;
                }

                params[i].value = LLVMBuildLoad2(generator->builder, generator->genType(((TypeDivided*)fas[i].internalTypes[0])->mainType, settings.loc), params[i].value, "getParameters_stotd");
            }
            else if(instanceof<TypeByval>(fas[i].internalTypes[0])) {
                if(!instanceof<TypePointer>(params[i].type)) LLVM::makeAsPointer(params[i]);

                byVals.push_back(i);
            }
        }
        else if(instanceof<TypeFunc>(fas[i].type)) {
            TypeFunc* tfunc = (TypeFunc*)(fas[i].type);
            if(instanceof<TypeBasic>(tfunc->main) && ((TypeBasic*)(tfunc->main))->type == BasicType::Char) {
                if(instanceof<TypePointer>(params[i].type) && instanceof<TypeFunc>(params[i].type->getElType())
                && instanceof<TypeVoid>(((TypeFunc*)params[i].type->getElType())->main)) {
                    // Casting void(...) to char(...)
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
    for(int i=0; i<types.size(); i++) sTypes += types[i]->toString() + ",";
    sTypes.back() = '>';
    return sTypes;
}

std::map<std::pair<std::string, std::string>, NodeFunc*>::iterator Call::findMethod(std::string structName, std::string methodName, std::vector<Node*>& arguments, int loc) {
    auto method = std::pair<std::string, std::string>(structName, methodName);
    auto methodf = AST::methodTable.find(method);

    if(methodf == AST::methodTable.end() || !hasIdenticallyArgs(Call::getTypes(arguments), methodf->second->args)) {method.second += typesToString(Call::getTypes(arguments)); methodf = AST::methodTable.find(method);}

    if(methodf == AST::methodTable.end()) {method.second = method.second.substr(0, method.second.find('[')); methodf = AST::methodTable.find(method);}

    if(methodf == AST::methodTable.end() || methodf->second->args.size() != arguments.size()) {
        // Choose the most right overload
        for(auto& it : AST::methodTable) {
            if(it.first.first != structName || it.second->args.size() != arguments.size()) continue;

            if(it.first.second != methodName) {
                if(it.first.second.find(methodName + "[") != 0 && it.first.second.find(methodName + "<") != 0) continue;
            }

            method.second = it.first.second;
            methodf = AST::methodTable.find(method);
            break;
        }
    }

    if(methodf == AST::methodTable.end()) generator->error("undefined method '" + methodName + "' of the structure '" + structName + "'!", loc);

    if(generator->functions.find(methodf->second->name) == generator->functions.end()) methodf->second->generate();

    return methodf;
}

RaveValue Call::make(int loc, Node* function, std::vector<Node*> arguments) {
    std::vector<int> byVals;

    if(instanceof<NodeIden>(function)) {
        std::string& ifName = ((NodeIden*)function)->name;

        if(AST::aliasTable.find(ifName) != AST::aliasTable.end()) return Call::make(loc, AST::aliasTable[ifName], arguments);
        else if(currScope != nullptr && currScope->aliasTable.find(ifName) != currScope->aliasTable.end()) return Call::make(loc, currScope->aliasTable[ifName], arguments);

        if(AST::funcTable.find(ifName) != AST::funcTable.end()) {
            checkAndGenerate(ifName);

            std::vector<Type*> types = Call::getTypes(arguments);
            std::string sTypes = typesToString(types);

            if(generator->functions.find(ifName) == generator->functions.end()) {
                // Function with compile-time args (ctargs)
                if(AST::funcTable[ifName]->isCtargs) {
                    std::vector<RaveValue> params = Call::genParameters(arguments, byVals, AST::funcTable[ifName], loc);

                    if(generator->functions.find(ifName + sTypes) != generator->functions.end())
                        return LLVM::call(generator->functions[ifName + sTypes], params, (instanceof<TypeVoid>(AST::funcTable[ifName]->type) ? "" : "callFunc"), byVals);
                    
                    return LLVM::call(generator->functions[AST::funcTable[ifName]->generateWithCtargs(Call::getParamsTypes(params))], params, (instanceof<TypeVoid>(AST::funcTable[ifName]->type) ? "" : "callFunc"));
                }

                // Function with templates
                if(AST::funcTable[ifName]->templateNames.size() > 0) {
                    std::string sTypes = Template::fromTypes(types);

                    if(AST::funcTable[ifName]->args.size() != arguments.size() && !AST::funcTable[ifName]->isCdecl64 && !AST::funcTable[ifName]->isWin64 && !AST::funcTable[ifName]->isVararg)
                        generator->error("wrong number of arguments for calling function '" + ifName + sTypes + "' (" + std::to_string(AST::funcTable[ifName]->args.size()) + " expected, " + std::to_string(arguments.size()) + " provided)!", loc);
                
                    // If it was already generated - call it
                    if(AST::funcTable.find(ifName + sTypes) != AST::funcTable.end()) {
                        std::vector<RaveValue> params = Call::genParameters(arguments, byVals, AST::funcTable[ifName + sTypes], loc);
                        return LLVM::call(generator->functions[ifName + sTypes], params, (instanceof<TypeVoid>(AST::funcTable[ifName + sTypes]->type) ? "" : "callFunc"), byVals);
                    }

                    size_t tnSize = AST::funcTable[ifName]->templateNames.size();
                    NodeFunc* tnFunction = AST::funcTable[ifName];

                    std::vector<RaveValue> params = Call::genParameters(arguments, byVals, tnFunction, loc);
                    types = Call::getParamsTypes(params);
                    
                    // If types of parameters more than template types - remove last types
                    while(types.size() > tnSize) types.pop_back();

                    sTypes = Template::fromTypes(types);

                    if(types.size() == tnSize)
                        return LLVM::call(tnFunction->generateWithTemplate(types, ifName + sTypes), params, (instanceof<TypeVoid>(tnFunction->type) ? "" : "callFunc"), byVals);
                    else generator->error("wrong number of template types for calling function '" + ifName + "' (" + std::to_string(tnSize) + " expected, " + std::to_string(types.size()) + " provided)!", loc);
                }

                // Function is undefined
                generator->error("undefined function '" + ifName + "'!", loc);
            }

            if(AST::funcTable.find(ifName + sTypes) != AST::funcTable.end()) {
                checkAndGenerate(ifName + sTypes);
                std::vector<RaveValue> params = Call::genParameters(arguments, byVals, AST::funcTable[ifName + sTypes], loc);
                return LLVM::call(generator->functions[ifName + sTypes], params, (instanceof<TypeVoid>(AST::funcTable[ifName + sTypes]->type) ? "" : "callFunc"), byVals);
            }
            else {
                // Arrayable - accepts pointer and length instead of array
                if(AST::funcTable[ifName]->isArrayable && arguments.size() > 0) {
                    std::vector<Node*> newArgs;
                    bool isChanged = false;

                    for(int i=0; i<arguments.size(); i++) {
                        if(instanceof<TypeArray>(arguments[i]->getType())) {
                            isChanged = true;
                            newArgs.insert(newArgs.end(), {new NodeUnary(loc, TokType::Amp, arguments[i]), ((TypeArray*)arguments[i]->getType())->count->comptime()});
                        }
                        else newArgs.push_back(arguments[i]);
                    }

                    // If changed - just call it
                    if(isChanged) {
                        std::vector<Type*> newTypes = Call::getTypes(newArgs);
                        std::string newSTypes = typesToString(newTypes);

                        if(AST::funcTable.find(ifName + newSTypes) != AST::funcTable.end()) {
                            std::vector<RaveValue> params = Call::genParameters(newArgs, byVals, AST::funcTable[ifName + newSTypes], loc);
                            return LLVM::call(generator->functions[ifName + newSTypes], params, (instanceof<TypeVoid>(AST::funcTable[ifName + newSTypes]->type) ? "" : "callFunc"), byVals);
                        }

                        // Try to search manually
                        for(auto& kv : AST::funcTable) {
                            if(kv.first.find(ifName) != std::string::npos) {
                                if(hasIdenticallyArgs(newTypes, kv.second->args)) {
                                    std::vector<RaveValue> params = Call::genParameters(newArgs, byVals, kv.second, loc);
                                    return LLVM::call(generator->functions[kv.first], params, (instanceof<TypeVoid>(kv.second->type) ? "" : "callFunc"), byVals);
                                }
                            }
                        }
                    }

                    if(!hasIdenticallyArgs(types, AST::funcTable[ifName]->args)) {
                        for(auto& it : AST::funcTable) {
                            if(it.first != ifName) {
                                if(it.first.find(ifName + "[") != 0 && it.first.find(ifName + "<") != 0) continue;
                            }

                            if(hasIdenticallyArgs(types, it.second->args)) {
                                checkAndGenerate(it.first);
                                std::vector<RaveValue> params = Call::genParameters(arguments, byVals, it.second, loc);
                                return LLVM::call(generator->functions[it.first], params, (instanceof<TypeVoid>(it.second->type) ? "" : "callFunc"), byVals);
                            }
                        }

                        if(AST::funcTable[ifName]->args.size() != arguments.size()) {
                            // Choose the most right overload
                            for(auto& it : AST::funcTable) {
                                if(it.second->args.size() != arguments.size()) continue;

                                if(it.first != ifName) {
                                    if(it.first.find(ifName + "[") != 0 && it.first.find(ifName + "<") != 0) continue;
                                }

                                checkAndGenerate(it.first);
                                std::vector<RaveValue> params = Call::genParameters(arguments, byVals, it.second, loc);
                                return LLVM::call(generator->functions[it.first], params, (instanceof<TypeVoid>(it.second->type) ? "" : "callFunc"), byVals);
                            }
                        }

                        if(!AST::funcTable[ifName]->isCdecl64 && !AST::funcTable[ifName]->isWin64 && !AST::funcTable[ifName]->isVararg)
                            generator->error("wrong number of arguments for calling function '" + ifName + "' (" + std::to_string(AST::funcTable[ifName]->args.size()) + " expected, " + std::to_string(arguments.size()) + " provided)!", loc);
                    }

                    std::vector<RaveValue> params = Call::genParameters(arguments, byVals, AST::funcTable[ifName], loc);
                    return LLVM::call(generator->functions[ifName], params, (instanceof<TypeVoid>(AST::funcTable[ifName]->type) ? "" : "callFunc"), byVals);
                }
                else if(!hasIdenticallyArgs(types, AST::funcTable[ifName]->args)) {
                    // Choose the most right overload
                    for(auto& it : AST::funcTable) {
                        if(it.first != ifName) {
                            if(it.first.find(ifName + "[") != 0 && it.first.find(ifName + "<") != 0) continue;
                        }

                        if(hasIdenticallyArgs(types, it.second->args)) {
                            checkAndGenerate(it.first);
                            std::vector<RaveValue> params = Call::genParameters(arguments, byVals, it.second, loc);
                            return LLVM::call(generator->functions[it.first], params, (instanceof<TypeVoid>(it.second->type) ? "" : "callFunc"), byVals);
                        }
                    }
                }

                std::vector<RaveValue> params = Call::genParameters(arguments, byVals, AST::funcTable[ifName], loc);
                return LLVM::call(generator->functions[ifName], params, (instanceof<TypeVoid>(AST::funcTable[ifName]->type) ? "" : "callFunc"), byVals);
            }
        }

        // Function is a variable?
        if(currScope->has(ifName)) {
            if(!instanceof<TypeFunc>(currScope->getVar(ifName, loc)->type)) generator->error("undefined function '" + ifName + "'!", loc);

            TypeFunc* fn = (TypeFunc*)currScope->getVar(ifName, loc)->type;
            std::vector<FuncArgSet> fas = tfaToFas(fn->args);

            std::vector<RaveValue> params = Call::genParameters(arguments, byVals, fas, CallSettings{false, fn->isVarArg, loc});
            return LLVM::call(currScope->get(ifName), params, (instanceof<TypeVoid>(fn->main) ? "" : "callFunc"), byVals);
        }

        // Function is a template?
        if(ifName.find('<') != std::string::npos) {
            std::string mainName = ifName.substr(0, ifName.find('<'));
            bool presenceInFt = AST::funcTable.find(mainName) != AST::funcTable.end();
            std::string callTypes = typesToString(Call::getTypes(arguments));
            std::string sTypes;

            if(AST::funcTable.find(ifName + callTypes) != AST::funcTable.end()) {
                std::vector<RaveValue> params = Call::genParameters(arguments, byVals, AST::funcTable[ifName + callTypes], loc);
                return LLVM::call(generator->functions[ifName + callTypes], params, (instanceof<TypeVoid>(AST::funcTable[ifName + callTypes]->type) ? "" : "callFunc"), byVals);
            }

            if(presenceInFt) {
                if(AST::funcTable.find(mainName + callTypes) != AST::funcTable.end()) {
                    if(generator->functions.find(ifName + callTypes) != generator->functions.end()) {
                        // This function is already exists - just call it
                        std::vector<RaveValue> params = Call::genParameters(arguments, byVals, AST::funcTable[ifName + callTypes], loc);
                        return LLVM::call(generator->functions[ifName + callTypes], params, (instanceof<TypeVoid>(AST::funcTable[ifName + callTypes]->type) ? "" : "callFunc"), byVals);
                    }

                    mainName += callTypes;
                }
                else if((AST::funcTable[mainName]->args.size() != arguments.size()) && !AST::funcTable[mainName]->isCdecl64 && !AST::funcTable[mainName]->isWin64 && !AST::funcTable[mainName]->isVararg)
                    generator->error("wrong number of arguments for calling function '" + ifName + "' (" + std::to_string(AST::funcTable[mainName]->args.size()) + " expected, " + std::to_string(arguments.size()) + " provided)!", loc);
            }

            sTypes = ifName.substr(ifName.find('<') + 1, ifName.find('>') - ifName.find('<') - 1);

            Lexer tLexer(sTypes, 1);
            Parser tParser(tLexer.tokens, "(builtin)");

            std::vector<Type*> types;
            sTypes = "";

            while(tParser.peek()->type != TokType::Eof) {
                switch(tParser.peek()->type) {
                    case TokType::Number: case TokType::HexNumber: case TokType::FloatNumber: {
                        Node* value = tParser.parseExpr();
                        types.push_back(new TypeTemplateMember(value->getType(), value));
                        break;
                    }
                    default:
                        types.push_back(tParser.parseType(true));
                        break;
                }

                if(tParser.peek()->type == TokType::Comma) tParser.next();
            }

            for(int i=0; i<types.size(); i++) Template::replaceTemplates(&types[i]);

            if(presenceInFt) {
                for(int i=0; i<types.size(); i++) sTypes += types[i]->toString() + ",";

                sTypes = "<" + sTypes.substr(0, sTypes.length() - 1) + ">";

                if(AST::funcTable.find(mainName + sTypes) != AST::funcTable.end()) return Call::make(loc, new NodeIden(mainName + sTypes, loc), arguments);

                std::string sTypes2 = sTypes + typesToString(types);
                if(AST::funcTable.find(mainName + sTypes2) != AST::funcTable.end()) return Call::make(loc, new NodeIden(mainName + sTypes2, loc), arguments);

                AST::funcTable[mainName]->generateWithTemplate(types, mainName + sTypes + (mainName.find('[') == std::string::npos ? callTypes : ""));
            }
            else {
                sTypes = "<";

                for(int i=0; i<types.size(); i++) sTypes += types[i]->toString() + ",";

                sTypes.back() = '>';

                ifName = mainName + sTypes;
                mainName = ifName.substr(0, ifName.find('<'));

                if(AST::funcTable.find(mainName) == AST::funcTable.end()) {
                    // Not working... maybe generate structure?
                    if(AST::structTable.find(mainName) != AST::structTable.end()) AST::structTable[mainName]->genWithTemplate(sTypes, types);
                    else generator->error("undefined structure '" + ifName + "'!", loc);
                }

                return Call::make(loc, AST::funcTable[ifName], arguments);
            }

            return Call::make(loc, function, arguments);
        }

        if(currScope->has("this")) {
            NodeVar* _this = currScope->getVar("this", loc);
            if(instanceof<TypeStruct>(_this->type->getElType())) {
                TypeStruct* _struct = (TypeStruct*)_this->type->getElType();

                if(AST::structTable.find(_struct->name) != AST::structTable.end()) {
                    for(auto& it : AST::structTable[_struct->name]->variables) {
                        if(it->name == ifName && instanceof<TypeFunc>(it->type)) {
                            std::vector<FuncArgSet> fas = tfaToFas(((TypeFunc*)it->type)->args);
                            std::vector<RaveValue> params = Call::genParameters(arguments, byVals, fas, CallSettings{false, ((TypeFunc*)it->type)->isVarArg, loc});
                            return LLVM::call(currScope->getWithoutLoad(it->name), params, (instanceof<TypeVoid>(it->type) ? "" : "callFunc"), byVals);
                        }
                    }
                }

                arguments.insert(arguments.begin(), new NodeIden("this", loc));

                auto methodf = Call::findMethod(_struct->name, ifName, arguments, loc);

                std::vector<Type*> types = Call::getTypes(arguments);

                if(!hasIdenticallyArgs(types, methodf->second->args)) {
                    for(auto& it : AST::funcTable) {
                        if(hasIdenticallyArgs(types, it.second->args)) {
                            if(generator->functions.find(it.second->name) == generator->functions.end()) it.second->generate();
                            std::vector<RaveValue> params = Call::genParameters(arguments, byVals, it.second, loc);
                            return LLVM::call(generator->functions[it.second->name], params, (instanceof<TypeVoid>(it.second->type) ? "" : "callFunc"), byVals);
                        }
                    }
                }

                std::vector<RaveValue> params = Call::genParameters(arguments, byVals, methodf->second, loc);
                if(instanceof<TypePointer>(params[0].type->getElType())) params[0] = LLVM::load(params[0], "NodeCall_load", loc);

                return LLVM::call(generator->functions[methodf->second->name], params, (instanceof<TypeVoid>(methodf->second->type) ? "" : "callFunc"), byVals);
            }
        }

        generator->error("undefined function '" + ifName + "'!", loc);
    }

    if(instanceof<NodeGet>(function)) {
        NodeGet* getFunc = (NodeGet*)function;

        if(instanceof<NodeIden>(getFunc->base)) {
            std::string &ifName = ((NodeIden*)getFunc->base)->name;

            if(!currScope->has(ifName) && !currScope->hasAtThis(ifName)) generator->error("undefined variable '" + ifName + "'!", loc);

            NodeVar* variable = currScope->getVar(ifName, loc);
            variable->isUsed = true;

            TypeStruct* structure = nullptr;

            if(instanceof<TypeStruct>(variable->type)) structure = (TypeStruct*)(variable->type);
            else if(instanceof<TypePointer>(variable->type) && instanceof<TypeStruct>(variable->type->getElType())) structure = (TypeStruct*)(variable->type->getElType());
            else generator->error("type of the variable '" + ifName + "' is not a structure!", loc);

            if(AST::structTable.find(structure->name) != AST::structTable.end()) {
                for(auto& it : AST::structTable[structure->name]->variables) {
                    if(it->name == ifName && instanceof<TypeFunc>(it->type)) {
                        std::vector<FuncArgSet> fas = tfaToFas(((TypeFunc*)it->type)->args);
                        std::vector<RaveValue> params = Call::genParameters(arguments, byVals, fas, CallSettings{false, ((TypeFunc*)it->type)->isVarArg, loc});
                        return LLVM::call(currScope->getWithoutLoad(it->name), params, (instanceof<TypeVoid>(it->type) ? "" : "callFunc"), byVals);
                    }
                }

                arguments.insert(arguments.begin(), getFunc->base);
                auto methodf = Call::findMethod(structure->name, getFunc->field, arguments, loc);

                std::vector<RaveValue> params = Call::genParameters(arguments, byVals, methodf->second, loc);
                if(instanceof<TypePointer>(params[0].type->getElType())) params[0] = LLVM::load(params[0], "NodeCall_load", loc);

                return LLVM::call(generator->functions[methodf->second->name], params, (instanceof<TypeVoid>(methodf->second->type) ? "" : "callFunc"), byVals);
            }

            generator->error("structure '" + structure->name + "' does not exist!", loc);
        }

        if(instanceof<NodeDone>(getFunc->base)) {
            RaveValue value = ((NodeDone*)getFunc->base)->value;
            Type* structure = value.type;

            while(instanceof<TypePointer>(structure)) structure = structure->getElType();

            if(AST::structTable.find(structure->toString()) != AST::structTable.end()) {
                arguments.insert(arguments.begin(), getFunc->base);
                auto methodf = Call::findMethod(structure->toString(), getFunc->field, arguments, loc);

                std::vector<RaveValue> params = Call::genParameters(arguments, byVals, methodf->second, loc);
                if(instanceof<TypePointer>(params[0].type->getElType())) params[0] = LLVM::load(params[0], "NodeCall_load", loc);

                return LLVM::call(generator->functions[methodf->second->name], params, (instanceof<TypeVoid>(methodf->second->type) ? "" : "callFunc"), byVals);
            }

            generator->error("structure '" + structure->toString() + "' does not exist!", loc);
        }

        if(instanceof<NodeCall>(getFunc->base)) {
            NodeCall* ncall = (NodeCall*)getFunc->base;
            
            return Call::make(loc, new NodeGet(new NodeDone(ncall->generate()), getFunc->field, getFunc->isMustBePtr, loc), arguments);
        }

        if(instanceof<NodeGet>(getFunc->base)) {
            NodeGet* nget = (NodeGet*)getFunc->base;
            nget->isMustBePtr = true;

            RaveValue value = nget->generate();
            Type* structure = value.type;

            while(instanceof<TypePointer>(structure)) structure = structure->getElType();

            if(AST::structTable.find(structure->toString()) != AST::structTable.end()) {
                arguments.insert(arguments.begin(), getFunc->base);
                auto methodf = Call::findMethod(structure->toString(), getFunc->field, arguments, loc);

                std::vector<RaveValue> params = Call::genParameters(arguments, byVals, methodf->second, loc);
                if(instanceof<TypePointer>(params[0].type->getElType())) params[0] = LLVM::load(params[0], "NodeCall_load", loc);

                return LLVM::call(generator->functions[methodf->second->name], params, (instanceof<TypeVoid>(methodf->second->type) ? "" : "callFunc"), byVals);
            }

            generator->error("structure '" + structure->toString() + "' does not exist!", loc);
        }

        if(instanceof<NodeIndex>(getFunc->base)) {
            NodeIndex* nindex = (NodeIndex*)getFunc->base;
            nindex->isMustBePtr = true;

            return Call::make(loc, new NodeGet(new NodeDone(nindex->generate()), getFunc->field, getFunc->isMustBePtr, loc), arguments);
        }

        generator->error("a call of this kind (NodeGet + " + std::string(typeid(function[0]).name()) + ") is unavailable!", loc);
    }

    if(instanceof<NodeUnary>(function)) {
        ((NodeUnary*)function)->base = new NodeDone(Call::make(loc, ((NodeUnary*)function)->base, arguments));
        return ((NodeUnary*)function)->generate();
    }

    generator->error("a call of this kind is unavailable!", loc);
    return {};
}

RaveValue NodeCall::generate() {
    return Call::make(loc, func, args);
}

void NodeCall::check() {this->isChecked = true;}
Node* NodeCall::comptime() {return this;}
Node* NodeCall::copy() {return new NodeCall(this->loc, this->func, this->args);}