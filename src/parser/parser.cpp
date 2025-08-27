/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at htypep://mozilla.org/MPL/2.0/.
*/

#include <vector>
#include <map>
#include <string>
#include "../include/compiler.hpp"
#include "../include/parser/ast.hpp"
#include "../include/parser/nodes/Node.hpp"
#include "../include/parser/nodes/NodeBlock.hpp"
#include "../include/utils.hpp"
#include "../include/lexer/tokens.hpp"
#include "../include/parser/parser.hpp"
#include "../include/parser/nodes/NodeNamespace.hpp"
#include "../include/parser/nodes/NodeNone.hpp"
#include "../include/parser/nodes/NodeType.hpp"
#include "../include/parser/nodes/NodeBitcast.hpp"
#include "../include/parser/nodes/NodeFunc.hpp"
#include "../include/parser/nodes/NodeRet.hpp"
#include "../include/parser/nodes/NodeVar.hpp"
#include "../include/parser/nodes/NodeInt.hpp"
#include "../include/parser/nodes/NodeIden.hpp"
#include "../include/parser/nodes/NodeAsm.hpp"
#include "../include/parser/nodes/NodeFloat.hpp"
#include "../include/parser/nodes/NodeString.hpp"
#include "../include/parser/nodes/NodeBool.hpp"
#include "../include/parser/nodes/NodeChar.hpp"
#include "../include/parser/nodes/NodeSizeof.hpp"
#include "../include/parser/nodes/NodeCast.hpp"
#include "../include/parser/nodes/NodeItop.hpp"
#include "../include/parser/nodes/NodePtoi.hpp"
#include "../include/parser/nodes/NodeSwitch.hpp"
#include "../include/parser/nodes/NodeStruct.hpp"
#include "../include/parser/nodes/NodeNull.hpp"
#include "../include/parser/nodes/NodeBreak.hpp"
#include "../include/parser/nodes/NodeContinue.hpp"
#include "../include/parser/nodes/NodeArray.hpp"
#include "../include/parser/nodes/NodeUnary.hpp"
#include "../include/parser/nodes/NodeGet.hpp"
#include "../include/parser/nodes/NodeIndex.hpp"
#include "../include/parser/nodes/NodeBinary.hpp"
#include "../include/parser/nodes/NodeBuiltin.hpp"
#include "../include/parser/nodes/NodeAliasType.hpp"
#include "../include/parser/nodes/NodeImport.hpp"
#include "../include/parser/nodes/NodeCall.hpp"
#include "../include/parser/nodes/NodeCmpxchg.hpp"
#include "../include/parser/nodes/NodeIf.hpp"
#include "../include/parser/nodes/NodeWhile.hpp"
#include "../include/parser/nodes/NodeLambda.hpp"
#include "../include/parser/nodes/NodeFor.hpp"
#include "../include/parser/nodes/NodeForeach.hpp"
#include "../include/parser/nodes/NodeDefer.hpp"
#include "../include/parser/nodes/NodeConstStruct.hpp"
#include "../include/parser/nodes/NodeSlice.hpp"
#include "../include/parser/nodes/NodeComptime.hpp"
#include <inttypes.h>
#include <sstream>

std::map<char, int> operators;

Parser::Parser(std::vector<Token*> tokens, std::string file) 
    : tokens(tokens), file(file) {
    
    operators = {
        {TokType::Multiply, 1},
        {TokType::Divide, 1},
        {TokType::Plus, 0},
        {TokType::Minus, 0},
        {TokType::Rem, 0},
        {TokType::BitLeft, -51},
        {TokType::BitRight, -51},
        {TokType::BitXor, -55},
        {TokType::BitOr, -55},
        {TokType::Amp, -55},
        {TokType::Less, -70},
        {TokType::More, -70},
        {TokType::LessEqual, -70},
        {TokType::MoreEqual, -70},
        {TokType::Equal, -80},
        {TokType::Nequal, -80},
        {TokType::In, -80},
        {TokType::NeIn, -80},
        {TokType::Or, -85},
        {TokType::And, -85},
        {TokType::Equ, -95},
        {TokType::PluEqu, -97},
        {TokType::MinEqu, -97},
        {TokType::MulEqu, -97},
        {TokType::DivEqu, -97}
    };
}

void Parser::error(std::string msg, int line) {
    std::cout << "\033[0;31mError in \033[1m" + file + "\033[22m file at " 
              << line << " line: " << msg << "\033[0;0m" << std::endl;

    haveErrors = true;
}

void Parser::error(std::string msg) { error(msg, peek()->line); }

void Parser::warning(std::string msg, int line) {
    std::cout << "\033[0;33mWarning in \033[1m" + file + "\033[22m file at " 
              << line << " line: " << msg << "\033[0;0m" << std::endl;
}

void Parser::warning(std::string msg) { warning(msg, peek()->line); }

Token* Parser::skipStmt() {
    while (peek()->type != TokType::Eof && peek()->type != TokType::Semicolon) next();
    return peek();
}

Token* Parser::skip(int openType, int closeType) {
    int count = 1;

    while (peek()->type != TokType::Eof) {
        if (peek()->type == openType) count += 1;
        else if (peek()->type == closeType) count -= 1;

        if (count == 0) break;
        next();
    }

    return peek();
}

Token* Parser::peek() { return tokens[idx]; }

Token* Parser::next(int add) { idx += add; return tokens[idx]; }

Token* Parser::expect(int type, std::string msg, int add) {
    Token* _next = next(add);

    if (_next->type != type) {
        error(msg, _next->line);
        return nullptr;
    }

    return _next;
}

void Parser::parseAll() {
    auto start = std::chrono::steady_clock::now();

    while (peek()->type != TokType::Eof) {
        std::vector<Node*> _nodes;
        parseTopLevel(_nodes);

        for (auto node : _nodes) {
            if (node != nullptr && !instanceof<NodeNone>(node)) nodes.push_back(node);
        }
    }

    auto end = std::chrono::steady_clock::now();

    Compiler::parseTime += std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    if (haveErrors) std::exit(1);
}

void Parser::parseTopLevel(std::vector<Node*>& list, std::string s) {
    while (peek()->type == TokType::Semicolon) next();
    if (peek()->type == TokType::Eof || peek()->type == TokType::Lbra) return;

    if (peek()->value == "namespace") return list.push_back(parseNamespace(s));
    else if (peek()->value == "struct") return list.push_back(parseStruct());
    else if (peek()->value == "using") return list.push_back(parseUsing());
    else if (peek()->value == "import") return list.push_back(parseImport());
    else if (peek()->value == "type") return list.push_back(parseAliasType());
    else if (peek()->type == TokType::Builtin) {
        if (peek()->value == "@if") return list.push_back(parseIf(s, true));

        Token* tok = peek();
        next();

        std::vector<Node*> args;
        if (peek()->type == TokType::Rpar) {
            next();

            while (peek()->type != TokType::Lpar) {
                args.push_back(parseExpr(s));
                if (peek()->type == TokType::Comma) next();
            }

            next();
        }
    
        NodeBlock* block = new NodeBlock({});
        if (peek()->type == TokType::Rbra) {
            next();
            while (peek()->type != TokType::Lbra) parseTopLevel(block->nodes, s);
            next();
        }

        NodeBuiltin* nb = new NodeBuiltin(tok->value, args, tok->line, block);
        nb->isTopLevel = true;
        return list.push_back(nb);
    }
    else if (peek()->type == TokType::Rpar) {
        std::vector<DeclarMod> mods;
        next();

        while (peek()->type != TokType::Lpar) {
            std::string name = peek()->value;
            next();

            Node* value = nullptr;
            if (peek()->type == TokType::ValSel) {
                next();
                value = parseExpr(s);
            }

            mods.push_back(DeclarMod{.name = name, .value = value});

            if (peek()->type == TokType::Comma) next();
            else if (peek()->type == TokType::Lpar) break;
        }
        
        next();

        if (peek()->value != "struct") return parseDecl(list, s, mods);
        return list.push_back(parseStruct(mods));
    }

    return parseDecl(list, s);
}

Node* Parser::parseNamespace(std::string s) {
    int loc = peek()->line;
    next();

    std::string name = peek()->value;

    if (expect(TokType::Rbra, "expected token '{'!") == nullptr) {
        skip(TokType::Rbra, TokType::Lbra);
        return nullptr;
    }

    next();

    std::vector<Node*> nNodes;
    std::vector<Node*> _nodes;

    parseTopLevel(_nodes, s);

    while (_nodes.size() > 0) {
        for (size_t i=0; i<_nodes.size(); i++) nNodes.push_back(_nodes[i]);
        _nodes.clear();
        parseTopLevel(_nodes, s);
    }

    if (expect(TokType::Lbra, "expected token '}'!", 0) == nullptr) {
        next();
        return nullptr;
    }

    next();
    return new NodeNamespace(name, nNodes, loc);
}

std::vector<FuncArgSet> Parser::parseFuncArgSets() {
    std::vector<FuncArgSet> args;

    if (peek()->type != TokType::Rpar) return args;
    next();

    while (peek()->type != TokType::Lpar) {
        Type* type = parseType();
        if (type == nullptr) return args;

        std::string name = peek()->value;

        next();
        args.push_back(FuncArgSet{.name = name, .type = type, .internalTypes = {type}});

        if (peek()->type == TokType::Comma) next();
    }

    return args;
}

Node* Parser::parseBuiltin(std::string f) {
    std::string name = peek()->value;
    if (name == "@if") return parseIf(f, true);

    std::vector<Node*> args;
    NodeBlock* block;
    bool isTopLevel = false;

    next();
    if (peek()->type == TokType::Rpar) {
        next();
        while (peek()->type != TokType::Lpar) {
            args.push_back(parseExpr(f));
            if (peek()->type == TokType::Comma) next();
        }
        next();
    }

    if (peek()->type == TokType::Rbra) {
        if (f != "") block = parseBlock(f);
        else {
            block = new NodeBlock({});
            next();

            std::vector<Node*> _nodes;
            parseTopLevel(_nodes, f);

            while (_nodes.size() > 0) {
                for (size_t i=0; i<_nodes.size(); i++) block->nodes.push_back(_nodes[i]);
                _nodes.clear();
                parseTopLevel(_nodes, f);
            }

            next();
            isTopLevel = true;
        }
    }
    else block = new NodeBlock({});

    NodeBuiltin* nb = new NodeBuiltin(name, args, peek()->line, block);
    nb->isTopLevel = isTopLevel;
    return nb;
}

Node* Parser::parseOperatorOverload(Type* type, std::string s) {
    next();
    Token* _t = peek();
    std::string name = s + "(" + _t->value;
    next();

    // Build operator name
    if (peek()->type != TokType::Rpar) {
        name += peek()->value;
        next();
        if (peek()->type != TokType::Rpar) {
            name += peek()->value;
            next();
        }
    }
    name += ")";

    // Parse arguments
    std::vector<FuncArgSet> args;
    if (peek()->type == TokType::Rpar) {
        next();
        while (peek()->type != TokType::Lpar) {
            Type* argType = parseType();
            args.push_back(FuncArgSet{.name = peek()->value, .type = argType, .internalTypes = {argType}});
            next();
            if (peek()->type == TokType::Comma) next();
        }
        next();
    }

    if (peek()->type == TokType::ShortRet) {
        next();
        NodeBlock* nb = new NodeBlock({new NodeRet(parseExpr("operator"), _t->line)});
        if (peek()->type == TokType::Semicolon) next();
        return new NodeFunc(name, args, nb, false, {}, _t->line, type, {});
    }

    NodeBlock* nb = parseBlock("operator");
    if (peek()->type == TokType::ShortRet) {
        next();
        nb->nodes.push_back(new NodeRet(parseExpr("operator"), _t->line));
    }

    return new NodeFunc(name, args, nb, false, {}, _t->line, type, {});
}

void Parser::parseDecl(std::vector<Node*>& list, std::string s, std::vector<DeclarMod> _mods) {
    std::vector<DeclarMod> mods = _mods;
    int loc = 0;
    std::string name = "";
    bool isExtern = false;
    bool isVolatile = false;

    if (peek()->value == "extern") {isExtern = true; next();}
    if (peek()->value == "volatile") {isVolatile = true; next();}

    if (peek()->type == TokType::Rpar) {
        next();
        while (peek()->type != TokType::Lpar) {
            std::string name = peek()->value;
            next();
            Node* value = nullptr;
            if (peek()->type == TokType::ValSel) {
                next();
                value = parseExpr();
            }
            mods.push_back(DeclarMod{.name = name, .value = value});
            if (peek()->type == TokType::Comma) next();
            else if (peek()->type == TokType::Lpar) break;
        }
        next();
    }

    int currIdx = idx;
    auto type = parseType(false, true);

    if (type == nullptr) {
        next(2);
        if (peek()->type == TokType::Rpar) skip(TokType::Rpar, TokType::Lpar);

        if (peek()->type == TokType::ShortRet) skipStmt();
        else if (peek()->type == TokType::Rbra) skip(TokType::Rbra, TokType::Lbra);

        if (peek()->type != TokType::Eof) next();
        return;
    }

    if (instanceof<TypeCall>(type)) {
        idx = currIdx;
        Node* n = parseAtom(s);
        if (peek()->type == TokType::Semicolon) next();
        return list.push_back(n);
    }

    if (expect(TokType::Identifier, "a declaration name must be an identifier!", 0) == nullptr) {
        if (peek()->type == TokType::Semicolon) {
            next();
            return;
        }
        else if (peek()->type == TokType::Equ) {
            skipStmt();
            next();
            return;
        }

        next();

        if (peek()->type == TokType::Rpar) skip(TokType::Rpar, TokType::Lpar);

        if (peek()->type == TokType::ShortRet) skipStmt();
        else if (peek()->type == TokType::Rbra) skip(TokType::Rbra, TokType::Lbra);

        if (peek()->type != TokType::Eof) next();
        return;
    }

    if (peek()->value == "operator") return list.push_back(parseOperatorOverload(type, s));

    name = peek()->value;
    if (isBasicType(name)) error("a declaration cannot be named as a basic type!");

    loc = peek()->line;
    next();

    if (peek()->type != TokType::Less && peek()->type != TokType::Rpar && peek()->type != TokType::Rbra
    && peek()->type != TokType::ShortRet && peek()->type != TokType::Equ && peek()->type != TokType::Semicolon
    && peek()->type != TokType::Comma) {
        error("expected token ';'!", loc);
        return;
    }

    if (peek()->type == TokType::Rbra && s.find("__RAVE_PARSER_FUNCTION_") != std::string::npos) {
        error("function declarations cannot be inside other functions!", loc);
        return;
    }

    std::vector<std::string> templates;
    if (peek()->type == TokType::Less) {
        next();
        while (peek()->type != TokType::More) {
            templates.push_back(peek()->value);
            next();
            if (peek()->type == TokType::Comma) next();
        }
        next();
    }

    if (peek()->type == TokType::Rpar) {
        std::vector<FuncArgSet> args = parseFuncArgSets();
        if (peek()->type == TokType::Lpar) next();
        if (peek()->type == TokType::Rbra) next();
        if (peek()->type == TokType::ShortRet) {
            next();
            Node* expr = parseExpr();
            if (peek()->type != TokType::Lpar) {
                if (peek()->type != TokType::Semicolon) error("expected token ';'!");
                next();
            }
            return list.push_back(new NodeFunc(name, args, new NodeBlock({new NodeRet(expr, loc)}), isExtern, mods, loc, type, templates));
        }
        else if (peek()->type == TokType::Semicolon) {
            next();
            return list.push_back(new NodeFunc(name, args, nullptr, isExtern, mods, loc, type, templates));
        }

        NodeBlock* block = parseBlock(name);
        if (peek()->type == TokType::ShortRet) {
            next();
            Node* n = parseExpr();
            if (peek()->type == TokType::Semicolon) next();

            if (instanceof<TypeVoid>(type)) block->nodes.push_back(n);
            else block->nodes.push_back(new NodeRet(n, loc));
        }

        return list.push_back(new NodeFunc(name, args, block, isExtern, mods, loc, type, templates));
    }
    else if (peek()->type == TokType::Rbra) {
        NodeBlock* block = parseBlock("__RAVE_PARSER_FUNCTION_" + name);
        if (peek()->type == TokType::ShortRet) {
            next();
            Node* n = parseExpr();
            if (peek()->type == TokType::Semicolon) next();
            if (instanceof<TypeVoid>(type)) block->nodes.push_back(n);
            else block->nodes.push_back(new NodeRet(n, peek()->line));
        }
        return list.push_back(new NodeFunc(name, {}, block, isExtern, mods, loc, type, templates));
    }
    else if (peek()->type == TokType::ShortRet) {
        next();
        Node* n = parseExpr();
        if (peek()->type != TokType::Lpar) {
            if (peek()->type != TokType::Semicolon) error("expected token ';'!");
            next();
        }

        if (!instanceof<TypeVoid>(type)) return list.push_back(new NodeFunc(name, {}, new NodeBlock({new NodeRet(n, loc)}), isExtern, mods, loc, type, templates));
        return list.push_back(new NodeFunc(name, {}, new NodeBlock({n}), isExtern, mods, loc, type, templates));
    }
    else if (peek()->type == TokType::Semicolon) {
        next();
        return list.push_back(new NodeVar(name, nullptr, isExtern, instanceof<TypeConst>(type), (s == ""), mods, loc, type, isVolatile));
    }
    else if (peek()->type == TokType::Comma || peek()->type == TokType::Semicolon || peek()->type == TokType::Equ) {
        NodeVar* variable = new NodeVar(name, nullptr, isExtern, instanceof<TypeConst>(type), (s == ""), mods, loc, type, isVolatile);

        if (peek()->type == TokType::Equ) {
            next();
            variable->value = parseExpr();
        }

        list.push_back(variable);
        if (peek()->type != TokType::Semicolon) next();

        while (peek()->type != TokType::Semicolon) {
            std::string varName = peek()->value;
            next();

            Node* varValue = nullptr;

            if (peek()->type == TokType::Equ) {
                next();
                varValue = parseExpr();
            }

            if (peek()->type == TokType::Comma) next();
            list.push_back(new NodeVar(varName, varValue, isExtern, instanceof<TypeConst>(type), (s == ""), mods, loc, type, isVolatile));
        }

        next();
        return;
    }

    NodeBlock* _b = parseBlock(name);
    return list.push_back(new NodeFunc(name, {}, _b, isExtern, mods, loc, type, templates));
}

Node* Parser::parseCmpXchg(std::string s) {
    int loc = peek()->line;
    next();
    Node* ptr = parseExpr(s);
    if (peek()->type == TokType::Comma) next();
    Node* val1 = parseExpr(s);
    if (peek()->type == TokType::Comma) next();
    Node* val2 = parseExpr(s);
    if (peek()->type == TokType::Lpar) next();
    return new NodeCmpxchg(ptr, val1, val2, loc);
}

Node* Parser::parseConstantStructure(std::string structName) {
    int loc = peek()->line;

    next();
    std::vector<Node*> values;

    while (peek()->type != TokType::Lbra) {
        values.push_back(parseExpr(structName));
        if (peek()->type == TokType::Comma) next();
        else if (peek()->type != TokType::Lbra) error("expected token '}'!");
    }
    next();

    return new NodeConstStruct(structName, values, loc);
}

Node* Parser::parseAtom(std::string f) {
    Token* t = peek();
    next();

    int size = peek()->value.size();

    if (t->type == TokType::Number) {
        int expNumber = 0;

        if (peek()->value == "e") {
            // Exponent
            next();

            char sign = peek()->value[0];
            next();

            std::string exponent = peek()->value;
            next();

            if (sign == '-') expNumber = std::stoi(exponent) * -1;
            else expNumber = std::stoi(exponent);
        }

        if (peek()->type == TokType::Identifier) {
            std::string iden = peek()->value;

            if (iden == "u" || iden == "l" || iden == "c" || iden == "s") {
                NodeInt* _int = new NodeInt(BigInt(t->value));
                next();

                if (iden == "u") _int->isUnsigned = true;
                else if (iden == "l") _int->isMustBeLong = true;
                else if (iden == "c") _int->isMustBeChar = true;
                else if (iden == "s") _int->isMustBeShort = true;
                return _int;
            }
            else if (iden == "f" || iden == "d" || iden == "h" || iden == "bh" || iden == "r") {
                NodeFloat* nfloat = nullptr;

                if (expNumber != 0) {
                    if (expNumber < 0) {
                        expNumber *= -1;
                        std::string number = "0.";
                        for (int i=1; i<expNumber; i++) number += "0";

                        nfloat = new NodeFloat(std::stod(number + t->value));
                    }
                    else {
                        std::string number = t->value;
                        for (size_t i=0; i<expNumber; i++) number += "0";

                        nfloat = new NodeFloat(std::stod(t->value + number));
                    }
                }
                else nfloat = new NodeFloat(std::stod(t->value));

                next();

                if (iden == "f") nfloat->isMustBeFloat = true;
                else if (iden == "d") nfloat->type = basicTypes[BasicType::Double];
                else if (iden == "h") nfloat->type = basicTypes[BasicType::Half];
                else if (iden == "bh") nfloat->type = basicTypes[BasicType::Bhalf];
                else if (iden == "r") nfloat->type = basicTypes[BasicType::Real];
                return nfloat;
            }
        }

        return new NodeInt(BigInt(t->value));
    }

    if (t->type == TokType::FloatNumber) {
        int expNumber = 0;

        if (peek()->value == "e") {
            // Exponent
            next();

            char sign = peek()->value[0];
            next();

            std::string exponent = peek()->value;
            next();

            if (sign == '-') expNumber = std::stoi(exponent) * -1;
            else expNumber = std::stoi(exponent);
        }

        NodeFloat* value = nullptr;

        double number = std::stod(t->value);
        if (expNumber != 0) number = std::pow(10, expNumber) * number;

        if (peek()->type == TokType::Identifier) {
            std::string suffix = peek()->value;
            next();

            if (suffix == "d") value = new NodeFloat(number, basicTypes[BasicType::Double]);
            else if (suffix == "f") {
                value = new NodeFloat(number, basicTypes[BasicType::Float]);
                value->isMustBeFloat = true;
            }
            else if (suffix == "h") value = new NodeFloat(number, basicTypes[BasicType::Half]);
            else if (suffix == "bh") value = new NodeFloat(number, basicTypes[BasicType::Bhalf]);
            else if (suffix == "r") value = new NodeFloat(number, basicTypes[BasicType::Real]);
        }
        else value = new NodeFloat(number);

        return value;
    }

    if (t->type == TokType::HexNumber) {
        long long number;
        std::stringstream ss;
        ss << std::hex << t->value;
        ss >> number;
        return new NodeInt(BigInt(number));
    }

    if (t->type == TokType::True) return new NodeBool(true);

    if (t->type == TokType::False) return new NodeBool(false);

    if (t->type == TokType::String) {
        if (peek()->type == TokType::Identifier && peek()->value == "w") {
            next();
            return new NodeString(tokens[idx-2]->value, true);
        }
        return new NodeString(t->value,false);
    }

    if (t->type == TokType::Char) {
        if (peek()->type == TokType::Identifier && peek()->value == "w") {
            next();
            return new NodeChar(tokens[idx-2]->value, true);
        }
        return new NodeChar(t->value, false);
    }

    if (peek()->type == TokType::Rbra) return parseConstantStructure(t->value);

    if (t->type == TokType::Identifier) {
        if (t->value == "null") return new NodeNull(nullptr, t->line);
        else if (t->value == "cast") {
            if (peek()->type != TokType::Rpar) error("expected token '('!");
            next();
            Type* ty = parseType();
            if (peek()->type != TokType::Lpar) error("expected token ')'!");
            next();
            Node* expr = parseExpr();
            return new NodeCast(ty, expr, t->line);
        }
        else if (t->value == "sizeof") {
            if (peek()->type != TokType::Rpar) error("expected token '('!");
            next();
            NodeType* val = new NodeType(parseType(), t->line);
            if (peek()->type != TokType::Lpar) error("expected token ')'!");
            next();
            return new NodeSizeof(val, t->line);
        }
        else if (t->value == "bitcast") {
            if (peek()->type != TokType::Rpar) error("expected token '('!");
            next();
            Type* ty = parseType();
            if (peek()->type != TokType::Lpar) error("expected token ')'!");
            next();
            Node* expr = parseExpr();
            return new NodeBitcast(ty, expr, t->line);
        }
        else if (t->value == "itop") {
            if (peek()->type != TokType::Rpar) error("expected token '('!");
            next();
            Type* type = parseType();
            if (peek()->type != TokType::Comma) error("expected token ','!");
            next();
            Node* val = parseExpr();
            if (peek()->type != TokType::Lpar) error("expected token ')'!");
            next();
            return new NodeItop(val, type, t->line);
        }
        else if (t->value == "ptoi") {
            if (peek()->type != TokType::Rpar) error("expected token '('!");
            next();
            Node* val = parseExpr();
            if (peek()->type != TokType::Lpar) error("expected token ')'!");
            next();
            return new NodePtoi(val, t->line);
        }
        else if (t->value == "cmpxchg") return parseCmpXchg(f);
        else if (t->value == "asm") {
            if (peek()->type != TokType::Rpar) error("expected token '('!");
            next();
            Type* type = nullptr;
            if (peek()->type != TokType::String) {
                type = parseType();
                if (peek()->type == TokType::Comma) next();
            }
            else type = typeVoid;
            std::string line = peek()->value;
            std::string additions = "";
            std::vector<Node*> args;
            next();

            if (peek()->type == TokType::Comma) {
                next();
                additions = peek()->value;
                next();
            }
            if (peek()->type == TokType::Comma) {
                next();
                while (peek()->type != TokType::Lpar) {
                    args.push_back(parseExpr(f));
                    if (peek()->type != TokType::Lpar) next();
                }
            }
            if (peek()->type != TokType::Lpar) error("expected token ')'!");
            next();
            return new NodeAsm(line, true, type, additions, args, t->line);
        }
        else if (peek()->type == TokType::Less) {
            if (isTemplate()) {
                next();
                std::string all = t->value + "<";
                int countOfL = 0;

                while (countOfL != -1) {
                    all += peek()->value;
                    if (peek()->type == TokType::Less) countOfL += 1;
                    else if (peek()->type == TokType::More) countOfL -= 1;
                    next();
                }

                if (peek()->type == TokType::More) next();
                if (peek()->type == TokType::Rbra) return parseConstantStructure(all);
                
                // Check for lambda
                if (peek()->type == TokType::Rpar) {
                    int rpars = 0;
                    long oldIdx = idx;

                    while (rpars != -1) {
                        idx += 1;

                        if (peek()->type == TokType::Rpar) rpars += 1;
                        else if (peek()->type == TokType::Lpar) rpars -= 1;
                    }

                    next();

                    if (peek()->type == TokType::ShortRet) {
                        idx = oldIdx;
                        auto args = parseFuncArgs();
                        idx += 1;
                        return new NodeLambda(peek()->line, new TypeFunc(new TypeStruct(all), args, false), new NodeBlock({new NodeRet(parseExpr(), oldIdx)}));
                    }
                    else idx = oldIdx;
                }

                return parseCall(new NodeIden(all, peek()->line));
            }
        }
        else if (peek()->type == TokType::Rpar) {
            idx -= 1;
            if (isDefinedLambda()) return parseLambda();
            else idx += 1;
        }

        if (isBasicType(t->value)) {
            idx -= 1;
            return new NodeType(parseType(), t->line);
        }
        else {
            // Maybe this is a pointer?
            // TODO: Add support of arrays and structures
            if (peek()->type == TokType::Multiply) {
                if (tokens[idx + 1]->type == TokType::Lpar || tokens[idx + 1]->type == TokType::Rarr) {
                    idx -= 1;
                    return new NodeType(parseType(), t->line);
                }
            }
        }

        return new NodeIden(t->value, peek()->line);
    }

    if (t->type == TokType::Rpar) {
        auto e = parseExpr();
        if (peek()->type != TokType::Lpar) error("expected token ')'!");
        next();
        return e;
    }

    if (t->type == TokType::Rarr) {
        std::vector<Node*> values;
        while (peek()->type != TokType::Larr) {
            values.push_back(parseExpr());
            if (peek()->type == TokType::Comma) next();
            else if (peek()->type != TokType::Larr) error("expected token ']'!");
        }
        next();
        return new NodeArray(t->line, values);
    }

    if (t->type == TokType::Builtin) {
        std::string name = t->value;
        std::vector<Node*> args;
        if (peek()->type == TokType::Rpar) {
            next();
            while (peek()->type != TokType::Lpar) {
                if (isBasicType(peek()->value) || tokens[idx+1]->value == "*") {
                    Type* pType = parseType(true);
                    if (peek()->type != TokType::Comma && peek()->type != TokType::Lpar) {
                        char op= peek()->type;
                        Node* node;
                        if (tokens[idx+1]->type == TokType::Builtin) node = parseAtom(f);
                        else {
                            next();
                            node = new NodeType(parseType(true), peek()->line);
                        }
                        args.push_back(new NodeBinary(op, new NodeType(pType, peek()->line), node, peek()->line));
                    }
                    else args.push_back(new NodeType(pType, peek()->line));
                }
                else args.push_back(parseExpr(f));
                if (peek()->type == TokType::Comma) next();
            }
            next();
        }

        NodeBlock* block;
        if (peek()->type == TokType::Rbra) block = parseBlock(f);
        else block = new NodeBlock({});
        if (peek()->type == TokType::Identifier && tokens[idx-1]->type != TokType::Lbra && tokens[idx-1]->type != TokType::Semicolon) {
            std::string name = peek()->value;
            Node* value = nullptr;
            next();
            if (peek()->type == TokType::Equ) {
                next();
                value = parseExpr(f);
            }
            return new NodeVar(name, value, false, false, false, {}, peek()->line, new TypeBuiltin(name, args, block));
        }
        return new NodeBuiltin(name, args, t->line, block);
    }
    if (t->type == TokType::Destructor) return new NodeUnary(t->line, TokType::Destructor, parseExpr());
    error("expected a number, true/false, char, variable or expression. Got: \033[1m" + t->value + "\033[22m on \033[1m" + std::to_string(t->line) + "\033[22m line.");
    return nullptr;
}

Node* Parser::parseStruct(std::vector<DeclarMod> mods) {
    int loc = peek()->line;

    Token* __name = expect(TokType::Identifier, "a declaration name must be an identifier!");
    if (__name == nullptr) {
        next();

        if (peek()->type == TokType::Less) skip(TokType::Less, TokType::More);
        skip(TokType::Rbra, TokType::Lbra);

        if (peek()->type == TokType::Lbra) next();
    }

    std::string name = __name->value;
    next();

    std::vector<std::string> templateNames;
    if (peek()->type == TokType::Less) {
        next();
        while (peek()->type != TokType::More) {
            Type* _type = parseType();
            if (_type == nullptr) {
                skip(TokType::Less, TokType::More);
                break;
            }

            templateNames.push_back(_type->toString());
            if (peek()->type == TokType::Comma) next();
        }

        next();
    }

    std::string _exs = "";
    if (peek()->type == TokType::ValSel) {
        next();
        _exs = peek()->value;
        next();
    }

    if (expect(TokType::Rbra, "expected token '{'!", 0) == nullptr) {
        skip(TokType::Rbra, TokType::Lbra);
        if (peek()->type == TokType::Lbra) next();

        return nullptr;
    }

    next();
    
    std::vector<Node*> nodes;
    std::vector<Node*> _nodes;
    parseTopLevel(_nodes, name);

    while (_nodes.size() > 0) {
        for (size_t i=0; i<_nodes.size(); i++) nodes.push_back(_nodes[i]);
        _nodes.clear();
        parseTopLevel(_nodes, name);
    }

    if (expect(TokType::Lbra, "expected token '}'!", 0) == nullptr) return nullptr;

    next();

    return new NodeStruct(name, nodes, loc, _exs, templateNames, mods);
}

Node* Parser::parseUsing() {
    // TODO
    return nullptr;
}

std::string getDirectory2(std::string file) {
    return file.substr(0, file.find_last_of("/\\"));
}

std::string getGlobalFile(Parser* parser) {
    parser->next();
    std::string buffer = "";
    while (parser->peek()->type != TokType::More) {
        buffer += parser->peek()->value;
        parser->next();
        if (parser->peek()->type == TokType::Divide) {buffer += "/"; parser->next();}
        else if (parser->peek()->type != TokType::Divide && parser->peek()->type != TokType::More) parser->error("expected token '/' or '>'!");
    }
    return exePath + buffer;
}

Node* Parser::parseImport() {
    int loc = peek()->line;
    next();
    std::vector<ImportFile> files;

    if (peek()->type != TokType::Less && peek()->type != TokType::String) error("expected token '<' or string!");

    while (peek()->type == TokType::Less || peek()->type == TokType::String) {
        if (peek()->type == TokType::Less) files.push_back(ImportFile{getGlobalFile(this) + ".rave", true});
        else files.push_back(ImportFile{peek()->value + ".rave", false});
        next();
    }

    if (peek()->type == TokType::Semicolon) next();

    if (files.size() > 0) {
        std::vector<NodeImport*> imports;
        for (size_t i=0; i<files.size(); i++) imports.push_back(new NodeImport(files[i], std::vector<std::string>(), loc));
        return new NodeImports(imports, loc);
    }

    return new NodeImport(files[0], std::vector<std::string>(), peek()->line);
}

Node* Parser::parseAliasType() {
    int loc = peek()->line;

    std::string name = expect(TokType::Identifier, "a type name must be an identifier!")->value;
    expect(TokType::Equ, "expected token '='!");
    next();

    Type* childType = parseType();

    if (peek()->type == TokType::Semicolon) next();
    return new NodeAliasType(name, childType, loc);
}

Type* Parser::parseTypeAtom() {
    if (peek()->type == TokType::Identifier) {
        std::string id = peek()->value;
        next();

        if (id == "void") return typeVoid;
        else if (id == "auto") return new TypeAuto();
        else if (id == "const") {
            if (peek()->type == TokType::Rpar) next();
            else {error("expected token '('!"); return nullptr;}
                Type* _t = parseType();
            if (peek()->type == TokType::Lpar) next();
            else {error("expected token ')'!"); return nullptr;}
            return new TypeConst(_t); 
        }
        else return getTypeByName(id);
    }
    else if (peek()->type == TokType::Builtin) {
        if (peek()->value == "@") {
            error("expected a type name, not \033[1m" + peek()->value + "\033[22m!");
            next();

            return nullptr;
        }

        std::vector<Node*> args;
        NodeBlock* block;
        Token* info = peek();
        idx += 2;

        while (peek()->type != TokType::Lpar) {
            args.push_back(parseExpr());
            if (peek()->type == TokType::Comma) next();
        }

        if (peek()->type == TokType::Rbra) return new TypeBuiltin(info->value, args, parseBlock(""));
        return new TypeBuiltin(info->value, args, new NodeBlock({}));
    }
    else error("expected a type name, not \033[1m" + peek()->value + "\033[22m!");
    return nullptr;
}

std::vector<TypeFuncArg*> Parser::parseFuncArgs() {
    std::vector<TypeFuncArg*> buffer;
    if (peek()->type == TokType::Rpar) next();
    else error("expected token '('!");
    while (peek()->type != TokType::Lpar) {
        Type* ty = parseType();
        std::string name = "";
        if (peek()->type == TokType::Identifier) {name = peek()->value; next();}
        buffer.push_back(new TypeFuncArg(ty,name));

        if (peek()->type == TokType::Comma) next();
        else if (peek()->type != TokType::Lpar) {
            error("expected token ')'!");
            return buffer;
        }
    }
    next();
    return buffer;
}

Type* Parser::parseType(bool cannotBeTemplate, bool isDeclaration) {
    if (peek()->type == TokType::Builtin && peek()->value == "@value") {
        // @value(type, name) is used for entering values right into structure template.
        expect(TokType::Rpar, "expected token '('!");
        next();

        Type* mainType = parseType();
        if (peek()->type != TokType::Comma) error("expected token ','!");

        std::string name = expect(TokType::Identifier, "expected identifier!")->value;
        expect(TokType::Lpar, "expected token ')'!");
        next();

        return new TypeTemplateMemberDefinition(mainType, name);
    }

    Type* ty = parseTypeAtom();
    if (ty == nullptr) return nullptr;

    while (
        (peek()->type == TokType::Multiply || peek()->type == TokType::Rarr ||
        peek()->type == TokType::Rpar || (peek()->type == TokType::Less && !cannotBeTemplate)) && peek()->type != TokType::Eof
    ) {
        if (ty == nullptr) return nullptr;

        if (peek()->type == TokType::Multiply) {next(); ty = new TypePointer(ty);}
        else if (peek()->type == TokType::Rarr) {
            Node* count = nullptr;
            next();
            if (peek()->type != TokType::Larr) count = parseExpr();
            
            if (peek()->type != TokType::Larr) {
                if (tokens[idx + 1]->type != TokType::Equ) error("expected token ']'!");
            }
            else next();

            ty = new TypeArray(count, ty);
        }
        else if (peek()->type == TokType::Rpar) ty = new TypeFunc(ty, parseFuncArgs(), false);
        else {
            std::vector<Type*> tTypes;
            std::string tTypesString = "";

            next();
            while (peek()->type != TokType::More) {
                if (peek()->type == TokType::Number || peek()->type == TokType::HexNumber || peek()->type == TokType::FloatNumber) {
                    Node* number = parseAtom();
                    tTypes.push_back(new TypeTemplateMember(number->getType(), number));
                }
                else tTypes.push_back(parseType(cannotBeTemplate));

                if (peek()->type == TokType::Comma) next();
                else if (peek()->type != TokType::More) error("expected token '>'!");
            }
            next();

            for (size_t i=0; i<tTypes.size(); i++) tTypesString += tTypes[i]->toString() + ",";
            ty = new TypeStruct(ty->toString() + "<" + tTypesString.substr(0, tTypesString.size() - 1) + ">", tTypes);

            if (peek()->type == TokType::Rpar) {
                if (isDeclaration) ty = new TypeFunc(ty, parseFuncArgs(), false);
                else ty = new TypeCall(((TypeStruct*)ty)->name, parseFuncCallArgs());
            }
        }
    }
    return ty;
}

bool Parser::isSlice() {
    const int savedIdx = idx;
    int countOfRarr = 1;

    while (true) {
        Token* tok = next();

        if (tok->type == TokType::Eof) break;

        if (tok->type == TokType::Rarr) countOfRarr++;
        else if (tok->type == TokType::Larr) {
            if (--countOfRarr == 0) break;
        }
        else if (tok->type == TokType::SliceOper) {
            idx = savedIdx;
            return true;
        }
    }

    idx = savedIdx;
    return false;
}

bool Parser::isTemplate() {
    const int startIdx = idx;
    const Token* nextToken = tokens[idx + 1];

    if (nextToken->type == TokType::Number || nextToken->type == TokType::HexNumber ||
        nextToken->type == TokType::FloatNumber || nextToken->type == TokType::String ||
        nextToken->type == TokType::Rarr
    ) return false;

    next();
    char peekType = peek()->type;
    if (peekType == TokType::Number || peekType == TokType::String ||
        peekType == TokType::HexNumber || peekType == TokType::FloatNumber) {
        idx = startIdx;
        return false;
    }

    next();
    peekType = peek()->type;
    if (peekType == TokType::Multiply || peekType == TokType::Comma ||
        peekType == TokType::Rarr || peekType == TokType::More || peekType == TokType::Less) {
        if (peekType == TokType::Multiply) {
            next();
            peekType = peek()->type;
            if (peekType == TokType::Multiply || peekType == TokType::Rarr ||
                peekType == TokType::Less || peekType == TokType::More || peekType == TokType::Comma) {
                idx = startIdx;
                return true;
            }
        }
        else if (peekType == TokType::Comma || peekType == TokType::More || peekType == TokType::Less) {
            idx = startIdx;
            return true;
        }
        else if (peekType == TokType::Rarr) {
            while (peek()->type == TokType::Rarr) {
                next();
                // TODO: Alias support
                if (peek()->type != TokType::Number) {
                    idx = startIdx;
                    return false;
                }
                next();
                next();
            }
            peekType = peek()->type;
            if (peekType == TokType::Multiply || peekType == TokType::Less ||
                peekType == TokType::More || peekType == TokType::Comma) {
                idx = startIdx;
                return true;
            }
        }
    }

    idx = startIdx;
    return false;
}

Node* Parser::parsePrefix(std::string f) {
    static const std::vector<char> operators = {
        TokType::Amp,
        TokType::Multiply,
        TokType::Minus,
        TokType::Ne,
    };

    if (std::find(operators.begin(), operators.end(), peek()->type) != operators.end()) {
        auto tok = peek();
        next();

        auto tok2 = peek();
        next();

        if (tok2->value == "[" || tok2->value == ".") {
            idx -= 1;
            return new NodeUnary(tok->line, tok->type, parseSuffix(parseAtom(f), f));
        }
        else idx -= 1;

        return new NodeUnary(tok->line, tok->type, parseExpr(f));
    }

    return parseAtom(f);
}

Node* Parser::parseSuffix(Node* base, std::string f) {
    while (peek()->type == TokType::Rpar
         || peek()->type == TokType::Rarr
         || peek()->type == TokType::Dot
    ) {
        if (peek()->type == TokType::Rpar) base = parseCall(base);
        else if (peek()->type == TokType::Rarr) {
            if (isSlice()) base = parseSlice(base, f);
            else {
                base = parseIndex(base, f);
            }
        }
        else if (peek()->type == TokType::Dot) {
            next();

            bool isPtr = (peek()->type == TokType::Amp);
            if (isPtr) next();

            std::string field = peek()->value;
            next();

            if (peek()->type == TokType::Rpar) base = parseCall(new NodeGet(base, field, isPtr, peek()->line));
            else if (isPtr && peek()->type == TokType::Rarr) {
                base = new NodeGet(base, field, peek()->type == TokType::Equ || isPtr, peek()->line);
                ((NodeGet*)base)->isPtrForIndex = true;
            }
            else if (peek()->type == TokType::Less) {
                if (isTemplate()) {
                    std::vector<Type*> types;
                    std::string sTypes = "<";
                    next();
                    while (peek()->type != TokType::More) {
                        types.push_back(parseType());
                        sTypes += types[types.size() - 1]->toString() + ",";
                        if (peek()->type == TokType::Comma) next();
                    }
                    sTypes = sTypes.substr(0, sTypes.size() - 1) + ">";
                    next();
                    if (peek()->type == TokType::Rpar) base = parseCall(new NodeGet(base, field+sTypes, isPtr, peek()->line));
                    else {
                        error("Assert into parseSuffix!");
                        return nullptr;
                    }
                }
                else base = new NodeGet(base, field, peek()->type == TokType::Equ || isPtr, peek()->line);
            }
            else base = new NodeGet(base, field, peek()->type == TokType::Equ || isPtr, peek()->line);
        }
    }
    return base;
}

Node* Parser::parseIndex(Node* base, std::string f) {
    Node* index = base;
    int loc = peek()->line;

    while (peek()->type == TokType::Rarr) {
        next();
        index = new NodeIndex(index, {parseExpr(f)}, loc);
        next();
    }

    return index;
}

Node* Parser::parseCall(Node* func) {
    if (instanceof<NodeInt>(func)) error("a function name must be an identifier!");
    return new NodeCall(peek()->line, func, parseFuncCallArgs());
}

Node* Parser::parseBasic(std::string f) {return parseSuffix(parsePrefix(f), f);}

Node* Parser::parseExpr(std::string f) {
    std::vector<Token*> operatorStack;
    std::vector<Node*> nodeStack;

    nodeStack.push_back(parseBasic(f));

    while (operators.find(peek()->type) != operators.end()) {
        auto tok = peek();

        next();

        int prec = operators[tok->type];

        while (!operatorStack.empty() && prec <= operators[operatorStack.back()->type]) {
            auto rhs = nodeStack.back(); nodeStack.pop_back();
            auto lhs = nodeStack.back(); nodeStack.pop_back();

            nodeStack.push_back(new NodeBinary(operatorStack.back()->type, lhs, rhs, operatorStack.back()->line));
            operatorStack.pop_back();
        }

        operatorStack.push_back(tok);
        nodeStack.push_back(parseBasic(f));
    }

    while (!operatorStack.empty()) {
        auto rhs = nodeStack.back(); nodeStack.pop_back();
        auto lhs = nodeStack.back(); nodeStack.pop_back();

        nodeStack.push_back(new NodeBinary(operatorStack.back()->type, lhs, rhs, operatorStack.back()->line));
        operatorStack.pop_back();
    }

    return nodeStack.back();
}

Node* Parser::parseWhile(std::string f) {
    int line = peek()->line;
    next();

    Node* cond = nullptr;
    if (peek()->type == TokType::Rbra) cond = new NodeBool(true);
    else {
        next();
        cond = parseExpr(f);
        if (peek()->type == TokType::Lpar) next();
    }

    return new NodeWhile(cond, parseStmt(f), line);
}

Node* Parser::parseDefer(bool isFunctionScope, std::string f) {
    int line = peek()->line;
    next();
    return new NodeDefer(line, parseStmt(f), isFunctionScope);
}

Node* Parser::parseFor(std::string f) {
    int line = peek()->line;
    idx += 2;

    std::vector<Node*> presets;
    std::vector<Node*> afters;
    Node* cond;
    int curr = 0;

    if (peek()->type == TokType::Semicolon && tokens[idx + 1]->type == TokType::Semicolon) {
        // Infinite loop
        idx += 3;

        Node* stmt = parseStmt(f);
        if (!instanceof<NodeBlock>(stmt)) stmt = new NodeBlock(std::vector<Node*>({stmt}));
        return new NodeWhile(new NodeBool(true), (NodeBlock*)stmt, line);
    }

    while (peek()->type != TokType::Lpar) {
        if (peek()->type == TokType::Semicolon) {
            curr += 1;
            next();
        }

        if (peek()->type == TokType::Comma) next();

        if (curr == 0) {
            if (tokens[idx + 1]->type == TokType::Equ) presets.push_back(parseExpr(f));
            else if (tokens[idx + 1]->type == TokType::Semicolon) {curr += 1; next();}
            else {
                Type* type = parseType();
                std::string name = peek()->value;
                next();
                if (peek()->type == TokType::Equ) {
                    next();
                    Node* value = parseExpr(f);
                    presets.push_back(new NodeVar(name, value, false, false, false, {}, peek()->line, type));
                    if (peek()->type != TokType::Semicolon && peek()->type != TokType::Comma) curr += 1;
                }
                else presets.push_back(new NodeVar(name, nullptr, false, false, false, {}, peek()->line, type));
            }
        }
        else if (curr == 1) cond = parseExpr(f);
        else afters.push_back(parseExpr(f));
    }
    next();

    Node* stmt = parseStmt(f);
    if (!instanceof<NodeBlock>(stmt)) stmt = new NodeBlock(std::vector<Node*>({stmt}));
    return new NodeFor(presets, cond, afters, (NodeBlock*)stmt, line);
}

Node* Parser::parseForeach(std::string f) {
    int line = peek()->line;
    next(2);

    NodeIden* elName = new NodeIden(peek()->value, line);
    next();

    if (peek()->type != TokType::Semicolon) {
        error("expected token ';'!");
        return nullptr;
    }

    next();

    Node* dataVar = parseExpr(f);
    
    if (peek()->type == TokType::Lpar) {
        next();
        Node* stmt = parseStmt(f);
        if (!instanceof<NodeBlock>(stmt)) stmt = new NodeBlock(std::vector<Node*>({stmt}));
        return new NodeForeach(elName, dataVar, nullptr, (NodeBlock*)stmt, line);
    }

    if (peek()->type != TokType::Semicolon) {
        error("expected token ';'!");
        return nullptr;
    }
    
    next();
    Node* lengthVar = parseExpr(f);
    next();

    Node* stmt = parseStmt(f);
    if (!instanceof<NodeBlock>(stmt)) stmt = new NodeBlock(std::vector<Node*>({stmt}));
    return new NodeForeach(elName, dataVar, lengthVar, (NodeBlock*)stmt, line);
}

bool Parser::isTemplateVariable() {
    int oldIdx = idx;
    int count = 1;

    while (peek()->type != TokType::Less) next();
    next();

    while (count > 0) {
        if (peek()->type == TokType::Less) count += 1;
        else if (peek()->type == TokType::More) count -= 1;
        
        next();
    }

    if (peek()->type == TokType::Rpar) {
        // Check for possible lambda

        next();

        count = 1;

        while (count > 0) {
            if (peek()->type == TokType::Rpar) count += 1;
            else if (peek()->type == TokType::Lpar) count -= 1;

            next();
        }

        bool result = peek()->type == TokType::Identifier || peek()->type == TokType::Rarr || peek()->type == TokType::Multiply;

        idx = oldIdx;
        return result;
    }

    bool result = peek()->type == TokType::Identifier || peek()->type == TokType::Rarr || peek()->type == TokType::Multiply;

    idx = oldIdx;
    return result;
}

Node* Parser::parseStmt(std::string f) {
    if (peek()->type == TokType::Rbra) return parseBlock(f);
    if (peek()->type == TokType::Semicolon) {next(); return parseStmt(f);}
    if (peek()->type == TokType::Eof) return nullptr;
    if (peek()->type == TokType::Identifier) {
        std::string id = peek()->value;

        if (tokens[idx + 1]->type == TokType::Less) {
            if (isTemplateVariable()) {
                std::vector<Node*> decl;
                parseDecl(decl, f);
                return decl.size() > 1 ? new NodeBlock(decl) : decl[0];
            }
            return parseExpr(f);
        }

        if (id == "extern" || id == "volatile" || id == "const") {
            std::vector<Node*> decl;
            parseDecl(decl, f);
            return decl.size() > 1 ? new NodeBlock(decl) : decl[0];
        }
        if (id == "if") return parseIf(f, false);
        if (id == "while") return parseWhile(f);
        if (id == "for") return parseFor(f);
        if (id == "foreach") return parseForeach(f);
        if (id == "break") return parseBreak();
        if (id == "continue") return parseContinue();
        if (id == "switch") return parseSwitch(f);
        if (id == "fdefer") return parseDefer(true, f);
        if (id == "defer") return parseDefer(false, f);

        if (tokens[idx+1]->type == TokType::Rarr && tokens[idx+4]->type != TokType::Equ
           && !TokType::isParent(tokens[idx+4]->type) && !TokType::isCompoundAssigment(tokens[idx+4]->type)
           && tokens[idx+4]->type != TokType::Rarr && tokens[idx+4]->type != TokType::Dot) {
            if (tokens[idx+2]->type == TokType::Number && tokens[idx+3]->type == TokType::Larr) {
                std::vector<Node*> decl;
                parseDecl(decl, f);
                return decl.size() > 1 ? new NodeBlock(decl) : decl[0];
            }
        }
        
        next();

        if (peek()->type != TokType::Identifier) {
            if (peek()->type == TokType::Multiply) {
                idx -= 1;
                std::vector<Node*> decl;
                parseDecl(decl, f);
                return decl.size() > 1 ? new NodeBlock(decl) : decl[0];
            }
            else if (peek()->type == TokType::Rarr || peek()->type == TokType::Rpar) {
                if (isBasicType(id)) {
                    idx -= 1;
                    std::vector<Node*> decl;
                    parseDecl(decl, f);
                    return decl.size() > 1 ? new NodeBlock(decl) : decl[0];
                }
            }

            idx -= 1;

            if (peek()->type == TokType::Builtin) return parseBuiltin(f);
            Node* expr = parseExpr(f);

            if (peek()->type != TokType::Lbra) {
                if (peek()->type != TokType::Semicolon) error("expected token ';'!");
                next();
            }
            else next();
            return expr;
        }
        idx -= 1;

        std::vector<Node*> decl;
        parseDecl(decl, f);
        return decl.size() > 1 ? new NodeBlock(decl) : decl[0];
    }

    if (peek()->type == TokType::Rpar) {
        std::vector<DeclarMod> mods;
        next();

        while (peek()->type != TokType::Lpar) {
            if (peek()->type != TokType::Identifier) {
                // This is an expression
                idx -= 1;
                Node* expr = parseExpr(f);
                next();
                return expr;
            }

            next();

            std::string name = peek()->value;
            Node* value = nullptr;
            if (peek()->type == TokType::ValSel) {
                next();
                value = parseExpr();
            }

            mods.push_back(DeclarMod{.name = name, .value = value});

            if (peek()->type == TokType::Comma) next();
            else if (peek()->type == TokType::Lpar) break;
        }
        next();

        std::vector<Node*> decl;
        parseDecl(decl, f);
        return decl.size() > 1 ? new NodeBlock(decl) : decl[0];
    }

    if (peek()->type == TokType::Builtin) return parseBuiltin(f);

    Node* expr = parseExpr(f);
    if (peek()->type == TokType::Lbra) next();
    else {
        if (peek()->type != TokType::Semicolon) error("expected token ';'!");
        next();
    }

    return expr;
}

Node* Parser::parseBreak() {
    next();
    Node* nbreak = new NodeBreak(peek()->line);
    next();
    return nbreak;
}

Node* Parser::parseContinue() {
    next();
    Node* ncontinue = new NodeContinue(peek()->line);
    next();
    return ncontinue;
}

Node* Parser::parseAnyLevelBlock(std::string f) {
    if (f != "") {
        if (peek()->type != TokType::Rbra) return parseStmt(f);
        return parseBlock(f);
    }

    if (peek()->type != TokType::Rbra) {
        std::vector<Node*> decl;
        parseTopLevel(decl, f);
        return decl.size() > 1 ? new NodeBlock(decl) : decl[0];
    }

    NodeBlock* block = new NodeBlock({});
    
    next();
    while (peek()->type != TokType::Lbra) parseTopLevel(block->nodes, f);
    next();

    return block;
}

Node* Parser::parseIf(std::string f, bool isStatic) {
    int line = peek()->line;
    next();

    bool isLikely = false;
    bool isUnlikely = false;

    if (peek()->value == "likely") {isLikely = true; next();}
    else if (peek()->value == "unlikely") {isUnlikely = true; next();}

    if (peek()->type != TokType::Rpar) error("expected token '('!");

    next();
    Node* cond = parseExpr();
    if (peek()->type != TokType::Lpar) error("expected token ')'!");
    next();

    NodeIf* _if = nullptr;

    Node* body = parseAnyLevelBlock(f);

    if (peek()->value == "else" || (isStatic && peek()->value == "@else")) {
        next();
    
        if (peek()->value == "likely") {isUnlikely = true; next();}
        else if (peek()->value == "unlikely") {isLikely = true; next();}
    
        _if = new NodeIf(cond, body, parseAnyLevelBlock(f), line, isStatic);
    }
    else _if = new NodeIf(cond, body, nullptr, line, isStatic);

    _if->isLikely = isLikely;
    _if->isUnlikely = isUnlikely;

    if (isStatic) return new NodeComptime(_if);

    return _if;
}

std::pair<std::vector<Node*>, Node*> Parser::parseCase(std::string f) {
    std::vector<Node*> cases;

    while (peek()->value == "case") {
        expect(TokType::Rpar, "expected token '('!");
        next();

        Node* cond = parseExpr(f);

        if (peek()->type != TokType::Lpar) error("expected token ')'!");
        next();

        cases.push_back(cond);
    }

    Node* block = parseStmt(f);
    return std::pair<std::vector<Node*>, Node*>(cases, block);
}

Node* Parser::parseSwitch(std::string f) {
    int line = peek()->line;
    next(); next();
    Node* expr = parseExpr();
    next();

    std::vector<std::pair<std::vector<Node*>, Node*>> statements;
    Node* _default = nullptr;

    next();

    while (peek()->type != TokType::Lbra && peek()->type != TokType::Eof) {
        if (peek()->value == "case") statements.push_back(parseCase(f));
        else if (peek()->value == "default") {
            next();
            _default = parseStmt(f);
        }
        while (peek()->type == TokType::Semicolon) next();
    }

    if (peek()->type != TokType::Eof) next();

    return new NodeSwitch(expr, _default, statements, line);
}

NodeBlock* Parser::parseBlock(std::string s) {
    std::vector<Node*> nodes;
    if (peek()->type == TokType::Rbra) next();
    while (peek()->type != TokType::Lbra && peek()->type != TokType::Eof) {
        nodes.push_back(parseStmt(s));
        while (peek()->type == TokType::Semicolon) next();
    }
    if (peek()->type != TokType::Eof) next();
    return new NodeBlock(nodes);
}

Node* Parser::parseSlice(Node* base, std::string f) {
    int loc = peek()->line;
    next();
    Node* start = parseExpr(f);
    if (peek()->type != TokType::SliceOper) error("expected token \"..\"!");
    next();
    Node* end = parseExpr(f);
    if (peek()->type != TokType::Larr) error("expected token ']'!");
    next();
    return new NodeSlice(base, start, end, loc);
}

bool Parser::isDefinedLambda(bool updateIdx) {
    const auto& nextToken = tokens[idx + 1];
    if (nextToken->type != TokType::Rpar && nextToken->type != TokType::Multiply) return false;

    int oldIdx = idx;
    int cntOfRpars = 1;

    for (int i=idx + 2; i<tokens.size(); i++) {
        const auto& token = tokens[i];
        
        if (token->type == TokType::Lpar) {
            if (--cntOfRpars == 0) {
                bool result = (i + 1 < tokens.size() && (tokens[i + 1]->type == TokType::Rbra || tokens[i + 1]->type == TokType::ShortRet));
                if (!updateIdx) idx = i;
                return result;
            }
        }
        else if (token->type == TokType::Rpar) ++cntOfRpars;
    }

    return false;
}

Node* Parser::parseLambda() {
    int loc = peek()->line;
    Type* type = parseType(false, true);
    std::string name = "";

    if (peek()->type == TokType::Identifier) {
        name = peek()->value;
        next();
    }

    if (peek()->type == TokType::ShortRet) {
        next();
        Node* value = parseExpr();
        return new NodeLambda(loc, (TypeFunc*)type,new NodeBlock({new NodeRet(value, loc)}), name);
    }

    NodeBlock* b = parseBlock("lambda");
    if (peek()->type == TokType::ShortRet) {
        next();
        Node* value = parseExpr();
        b->nodes.push_back(new NodeRet(value, loc));
    }
    return new NodeLambda(loc, (TypeFunc*)type, b, name);
}

std::vector<Node*> Parser::parseFuncCallArgs() {
    std::vector<Node*> buffer;
    if (peek()->type == TokType::Rpar) next();
    else error("expected token '('!");
    while (peek()->type != TokType::Lpar) {
        if (peek()->type == TokType::Identifier) {
            if (isDefinedLambda()) buffer.push_back(parseLambda());
            else if (isBasicType(peek()->value)) buffer.push_back(new NodeType(parseType(), peek()->line));
            else buffer.push_back(parseExpr());
        }
        else buffer.push_back(parseExpr());
        if (peek()->type == TokType::Comma) next();
    } next();
    return buffer;
}
