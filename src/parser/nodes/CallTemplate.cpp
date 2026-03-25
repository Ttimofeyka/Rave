/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

// Template function handling for Call operations
// Split from NodeCall.cpp for better maintainability

#include "../../include/parser/nodes/NodeCall.hpp"
#include "../../include/parser/nodes/NodeIden.hpp"
#include "../../include/parser/nodes/NodeFunc.hpp"
#include "../../include/parser/nodes/NodeVar.hpp"
#include "../../include/parser/nodes/NodeStruct.hpp"
#include "../../include/parser/ast.hpp"
#include "../../include/parser/FuncRegistry.hpp"
#include "../../include/lexer/lexer.hpp"
#include "../../include/debug.hpp"

// Template namespace implementation
std::string Template::fromTypes(std::vector<Type*>& types) {
    std::string sTypes = "<";
    for (size_t i = 0; i < types.size(); i++)
        sTypes += types[i]->toString() + ",";
    sTypes.back() = '>';
    return sTypes;
}

std::vector<Type*> Template::parseTemplateTypes(const std::string& templateStr) {
    DEBUG_LOG(Debug::Category::Template, "Parsing template types: " + templateStr);

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

    for (size_t i = 0; i < types.size(); i++)
        Types::replaceTemplates(&types[i]);
    return types;
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

RaveValue Call::callTemplateFunction(int loc, const std::string& name, std::vector<Node*>& arguments) {
    DEBUG_LOG(Debug::Category::FuncCall, "Calling template function: " + name);

    std::vector<int> byVals;
    std::string mainName = name.substr(0, name.find('<'));
    std::string sTypes = name.substr(name.find('<') + 1, name.find('>') - name.find('<') - 1);

    std::vector<Type*> pTypes = Call::getTypes(arguments);
    std::string callTypes = typesToString(pTypes);

    // Check if already generated
    if (AST::funcTable.find(name + callTypes) != AST::funcTable.end()) {
        std::vector<RaveValue> params = Call::genParameters(arguments, byVals, AST::funcTable[name + callTypes], loc);
        return LLVM::call(generator->functions[name + callTypes], params,
            (instanceof<TypeVoid>(AST::funcTable[name + callTypes]->type) ? "" : "callFunc"), byVals);
    }

    bool presenceInFt = AST::funcTable.find(mainName) != AST::funcTable.end();

    if (presenceInFt) {
        if (AST::funcTable.find(mainName + callTypes) != AST::funcTable.end()) {
            if (generator->functions.find(name + callTypes) != generator->functions.end()) {
                std::vector<RaveValue> params = Call::genParameters(arguments, byVals, AST::funcTable[name + callTypes], loc);
                return LLVM::call(generator->functions[name + callTypes], params,
                    (instanceof<TypeVoid>(AST::funcTable[name + callTypes]->type) ? "" : "callFunc"), byVals);
            }
            mainName += callTypes;
        }
        else if ((AST::funcTable[mainName]->args.size() != arguments.size()) &&
                 !AST::funcTable[mainName]->isCdecl64 && !AST::funcTable[mainName]->isWin64 &&
                 !AST::funcTable[mainName]->isVararg)
            generator->error("wrong number of arguments for calling function \033[1m" + name +
                "\033[22m (\033[1m" + std::to_string(AST::funcTable[mainName]->args.size()) +
                "\033[22m expected, \033[1m" + std::to_string(arguments.size()) + "\033[22m provided)!", loc);
    }

    std::vector<Type*> types = Template::parseTemplateTypes(sTypes);

    if (presenceInFt) {
        sTypes = "<";
        for (size_t i = 0; i < types.size(); i++) sTypes += types[i]->toString() + ",";
        sTypes = sTypes.substr(0, sTypes.length() - 1) + ">";

        if (AST::funcTable.find(mainName + sTypes) != AST::funcTable.end())
            return Call::make(loc, new NodeIden(mainName + sTypes, loc), arguments);

        std::string sTypes2 = sTypes + typesToString(types);
        if (AST::funcTable.find(mainName + sTypes2) != AST::funcTable.end())
            return Call::make(loc, new NodeIden(mainName + sTypes2, loc), arguments);

        AST::funcTable[mainName]->generateWithTemplate(types,
            mainName + sTypes + (mainName.find('[') == std::string::npos ? callTypes : ""));
    }
    else {
        sTypes = "<";
        for (size_t i = 0; i < types.size(); i++) sTypes += types[i]->toString() + ",";
        sTypes.back() = '>';

        std::string ifName = mainName + sTypes;
        mainName = ifName.substr(0, ifName.find('<'));

        if (AST::funcTable.find(mainName) == AST::funcTable.end()) {
            if (AST::structTable.find(mainName) != AST::structTable.end())
                AST::structTable[mainName]->genWithTemplate(sTypes, types);
            else if (currScope->has("this")) {
                // Check for method on this
                NodeVar* _this = currScope->getVar("this", loc);
                if (instanceof<TypeStruct>(_this->type->getElType())) {
                    TypeStruct* _struct = (TypeStruct*)_this->type->getElType();
                    arguments.insert(arguments.begin(), new NodeIden("this", loc));
                    auto methodf = Call::findMethod(_struct->name, ifName, arguments, loc);
                    std::vector<RaveValue> params = Call::genParameters(arguments, byVals, methodf, loc);
                    if (instanceof<TypePointer>(params[0].type->getElType()))
                        params[0] = LLVM::load(params[0], "NodeCall_load", loc);
                    return LLVM::call(generator->functions[methodf->name], params,
                        (instanceof<TypeVoid>(methodf->type) ? "" : "callFunc"), byVals);
                }
            }
            else generator->error("undefined structure \033[1m" + ifName + "\033[22m!", loc);
        }

        return Call::make(loc, AST::funcTable[ifName], arguments);
    }

    return Call::make(loc, new NodeIden(name, loc), arguments);
}