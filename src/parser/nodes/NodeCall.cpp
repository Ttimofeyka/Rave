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

NodeCall::NodeCall(long loc, Node* func, std::vector<Node*> args) {
    this->loc = loc;
    this->func = func;
    this->args = std::vector<Node*>(args);
}

Type* NodeCall::getType() {
    if(instanceof<NodeIden>(this->func)) {
        if(AST::funcTable.find((((NodeIden*)this->func)->name + typesToString(this->getTypes()))) != AST::funcTable.end()) return AST::funcTable[(((NodeIden*)this->func)->name + typesToString(this->getTypes()))]->getType();
        if(AST::funcTable.find(((NodeIden*)this->func)->name) != AST::funcTable.end()) return AST::funcTable[((NodeIden*)this->func)->name]->type;
        return new TypePointer(new TypeVoid());
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
        else if(!instanceof<TypePointer>(fas[i].type) && LLVMGetTypeKind(LLVMTypeOf(params[i])) == LLVMPointerTypeKind) {
            if(LLVMIsAFunction(params[i]) == nullptr) params[i] = LLVM::load(params[i], "correctLoad");
        }
    }
    return params;
}

std::vector<LLVMValueRef> NodeCall::getParameters(NodeFunc* nfunc, bool isVararg, std::vector<FuncArgSet> fas) {
    std::vector<LLVMValueRef> params;
    if(this->args.size() != fas.size()) {
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
            if(currScope->detectMemoryLeaks && nfunc != nullptr) {
                if(instanceof<NodeIden>(this->args[i])) {
                    if(currScope->has(((NodeIden*)this->args[i])->name) && nfunc->isReleased(i)) {
                        currScope->getVar(((NodeIden*)this->args[i])->name)->isAllocated = false;
                    }
                }
            }
        }
        else if(instanceof<TypeStruct>(fas[i].type) && LLVMGetTypeKind(LLVMTypeOf(params[i])) == LLVMStructTypeKind) {
            if(currScope->detectMemoryLeaks && nfunc != nullptr) {
                if(instanceof<NodeIden>(this->args[i]) && nfunc->isReleased(i)) currScope->getVar(((NodeIden*)this->args[i])->name)->isAllocated = false;
            }
        }
        else if(instanceof<TypeBasic>(fas[i].type) && LLVMGetTypeKind(LLVMTypeOf(params[i])) == LLVMIntegerTypeKind) {
            TypeBasic* tbasic = (TypeBasic*)(fas[i].type);
            if(!tbasic->isFloat() && ("i"+std::to_string(tbasic->getSize())) != std::string(LLVMPrintTypeToString(LLVMTypeOf(params[i])))) {
                params[i] = LLVMBuildIntCast(generator->builder, params[i], generator->genType(tbasic, this->loc), "Itoi_getParameters");
            }
        }
        else if(instanceof<TypeBasic>(fas[i].type) && LLVMGetTypeKind(LLVMTypeOf(params[i])) == LLVMDoubleTypeKind) {
            if(((TypeBasic*)fas[i].type)->type == BasicType::Float) params[i] = LLVMBuildFPCast(generator->builder, params[i], LLVMFloatTypeInContext(generator->context), "getParameters_dtof");
        }
        else if(this->isCdecl64 && instanceof<TypeBasic>(fas[i].type) && LLVMGetTypeKind(LLVMTypeOf(params[i])) == LLVMStructTypeKind) {
            TypeBasic* tbasic = (TypeBasic*)(fas[i].type);
            if(!tbasic->isFloat()) {
                LLVMValueRef temp = LLVMBuildAlloca(generator->builder, LLVMTypeOf(params[i]), "StructToI_cdecl64_getParameters_temp");
                LLVMBuildStore(generator->builder, params[i], temp);
                temp = LLVMBuildPointerCast(generator->builder, temp, LLVMPointerType(generator->genType(tbasic, this->loc), 0), "StructToI_cdecl64_getParameters");
                params[i] = LLVM::load(temp, "StructToI_cdecl64_getParameters_load");
            }
        }
        else if(instanceof<TypeFunc>(fas[i].type)) {
            TypeFunc* tfunc = (TypeFunc*)(fas[i].type);
            if(instanceof<TypeBasic>(tfunc->main) && ((TypeBasic*)(tfunc->main))->type == BasicType::Char) {
                if(LLVM::isPointer(params[i]) && LLVMGetTypeKind(LLVMGetElementType(LLVMTypeOf(params[i]))) == LLVMFunctionTypeKind
                && typeToString(LLVMTypeOf(params[i])).find("void ") == 0) {
                    // Casting void(...) to char(...)
                    params[i] = LLVMBuildPointerCast(generator->builder, params[i], generator->genType(fas[i].type, this->loc), "castVFtoCF");
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

LLVMValueRef NodeCall::generate() {
    if(instanceof<NodeIden>(this->func)) {
        NodeIden* idenFunc = (NodeIden*)this->func;
        if(AST::funcTable.find(idenFunc->name) != AST::funcTable.end()) {
            std::vector<Type*> __types = this->getTypes();
            std::string sTypes = typesToString(__types);
            if(generator->functions.find(idenFunc->name) == generator->functions.end()) {
                // Function with compile-time args (ctargs)
                if(AST::funcTable[idenFunc->name]->isCtargs) {
                    if(generator->functions.find(idenFunc->name + sTypes) != generator->functions.end()) {
                        return LLVM::call(generator->functions[idenFunc->name + sTypes], this->getParameters(nullptr, false).data(), this->args.size(), (instanceof<TypeVoid>(AST::funcTable[idenFunc->name]->type) ? "" : "callFunc"));
                    }
                    std::vector<LLVMValueRef> params = this->getParameters(nullptr, false);
                    std::vector<LLVMTypeRef> types;
                    for(int i=0; i<params.size(); i++) types.push_back(LLVMTypeOf(params[i]));
                    return LLVM::call(generator->functions[AST::funcTable[idenFunc->name]->generateWithCtargs(types)], params.data(), this->args.size(), (instanceof<TypeVoid>(AST::funcTable[idenFunc->name]->type) ? "" : "callFunc"));
                }
                if(AST::funcTable[idenFunc->name]->templateNames.size() > 0) {
                    // Template function
                    std::string __typesAll = "<";
                    for(int i=0; i<__types.size(); i++) __typesAll += __types[i]->toString() + ",";
                    __typesAll = __typesAll.substr(0, __typesAll.length() - 1) + ">";

                    if(AST::funcTable.find(idenFunc->name + __typesAll) != AST::funcTable.end()) {
                        // If it was already generated - call it
                        std::vector<LLVMValueRef> params = this->getParameters(nullptr, false);
                        return LLVM::call(generator->functions[idenFunc->name + __typesAll], params.data(), params.size(), instanceof<TypeVoid>(AST::funcTable[idenFunc->name + __typesAll]->type) ? "" : "callFunc");
                    }

                    size_t tnSize = AST::funcTable[idenFunc->name]->templateNames.size();
                    std::vector<LLVMValueRef> params = this->getParameters(nullptr, false);
                    std::vector<Type*> types;
                    std::string all = "<";

                    for(int i=0; i<params.size(); i++) types.push_back(lTypeToType(LLVMTypeOf(params[i])));
                    while(types.size() > tnSize) types.pop_back();
    
                    if(types.size() == tnSize) {
                        for(int i=0; i<types.size(); i++) all += types[i]->toString() + ",";
                        all = all.substr(0, all.length() - 1) + ">";
                        LLVMValueRef fn = AST::funcTable[idenFunc->name]->generateWithTemplate(types, idenFunc->name + all);
                        if(AST::funcTable.find(idenFunc->name + all) != AST::funcTable.end()) {
                            return LLVM::call(fn, params.data(), params.size(), instanceof<TypeVoid>(AST::funcTable[idenFunc->name + all]) ? "" : "callFunc");
                        }
                    }
                }
                if(AST::debugMode) {
                    for(auto const& x : generator->functions) std::cout << "\t" << x.first << std::endl;
                    std::cout << "DEBUG_MODE: undefined function (generator->functions)!" << std::endl;
                }
                generator->error("undefined function '" + idenFunc->name + "'!", this->loc);
                return nullptr;
            }
            if(AST::funcTable.find(idenFunc->name + sTypes) != AST::funcTable.end()) {
                this->isCdecl64 = AST::funcTable[idenFunc->name + sTypes]->isCdecl64;
                return LLVM::call(generator->functions[idenFunc->name + sTypes], this->getParameters(AST::funcTable[idenFunc->name + sTypes], false, AST::funcTable[idenFunc->name + sTypes]->args).data(), this->args.size(), (instanceof<TypeVoid>(AST::funcTable[idenFunc->name + sTypes]->type) ? "" : "callFunc"));
            }
            else if(AST::funcTable[idenFunc->name]->isArrayable && this->args.size() > 0) {
                // Arrayable - accepts pointer and length instead of array
                std::vector<Node*> newArgs;
                bool isChanged = false;
                for(int i=0; i<this->args.size(); i++) {
                    if(instanceof<TypeArray>(this->args[i]->getType())) {
                        isChanged = true;
                        TypeArray* tarray = (TypeArray*)this->args[i]->getType();
                        newArgs.push_back(new NodeUnary(this->loc, TokType::GetPtr, this->args[i]));
                        newArgs.push_back(new NodeInt(tarray->count));
                    }
                    else newArgs.push_back(this->args[i]);
                }
                if(isChanged) {
                    std::vector<Type*> newTypes;
                    for(int i=0; i<newArgs.size(); i++) newTypes.push_back(newArgs[i]->getType());
                    std::string newSTypes = typesToString(newTypes);
                    if(AST::funcTable.find(idenFunc->name + newSTypes) != AST::funcTable.end()) {
                        this->args = newArgs;
                        this->isCdecl64 = AST::funcTable[idenFunc->name + newSTypes]->isCdecl64;
                        return LLVM::call(generator->functions[idenFunc->name + newSTypes], this->getParameters(AST::funcTable[idenFunc->name + newSTypes], false, AST::funcTable[idenFunc->name + newSTypes]->args).data(), this->args.size(), (instanceof<TypeVoid>(AST::funcTable[idenFunc->name + newSTypes]->type) ? "" : "callFunc"));
                    }
                }
            }
            this->isCdecl64 = AST::funcTable[idenFunc->name]->isCdecl64;
            return LLVM::call(generator->functions[idenFunc->name], this->getParameters(AST::funcTable[idenFunc->name], false, AST::funcTable[idenFunc->name]->args).data(), this->args.size(), (instanceof<TypeVoid>(AST::funcTable[idenFunc->name]->type) ? "" : "callFunc"));
        }
        if(currScope->has(idenFunc->name)) {
            if(!instanceof<TypeFunc>(currScope->getVar(idenFunc->name, this->loc)->type)) {
                generator->error("undefined function '" + idenFunc->name + "'!", this->loc);
                return nullptr;
            }
            TypeFunc* fn = (TypeFunc*)currScope->getVar(idenFunc->name, this->loc)->type;
            return LLVM::call(currScope->get(idenFunc->name), this->getParameters(nullptr, false, tfaToFas(fn->args)).data(), this->args.size(), (instanceof<TypeVoid>(fn->main) ? "" : "callFunc"));
        }
        if(idenFunc->name.find('<') != std::string::npos && AST::funcTable.find(idenFunc->name.substr(0, idenFunc->name.find('<'))) != AST::funcTable.end()) {
            std::string mainName = idenFunc->name.substr(0, idenFunc->name.find('<'));

            std::string callTypes = typesToString(this->getTypes());
            if(AST::funcTable.find(mainName + callTypes) != AST::funcTable.end()) {
                if(generator->functions.find(idenFunc->name + callTypes) != generator->functions.end()) {
                    // This function is already exists - just call it
                    this->isCdecl64 = AST::funcTable[idenFunc->name + callTypes]->isCdecl64;
                    std::vector<LLVMValueRef> values = this->getParameters(AST::funcTable[idenFunc->name + callTypes], false);
                    return LLVM::call(generator->functions[idenFunc->name + callTypes], values.data(), values.size(), instanceof<TypeVoid>(AST::funcTable[idenFunc->name + callTypes]->type) ? "" : "callFunc");
                }
                mainName += callTypes;
            }

            std::string sTypes = idenFunc->name.substr(idenFunc->name.find('<') + 1);
            Lexer* tLexer = new Lexer(sTypes.substr(0, sTypes.size() - 1), 1);
            Parser* tParser = new Parser(tLexer->tokens, "(builtin)");

            std::vector<Type*> types;
            while(tParser->peek()->type != TokType::Eof) {
                types.push_back(tParser->parseType(true));
                if(tParser->peek()->type == TokType::Comma) tParser->next();
            }

            AST::funcTable[mainName]->generateWithTemplate(types, idenFunc->name + (mainName.find('[') != std::string::npos ? callTypes : ""));

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
                types.push_back(tParser->parseType(true));
                if(tParser->peek()->type == TokType::Comma) tParser->next();
            }

            for(int i=0; i<types.size(); i++) {
                while(generator->toReplace.find(types[i]->toString()) != generator->toReplace.end()) types[i] = generator->toReplace[types[i]->toString()];
                sTypes += types[i]->toString()+",";
            }

            idenFunc->name = mainName + "<" + sTypes.substr(0, sTypes.size()-1) + ">";

            if(AST::funcTable.find(idenFunc->name.substr(0, idenFunc->name.find('<'))) == AST::funcTable.end()) {
                // Not working... maybe generate structure?
                if(AST::structTable.find(idenFunc->name.substr(0, idenFunc->name.find('<'))) != AST::structTable.end()) {
                    AST::structTable[idenFunc->name.substr(0, idenFunc->name.find('<'))]->genWithTemplate("<" + sTypes.substr(0, sTypes.size()-1) + ">", types);
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
        return nullptr;
    }
    if(instanceof<NodeGet>(this->func)) {
        NodeGet* getFunc = (NodeGet*)this->func;
        if(instanceof<NodeIden>(getFunc->base)) {
            NodeIden* idenFunc = (NodeIden*)getFunc->base;
            if(!currScope->has(idenFunc->name) && !currScope->hasAtThis(idenFunc->name)) {
                generator->error("undefined variable '" + idenFunc->name + "'!", this->loc);
                return nullptr;
            }
            NodeVar* var = currScope->getVar(idenFunc->name, this->loc);
            TypeStruct* structure = nullptr;

            if(instanceof<TypeStruct>(var->type)) structure = (TypeStruct*)var->type;
            else if(instanceof<TypePointer>(var->type) && instanceof<TypeStruct>(((TypePointer*)var->type)->instance)) structure = (TypeStruct*)((TypePointer*)var->type)->instance;
            else {
                generator->error("variable '" + idenFunc->name + "' does not have structure as type!", this->loc);
                return nullptr;
            }

            if(AST::structTable.find(structure->name) != AST::structTable.end()) {
                std::vector<LLVMValueRef> params = this->getParameters(nullptr, false);
                std::vector<Type*> types;
                params.insert(params.begin(), currScope->getWithoutLoad(idenFunc->name));
                for(int i=0; i<params.size(); i++) types.push_back(lTypeToType(LLVMTypeOf(params[i])));

                std::pair<std::string, std::string> method = std::pair<std::string, std::string>(structure->name, getFunc->field);
                
                if(AST::methodTable.find(method) != AST::methodTable.end() && hasIdenticallyArgs(types, AST::methodTable[method]->args)) {
                    if(generator->functions[AST::methodTable[method]->name] == nullptr) {
                        generator->error("using '" + AST::methodTable[method]->origName + "' method before declaring it!", this->loc);
                        return nullptr;
                    }
                    params = this->correctByLLVM(params, AST::methodTable[method]->args);
                    return LLVM::call(generator->functions[AST::methodTable[method]->name], params.data(), params.size(), (instanceof<TypeVoid>(AST::methodTable[method]->type) ? "" : "callFunc"));
                }
                
                method.second += typesToString(types);
                if(AST::methodTable.find(method) != AST::methodTable.end()) {
                    if(generator->functions[AST::methodTable[method]->name] == nullptr) {
                        generator->error("using '" + AST::methodTable[method]->origName + "' method before declaring it!", this->loc);
                        return nullptr;
                    }
                    params = this->correctByLLVM(params, AST::methodTable[method]->args);
                    return LLVM::call(generator->functions[AST::methodTable[method]->name], params.data(), params.size(), (instanceof<TypeVoid>(AST::methodTable[method]->type) ? "" : "callFunc"));
                }

                method.second = method.second.substr(0, method.second.find('['));
                if(AST::methodTable.find(method) != AST::methodTable.end()) {
                    if(generator->functions[AST::methodTable[method]->name] == nullptr) {
                        generator->error("using '" + AST::methodTable[method]->origName + "' method before declaring it!", this->loc);
                        return nullptr;
                    }
                    params = this->correctByLLVM(params, AST::methodTable[method]->args);
                    return LLVM::call(generator->functions[AST::methodTable[method]->name], params.data(), params.size(), (instanceof<TypeVoid>(AST::methodTable[method]->type) ? "" : "callFunc"));
                }

                generator->error("undefined method '" + method.second.substr(0, method.second.find('[')) + "' of structure '" + structure->name + "'!", this->loc);
                return nullptr;
            }

            generator->error("undefined structure '" + structure->name + "'!", this->loc);
            return nullptr;
        }
        else if(instanceof<NodeCall>(getFunc->base)) {
            NodeCall* ncall = (NodeCall*)getFunc->base;
            LLVMValueRef result = ncall->generate();

            if(currScope->localVars.find("__RAVE_NG_NGC") != currScope->localVars.end() && currScope->localVars["__RAVE_NG_NGC"] != nullptr) delete currScope->localVars["__RAVE_NG_NGC"];

            // Creating a temp variable
            currScope->localScope["__RAVE_NG_NGC"] = result;
            currScope->localVars["__RAVE_NG_NGC"] = new NodeVar(
                "__RAVE_NG_NGC", nullptr, false, false, false, {},
                this->loc, lTypeToType(LLVMTypeOf(currScope->localScope["__RAVE_NG_NGC"]))
            );

            NodeCall* ncall2 = new NodeCall(this->loc, new NodeGet(new NodeIden("__RAVE_NG_NGC", this->loc), getFunc->field, getFunc->isMustBePtr, this->loc), this->args);
            LLVMValueRef answer = ncall2->generate();

            if(LLVMGetTypeKind(LLVMTypeOf(result)) == LLVMStructTypeKind) {
                std::string structName = LLVMGetStructName(LLVMTypeOf(result));
                if(AST::structTable.find(structName) != AST::structTable.end()) {
                    if(AST::structTable[structName]->destructor != nullptr) {
                        std::vector<LLVMValueRef> cArgs = std::vector<LLVMValueRef>({LLVMGetArgOperand(answer, 0)});
                        LLVM::call(generator->functions[AST::structTable[structName]->destructor->name], cArgs.data(), 1, ((instanceof<TypeVoid>(AST::structTable[structName]->destructor->type) ? "" : "NodeCall_destructor")));
                    }
                }
            }
            return answer;
        }
        else if(instanceof<NodeGet>(getFunc->base)) {
            NodeGet* nget = (NodeGet*)getFunc->base;

            if(currScope->localVars.find("__RAVE_NG_NG") != currScope->localVars.end() && currScope->localVars["__RAVE_NG_NG"] != nullptr) delete currScope->localVars["__RAVE_NG_NG"];

            // Creating a temp variable
            currScope->localScope["__RAVE_NG_NG"] = nget->generate();
            currScope->localVars["__RAVE_NG_NG"] = new NodeVar(
                "__RAVE_NG_NG", nullptr, false, false, false, {},
                this->loc, lTypeToType(LLVMTypeOf(currScope->localScope["__RAVE_NG_NG"]))
            );

            NodeCall* ncall2 = new NodeCall(this->loc, new NodeGet(new NodeIden("__RAVE_NG_NG", this->loc), getFunc->field, getFunc->isMustBePtr, this->loc), this->args);
            return ncall2->generate();
        }
        generator->error("a call of this kind (NodeGet + " + std::string(typeid(this->func[0]).name()) + ") is temporarily unavailable!", this->loc);
        return nullptr;
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