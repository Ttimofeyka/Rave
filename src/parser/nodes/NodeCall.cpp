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

// Get a vector of arguments types
std::vector<Type*> NodeCall::getTypes() {
    std::vector<Type*> arr;
    for(int i=0; i<this->args.size(); i++) arr.push_back(this->args[i]->getType());
    return arr;
}

// Correct arguments by using a vector of FuncArgSet
std::vector<RaveValue> NodeCall::correctByLLVM(std::vector<RaveValue> values, std::vector<FuncArgSet> fas) {
    std::vector<RaveValue> params = std::vector<RaveValue>(values);
    if(fas.empty() || params.size() != fas.size()) return params;

    for(int i=0; i<params.size(); i++) {
        if(fas[i].type->toString() == "void*" || fas[i].type->toString() == "char*") {
            if((!instanceof<TypePointer>(params[i].type) || !instanceof<TypeBasic>(params[i].type->getElType()))) LLVM::cast(params[i], new TypePointer(basicTypes[BasicType::Char]));
        }
        else if(instanceof<TypePointer>(fas[i].type) && instanceof<TypeStruct>(((TypePointer*)fas[i].type)->instance)) {
            if(!instanceof<TypePointer>(params[i].type)) LLVM::makeAsPointer(params[i]);
        }
        else if(!instanceof<TypePointer>(fas[i].type) && instanceof<TypePointer>(params[i].type)) {
            if(LLVMIsAFunction(params[i].value) == nullptr) {
                if(instanceof<TypeStruct>(fas[i].type)) {
                    params[i].value = LLVMBuildLoad2(generator->builder, generator->genType(fas[i].type, loc), params[i].value, "correctLoad");
                    params[i].type = fas[i].type;
                }
                else params[i] = LLVM::load(params[i], "correctLoad", loc);
            }
        }
    }

    return params;
}

// Generate parameters
std::vector<RaveValue> NodeCall::getParameters(std::vector<int>& byVals, NodeFunc* nfunc, bool isVararg, std::vector<FuncArgSet> fas) {
    std::vector<RaveValue> params;

    if(this->args.size() != fas.size()) {
        for(int i=0; i<this->args.size(); i++) {
            // If this is a NodeIden/NodeIndex/NodeGet - set the 'isMustBePtr' to false
            if(instanceof<NodeIden>(args[i])) ((NodeIden*)args[i])->isMustBePtr = false;
            else if(instanceof<NodeIndex>(args[i])) ((NodeIndex*)args[i])->isMustBePtr = false;
            else if(instanceof<NodeGet>(args[i])) ((NodeGet*)args[i])->isMustBePtr = false;
            params.push_back(this->args[i]->generate());
        }

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
            if(!instanceof<TypePointer>(params[i].type) || !instanceof<TypeBasic>(params[i].type->getElType())) LLVM::cast(params[i], new TypePointer(basicTypes[BasicType::Char]));
        }
        else if(instanceof<TypePointer>(fas[i].type) && instanceof<TypeStruct>(((TypePointer*)fas[i].type)->instance)) {
            if(!instanceof<TypePointer>(params[i].type)) LLVM::makeAsPointer(params[i]);
        }
        else if(!instanceof<TypePointer>(fas[i].type) && instanceof<TypePointer>(params[i].type)) {
            if(instanceof<TypeStruct>(fas[i].type)) {
                if(AST::structTable.find(fas[i].type->toString()) != AST::structTable.end()) params[i] = LLVM::load(params[i], "getParameters_load", loc);
            }
            else params[i] = LLVM::load(params[i], "getParameters_load", loc);
        }
        else if(instanceof<TypeBasic>(fas[i].type) && instanceof<TypeBasic>(params[i].type) && !((TypeBasic*)params[i].type)->isFloat()) {
            TypeBasic* tbasic = (TypeBasic*)(fas[i].type);
            if(!tbasic->isFloat() && tbasic->type != ((TypeBasic*)params[i].type)->type) LLVM::cast(params[i], tbasic);
        }
        else if(instanceof<TypeBasic>(fas[i].type) && instanceof<TypeBasic>(params[i].type) &&  ((TypeBasic*)params[i].type)->type == BasicType::Double) {
            if(((TypeBasic*)fas[i].type)->type == BasicType::Float) LLVM::cast(params[i], fas[i].type);
        }
        else if(isCW64) {
            if(instanceof<TypeDivided>(fas[i].internalTypes[0])) {
                if(!instanceof<TypePointer>(params[i].type)) {
                    RaveValue temp = LLVM::alloc(params[i].type, "getParameters_stotd_temp");
                    LLVMBuildStore(generator->builder, params[i].value, temp.value);
                    params[i].value = temp.value;
                }
                params[i].value = LLVMBuildLoad2(generator->builder, generator->genType(((TypeDivided*)fas[i].internalTypes[0])->mainType, loc), params[i].value, "getParameters_stotd");
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
                    params[i].value = LLVMBuildPointerCast(generator->builder, params[i].value, generator->genType(fas[i].type, this->loc), "castVFtoCF");
                    TypeFunc* tfunc = ((TypeFunc*)params[i].type->getElType());
                    tfunc->main = basicTypes[BasicType::Char];
                    ((TypePointer*)params[i].type)->instance = tfunc;
                }
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

Type* NodeCall::__getType(Node* _fn) {
    if(instanceof<NodeIden>(_fn)) {
        NodeIden* niden = (NodeIden*)_fn;

        if(AST::aliasTable.find(niden->name) != AST::aliasTable.end()) return __getType(AST::aliasTable[niden->name]);
        else if(currScope != nullptr && currScope->aliasTable.find(niden->name) != currScope->aliasTable.end()) return __getType(currScope->aliasTable[niden->name]);

        if(AST::funcTable.find((niden->name + typesToString(this->getTypes()))) != AST::funcTable.end()) return AST::funcTable[(((NodeIden*)_fn)->name + typesToString(this->getTypes()))]->getType();
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

RaveValue NodeCall::generate() {
    std::vector<int> byVals;

    if(instanceof<NodeIden>(this->func)) {
        NodeIden* idenFunc = (NodeIden*)this->func;

        if(AST::aliasTable.find(idenFunc->name) != AST::aliasTable.end()) return (new NodeCall(loc, AST::aliasTable[idenFunc->name], this->args))->generate();
        else if(currScope != nullptr && currScope->aliasTable.find(idenFunc->name) != currScope->aliasTable.end())
            return (new NodeCall(loc, currScope->aliasTable[idenFunc->name], this->args))->generate();

        if(AST::funcTable.find(idenFunc->name) != AST::funcTable.end()) {
            std::vector<Type*> __types = this->getTypes();
            std::string sTypes = typesToString(__types);

            if(generator->functions.find(idenFunc->name) == generator->functions.end()) {
                // Function with compile-time args (ctargs)
                if(AST::funcTable[idenFunc->name]->isCtargs) {
                    std::vector<RaveValue> params = this->getParameters(byVals, nullptr, false);

                    if(generator->functions.find(idenFunc->name + sTypes) != generator->functions.end()) {
                        return LLVM::call(generator->functions[idenFunc->name + sTypes], params, (instanceof<TypeVoid>(AST::funcTable[idenFunc->name]->type) ? "" : "callFunc"), byVals);
                    }

                    std::vector<Type*> types;
                    for(int i=0; i<params.size(); i++) types.push_back(params[i].type);
                    return LLVM::call(generator->functions[AST::funcTable[idenFunc->name]->generateWithCtargs(types)], params, (instanceof<TypeVoid>(AST::funcTable[idenFunc->name]->type) ? "" : "callFunc"));
                }

                if(AST::funcTable[idenFunc->name]->templateNames.size() > 0) {
                    // Template function
                    std::string __typesAll = "<";
                    for(int i=0; i<__types.size(); i++) __typesAll += __types[i]->toString() + ",";
                    __typesAll = __typesAll.substr(0, __typesAll.length() - 1) + ">";

                    if(AST::funcTable.find(idenFunc->name + __typesAll) != AST::funcTable.end()) {
                        // If it was already generated - call it
                        std::vector<RaveValue> params = this->getParameters(byVals, AST::funcTable[idenFunc->name + __typesAll], false, AST::funcTable[idenFunc->name + __typesAll]->args);
                        return LLVM::call(generator->functions[idenFunc->name + __typesAll], params, instanceof<TypeVoid>(AST::funcTable[idenFunc->name + __typesAll]->type) ? "" : "callFunc", byVals);
                    }

                    size_t tnSize = AST::funcTable[idenFunc->name]->templateNames.size();
                    NodeFunc* nfunc = AST::funcTable[idenFunc->name];

                    std::vector<RaveValue> params = this->getParameters(byVals, nfunc, false);
                    std::vector<Type*> types;
                    std::string all = "<";

                    for(int i=0; i<params.size(); i++) types.push_back(params[i].type);
                    while(types.size() > tnSize) types.pop_back();
    
                    if(types.size() == tnSize) {
                        for(int i=0; i<types.size(); i++) all += types[i]->toString() + ",";
                        all.back() = '>';

                        RaveValue fn = AST::funcTable[idenFunc->name]->generateWithTemplate(types, idenFunc->name + all);
                        if(AST::funcTable.find(idenFunc->name + all) != AST::funcTable.end()) {
                            return LLVM::call(fn, params, instanceof<TypeVoid>(AST::funcTable[idenFunc->name + all]) ? "" : "callFunc", byVals);
                        }
                    }
                }

                if(AST::debugMode) {
                    for(auto const& x : generator->functions) std::cout << "\t" << x.first << std::endl;
                    std::cout << "DEBUG_MODE: undefined function (generator->functions)!" << std::endl;
                }

                generator->error("undefined function '" + idenFunc->name + "'!", this->loc);
                return {};
            }

            if(AST::funcTable.find(idenFunc->name + sTypes) != AST::funcTable.end()) {
                isCW64 = AST::funcTable[idenFunc->name + sTypes]->isCdecl64 || AST::funcTable[idenFunc->name + sTypes]->isWin64;
                std::vector<RaveValue> params = this->getParameters(byVals, AST::funcTable[idenFunc->name + sTypes], false, AST::funcTable[idenFunc->name + sTypes]->args);
                return LLVM::call(generator->functions[idenFunc->name + sTypes], params, (instanceof<TypeVoid>(AST::funcTable[idenFunc->name + sTypes]->type) ? "" : "callFunc"), byVals);
            }
            else if(AST::funcTable[idenFunc->name]->isArrayable && this->args.size() > 0) {
                // Arrayable - accepts pointer and length instead of array
                std::vector<Node*> newArgs;
                bool isChanged = false;

                for(int i=0; i<args.size(); i++) {
                    if(instanceof<TypeArray>(args[i]->getType())) {
                        isChanged = true;
                        newArgs.insert(newArgs.end(), { new NodeUnary(loc, TokType::GetPtr, args[i]), ((TypeArray*)args[i]->getType())->count->comptime() });
                    }
                    else newArgs.push_back(args[i]);
                }

                if(isChanged) {
                    std::vector<Type*> newTypes;
                    for(int i=0; i<newArgs.size(); i++) newTypes.push_back(newArgs[i]->getType());
                    std::string newSTypes = typesToString(newTypes);
                    if(AST::funcTable.find(idenFunc->name + newSTypes) != AST::funcTable.end()) {
                        this->args = newArgs;
                        isCW64 = AST::funcTable[idenFunc->name + newSTypes]->isCdecl64 || AST::funcTable[idenFunc->name + newSTypes]->isWin64;
                        std::vector<RaveValue> params = this->getParameters(byVals, AST::funcTable[idenFunc->name + newSTypes], false, AST::funcTable[idenFunc->name + newSTypes]->args);
                        return LLVM::call(generator->functions[idenFunc->name + newSTypes], params, (instanceof<TypeVoid>(AST::funcTable[idenFunc->name + newSTypes]->type) ? "" : "callFunc"), byVals);
                    }
                }
            }

            isCW64 = AST::funcTable[idenFunc->name]->isCdecl64 || AST::funcTable[idenFunc->name]->isWin64;
            std::vector<RaveValue> params = this->getParameters(byVals, AST::funcTable[idenFunc->name], false, AST::funcTable[idenFunc->name]->args);
            return LLVM::call(generator->functions[idenFunc->name], params, (instanceof<TypeVoid>(AST::funcTable[idenFunc->name]->type) ? "" : "callFunc"), byVals);
        }

        if(currScope->has(idenFunc->name)) {
            if(!instanceof<TypeFunc>(currScope->getVar(idenFunc->name, this->loc)->type)) generator->error("undefined function '" + idenFunc->name + "'!", this->loc);

            TypeFunc* fn = (TypeFunc*)currScope->getVar(idenFunc->name, this->loc)->type;
            std::vector<RaveValue> params = this->getParameters(byVals, nullptr, false, tfaToFas(fn->args));
            return LLVM::call(currScope->get(idenFunc->name), params, (instanceof<TypeVoid>(fn->main) ? "" : "callFunc"), byVals);
        }

        if(idenFunc->name.find('<') != std::string::npos && AST::funcTable.find(idenFunc->name.substr(0, idenFunc->name.find('<'))) != AST::funcTable.end()) {
            std::string mainName = idenFunc->name.substr(0, idenFunc->name.find('<'));
            std::string callTypes = typesToString(this->getTypes());

            if(AST::funcTable.find(mainName + callTypes) != AST::funcTable.end()) {
                if(generator->functions.find(idenFunc->name + callTypes) != generator->functions.end()) {
                    // This function is already exists - just call it
                    isCW64 = AST::funcTable[idenFunc->name + callTypes]->isCdecl64 || AST::funcTable[idenFunc->name + callTypes]->isWin64;
                    std::vector<RaveValue> values = this->getParameters(byVals, AST::funcTable[idenFunc->name + callTypes], false);
                    return LLVM::call(generator->functions[idenFunc->name + callTypes], values, instanceof<TypeVoid>(AST::funcTable[idenFunc->name + callTypes]->type) ? "" : "callFunc", byVals);
                }
                mainName += callTypes;
            }

            std::string sTypes = idenFunc->name.substr(idenFunc->name.find('<') + 1);
            sTypes = sTypes.substr(0, sTypes.length() - 1);

            Lexer* tLexer = new Lexer(sTypes, 1);
            Parser* tParser = new Parser(tLexer->tokens, "(builtin)");

            std::vector<Type*> types;
            while(tParser->peek()->type != TokType::Eof) {
                if(tParser->peek()->type == TokType::Number || tParser->peek()->type == TokType::HexNumber || tParser->peek()->type == TokType::FloatNumber) {
                    Node* value = tParser->parseExpr();
                    types.push_back(new TypeTemplateMember(value->getType(), value));
                }
                else types.push_back(tParser->parseType(true));
                if(tParser->peek()->type == TokType::Comma) tParser->next();
            }

            for(int i=0; i<types.size(); i++) {
                Type* current = types[i];
                Type* previous = nullptr;
                
                while(instanceof<TypeConst>(current) || instanceof<TypeArray>(current) || instanceof<TypePointer>(current) || instanceof<TypeStruct>(current)) {
                    if(instanceof<TypeConst>(current)) {previous = current; current = ((TypeConst*)previous)->instance;}
                    else if(instanceof<TypeArray>(current)) {previous = current; current = ((TypeArray*)previous)->element;}
                    else if(instanceof<TypePointer>(current)) {previous = current; current = ((TypePointer*)previous)->instance;}
                    else {
                        while(generator->toReplace.find(current->toString()) != generator->toReplace.end()) current = generator->toReplace[current->toString()];

                        if(previous == nullptr) types[i] = current->copy();
                        else if(instanceof<TypeConst>(previous)) ((TypeConst*)previous)->instance = current->copy();
                        else if(instanceof<TypeArray>(previous)) ((TypeArray*)previous)->element = current->copy();
                        else ((TypePointer*)previous)->instance = current->copy();
                        break;
                    }
                }
            }

            sTypes = "";
            for(int i=0; i<types.size(); i++) sTypes += types[i]->toString() + ",";
            sTypes = sTypes.substr(0, sTypes.length() - 1);

            if(AST::funcTable.find(mainName + sTypes) != AST::funcTable.end()) return (new NodeCall(loc, new NodeIden(mainName + sTypes, loc), args))->generate();
            AST::funcTable[mainName]->generateWithTemplate(types, mainName + sTypes + (mainName.find('[') != std::string::npos ? callTypes : ""));

            delete tLexer;
            delete tParser;
            return this->generate();
        }

        if(idenFunc->name.find('<') != std::string::npos) {
            std::string mainName = idenFunc->name.substr(0, idenFunc->name.find('<'));
            std::string sTypes = idenFunc->name.substr(idenFunc->name.find('<') + 1);
            Lexer* tLexer = new Lexer(sTypes.substr(0, sTypes.size() - 1), 1);
            Parser* tParser = new Parser(tLexer->tokens, "(builtin)");

            std::vector<Type*> types;
            sTypes = "";

            while(tParser->peek()->type != TokType::Eof) {
                if(tParser->peek()->type == TokType::Number || tParser->peek()->type == TokType::HexNumber || tParser->peek()->type == TokType::FloatNumber) {
                    Node* value = tParser->parseExpr();
                    types.push_back(new TypeTemplateMember(value->getType(), value));
                }
                else types.push_back(tParser->parseType(true));
                if(tParser->peek()->type == TokType::Comma) tParser->next();
            }

            for(int i=0; i<types.size(); i++) {
                while(generator->toReplace.find(types[i]->toString()) != generator->toReplace.end()) types[i] = generator->toReplace[types[i]->toString()];
                sTypes += types[i]->toString() + ",";
            }

            idenFunc->name = mainName + "<" + sTypes.substr(0, sTypes.size() - 1) + ">";

            if(AST::funcTable.find(idenFunc->name.substr(0, idenFunc->name.find('<'))) == AST::funcTable.end()) {
                // Not working... maybe generate structure?
                if(AST::structTable.find(idenFunc->name.substr(0, idenFunc->name.find('<'))) != AST::structTable.end()) {
                    AST::structTable[idenFunc->name.substr(0, idenFunc->name.find('<'))]->genWithTemplate("<" + sTypes.substr(0, sTypes.size() - 1) + ">", types);
                }
                else generator->error("undefined structure '" + idenFunc->name + "'!", this->loc);
            }
            
            delete tLexer;
            delete tParser;
            return this->generate();
        }

        if(AST::debugMode) {
            for(auto const& x : AST::funcTable) std::cout << "\t" << x.first << std::endl;
            std::cout << "DEBUG_MODE 2: undefined function!" << std::endl;
        }

        generator->error("undefined function '" + idenFunc->name + "'!", this->loc);
    }

    if(instanceof<NodeGet>(this->func)) {
        NodeGet* getFunc = (NodeGet*)this->func;
        if(instanceof<NodeIden>(getFunc->base)) {
            NodeIden* idenFunc = (NodeIden*)getFunc->base;
            if(!currScope->has(idenFunc->name) && !currScope->hasAtThis(idenFunc->name)) generator->error("undefined variable '" + idenFunc->name + "'!", this->loc);

            NodeVar* var = currScope->getVar(idenFunc->name, this->loc);
            TypeStruct* structure = nullptr;

            if(instanceof<TypeStruct>(var->type)) structure = (TypeStruct*)var->type;
            else if(instanceof<TypePointer>(var->type) && instanceof<TypeStruct>(((TypePointer*)var->type)->instance)) structure = (TypeStruct*)((TypePointer*)var->type)->instance;
            else generator->error("variable '" + idenFunc->name + "' does not have structure as type!", this->loc);

            if(AST::structTable.find(structure->name) != AST::structTable.end()) {
                std::vector<RaveValue> params = this->getParameters(byVals, nullptr, false);
                std::vector<Type*> types;

                params.insert(params.begin(), currScope->getWithoutLoad(idenFunc->name));
                if(instanceof<TypePointer>(params[0].type->getElType())) params[0] = LLVM::load(params[0], "NodeCall_load", loc);
                for(int i=0; i<params.size(); i++) types.push_back(params[i].type);

                std::pair<std::string, std::string> method = std::pair<std::string, std::string>(structure->name, getFunc->field);

                auto methodf = AST::methodTable.find(method);

                if(methodf == AST::methodTable.end() || !hasIdenticallyArgs(types, AST::methodTable[method]->args)) {method.second += typesToString(types); methodf = AST::methodTable.find(method);}
                if(methodf == AST::methodTable.end()) {method.second = method.second.substr(0, method.second.find('[')); methodf = AST::methodTable.find(method);}
                if(methodf == AST::methodTable.end()) generator->error("undefined method '" + method.second.substr(0, method.second.find('[')) + "' of structure '" + structure->name + "'!", this->loc);

                if(generator->functions[methodf->second->name].value == nullptr) generator->error("using '" + methodf->second->origName + "' method before declaring it!", this->loc);

                params = this->correctByLLVM(params, methodf->second->args);
                return LLVM::call(generator->functions[methodf->second->name], params, (instanceof<TypeVoid>(methodf->second->type) ? "" : "callFunc"), byVals);
            }

            generator->error("undefined structure '" + structure->name + "'!", this->loc);
        }
        else if(instanceof<NodeCall>(getFunc->base)) {
            NodeCall* ncall = (NodeCall*)getFunc->base;
            RaveValue result = ncall->generate();

            if(currScope->localVars.find("__RAVE_NG_NGC") != currScope->localVars.end() && currScope->localVars["__RAVE_NG_NGC"] != nullptr) delete currScope->localVars["__RAVE_NG_NGC"];

            // Creating a temp variable
            currScope->localScope["__RAVE_NG_NGC"] = result;
            currScope->localVars["__RAVE_NG_NGC"] = new NodeVar(
                "__RAVE_NG_NGC", nullptr, false, false, false, {},
                this->loc, currScope->localScope["__RAVE_NG_NGC"].type
            );

            return (new NodeCall(this->loc, new NodeGet(new NodeIden("__RAVE_NG_NGC", this->loc), getFunc->field, getFunc->isMustBePtr, this->loc), this->args))->generate();
        }
        else if(instanceof<NodeGet>(getFunc->base)) {
            NodeGet* nget = (NodeGet*)getFunc->base;

            if(currScope->localVars.find("__RAVE_NG_NG") != currScope->localVars.end() && currScope->localVars["__RAVE_NG_NG"] != nullptr) delete currScope->localVars["__RAVE_NG_NG"];

            // Creating a temp variable
            currScope->localScope["__RAVE_NG_NG"] = nget->generate();
            currScope->localVars["__RAVE_NG_NG"] = new NodeVar(
                "__RAVE_NG_NG", nullptr, false, false, false, {},
                this->loc, currScope->localScope["__RAVE_NG_NG"].type
            );

            return (new NodeCall(this->loc, new NodeGet(new NodeIden("__RAVE_NG_NG", this->loc), getFunc->field, getFunc->isMustBePtr, this->loc), this->args))->generate();
        }
        else if(instanceof<NodeIndex>(getFunc->base)) {
            NodeIndex* nindex = (NodeIndex*)getFunc->base;

            if(currScope->localVars.find("__RAVE_NG_NI") != currScope->localVars.end() && currScope->localVars["__RAVE_NG_NI"] != nullptr) delete currScope->localVars["__RAVE_NG_NI"];

            // Creating a temp variable
            currScope->localScope["__RAVE_NG_NI"] = nindex->generate();
            currScope->localVars["__RAVE_NG_NI"] = new NodeVar(
                "__RAVE_NG_NI", nullptr, false, false, false, {},
                this->loc, currScope->localScope["__RAVE_NG_NI"].type
            );

            return (new NodeCall(this->loc, new NodeGet(new NodeIden("__RAVE_NG_NI", this->loc), getFunc->field, getFunc->isMustBePtr, this->loc), this->args))->generate();
        }
        generator->error("a call of this kind (NodeGet + " + std::string(typeid(this->func[0]).name()) + ") is temporarily unavailable!", this->loc);
    }

    if(instanceof<NodeUnary>(this->func)) {
        NodeCall* nc = new NodeCall(this->loc, ((NodeUnary*)this->func)->base, this->args);
        return (new NodeUnary(loc, ((NodeUnary*)this->func)->type, nc))->generate();
    }

    generator->error("a call of this kind is temporarily unavailable!", this->loc);
    return {};
}

void NodeCall::check() {this->isChecked = true;}
Node* NodeCall::comptime() {return this;}
Node* NodeCall::copy() {return new NodeCall(this->loc, this->func, this->args);}