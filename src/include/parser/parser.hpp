/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include "../utils.hpp"
#include <vector>
#include <string>
#include <map>
#include <iostream>
#include "../lexer/tokens.hpp"
#include "nodes/Node.hpp"
#include "nodes/NodeBlock.hpp"
#include "Types.hpp"

extern std::map<char, int> operators;

struct DeclarMod {
    std::string name;
    Node* value;
};

class Parser {
public:
    std::vector<Token*> tokens;
    long idx = 0;
    std::vector<Node*> nodes;
    std::string file;
    
    void error(std::string msg);
    void error(std::string msg, int line);
    void warning(std::string msg);
    void warning(std::string msg, int line);
    Token* peek();
    Token* next();

    Parser(std::vector<Token*> tokens, std::string file);

    void parseAll();
    void parseTopLevel(std::vector<Node*>& list, std::string s = "");
    Node* parseNamespace(std::string s = "");
    Node* parseStruct(std::vector<DeclarMod> mods = std::vector<DeclarMod>());
    Node* parseUsing();
    Node* parseImport();
    Node* parseAliasType();
    Node* parseExpr(std::string f = "");
    NodeBlock* parseBlock(std::string s = "");
    Node* parseAnyLevelBlock(std::string f);
    Node* parseAtom(std::string f = "");
    Node* parsePrefix(std::string f = "");
    Node* parseBasic(std::string f = "");
    Node* parseSuffix(Node* base, std::string f = "");
    void parseDecl(std::vector<Node*>& list, std::string f = "", std::vector<DeclarMod> mods = std::vector<DeclarMod>());
    Node* parseLambda();
    Node* parseCmpXchg(std::string s = "");
    Node* parseCall(Node* func);
    Node* parseSlice(Node* base, std::string f = "");
    Node* parseStmt(std::string f = "");
    Node* parseBuiltin(std::string f = "");
    Node* parseDefer(bool isFunctionScope, std::string f = "");
    Node* parseIf(std::string f, bool isStatic);
    Node* parseSwitch(std::string f);
    std::pair<std::vector<Node*>, Node*> parseCase(std::string f);
    Node* parseWhile(std::string f);
    Node* parseFor(std::string f);
    Node* parseForeach(std::string f);
    Node* parseOperatorOverload(Type* type, std::string s);
    Node* parseBreak();
    Node* parseContinue();
    Node* parseConstantStructure(std::string structName);
    std::vector<FuncArgSet> parseFuncArgSets();
    Node* parseIndex(Node* base, std::string f);

    bool isDefinedLambda(bool updateIdx = true);
    bool isSlice();
    bool isTemplate();
    bool isTemplateVariable();

    Type* parseTypeAtom();
    Type* parseType(bool cannotBeTemplate = false, bool isDeclaration = false);
    std::vector<TypeFuncArg*> parseFuncArgs();
    std::vector<Node*> parseFuncCallArgs();
};