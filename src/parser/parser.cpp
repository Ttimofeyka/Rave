/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at htypep://mozilla.org/MPL/2.0/.
*/

#include <vector>
#include <map>
#include <string>
#include "../include/parser/ast.hpp"
#include "../include/parser/nodes/Node.hpp"
#include "../include/parser/nodes/NodeBlock.hpp"
#include "../include/utils.hpp"
#include "../include/lexer/tokens.hpp"
#include "../include/parser/parser.hpp"
#include "../include/parser/nodes/NodeNamespace.hpp"
#include "../include/parser/nodes/NodeNone.hpp"
#include "../include/parser/nodes/NodeType.hpp"
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

Parser::Parser(std::vector<Token*> tokens, std::string file) {
    this->tokens = tokens;
    this->file = file;

    operators.clear();
    operators.insert({TokType::Plus, 0});
    operators.insert({TokType::Minus, 0});
    operators.insert({TokType::Multiply, 1});
    operators.insert({TokType::Divide, 1});
    operators.insert({TokType::Rem, 0});
    operators.insert({TokType::Equ, -95});
    operators.insert({TokType::PluEqu, -97});
    operators.insert({TokType::MinEqu, -97});
    operators.insert({TokType::MulEqu, -97});
    operators.insert({TokType::DivEqu, -97});
    operators.insert({TokType::Equal, -80});
    operators.insert({TokType::Nequal, -80});
    operators.insert({TokType::Less, -70});
    operators.insert({TokType::More, -70});
    operators.insert({TokType::LessEqual, -70});
    operators.insert({TokType::MoreEqual, -70});
    operators.insert({TokType::Or, -85});
    operators.insert({TokType::And, -85});
    operators.insert({TokType::BitLeft, -51});
    operators.insert({TokType::BitRight, -51});
    operators.insert({TokType::BitXor, -51});
    operators.insert({TokType::BitOr, -51});
}

void Parser::error(std::string msg) {
    std::cout << "\033[0;31mError in '" + this->file + "' file at " + std::to_string(this->peek()->line)+ " line: " + msg + "\033[0;0m" << std::endl;
    std::exit(1);
}

void Parser::error(std::string msg, int line) {
    std::cout << "\033[0;31mError in '" + this->file + "' file at " + std::to_string(line) + " line: " + msg + "\033[0;0m" << std::endl;
    std::exit(1);
}

void Parser::warning(std::string msg) {
    std::cout << "\033[0;33mWarning in '" + this->file + "' file at "+  std::to_string(this->peek()->line) + " line: " + msg + "\033[0;0m" << std::endl;
}

void Parser::warning(std::string msg, int line) {
    std::cout << "\033[0;33mWarning in '" + this->file + "' file at " + std::to_string(line) + " line: " + msg + "\033[0;0m" << std::endl;
}

Token* Parser::peek() {return this->tokens[this->idx];}
Token* Parser::next() {this->idx += 1; return this->tokens[this->idx];}

void Parser::parseAll() {
    while(this->peek()->type != TokType::Eof) {
        Node* n = this->parseTopLevel();
        if(n == nullptr) break;
        if(!instanceof<NodeNone>(n)) this->nodes.push_back(n);
    }
}

Node* Parser::parseTopLevel(std::string s) {
    while(this->peek()->type == TokType::Semicolon) this->next();
    if(this->peek()->type == TokType::Eof || this->peek()->type == TokType::Lbra) return nullptr;

    if(this->peek()->value == "namespace") return this->parseNamespace(s);
    else if(this->peek()->value == "struct") return this->parseStruct();
    else if(this->peek()->value == "using") return this->parseUsing();
    else if(this->peek()->value == "import") return this->parseImport();
    else if(this->peek()->value == "type") return this->parseAliasType();
    else if(this->peek()->type == TokType::Builtin) {
        if(this->peek()->value == "@if") return this->parseIf(s, true);

        Token* tok = this->peek();
        this->next();

        std::vector<Node*> args;
        if(this->peek()->type == TokType::Rpar) {
            this->next();
            while(this->peek()->type != TokType::Lpar) {
                args.push_back(this->parseExpr(s));
                if(this->peek()->type == TokType::Comma) this->next();
            } this->next();
        }
    
        NodeBlock* block = new NodeBlock({});
        if(this->peek()->type == TokType::Rbra) {
            this->next();
            while(this->peek()->type != TokType::Lbra) block->nodes.push_back(this->parseTopLevel(s));
            this->next();
        }
        NodeBuiltin* nb = new NodeBuiltin(tok->value, args, tok->line, block);
        nb->isTopLevel = true;
        return nb;
    }
    else if(this->peek()->type == TokType::Rpar) {
        std::vector<DeclarMod> mods;
        this->next();
        while(this->peek()->type != TokType::Lpar) {
            if(this->peek()->type == TokType::Builtin) {
                NodeBuiltin* nb = (NodeBuiltin*)this->parseBuiltin(s);
                mods.push_back(DeclarMod{.name = ("@" + nb->name), .value = "", .genValue = nb});
                if(this->peek()->type == TokType::Comma) this->next();
                continue;
            }
            std::string name = this->peek()->value; this->next();
            std::string value = "";
            if(this->peek()->type == TokType::ValSel) {
                this->next();
                value = this->peek()->value;
                this->next();
            }
            mods.push_back(DeclarMod{.name = name, .value = value});
            if(this->peek()->type == TokType::Comma) this->next();
            else if(this->peek()->type == TokType::Lpar) break;
        } this->next();
        if(this->peek()->value != "struct") return this->parseDecl(s, mods);
        return this->parseStruct(mods);
    }
    return this->parseDecl(s);
}

Node* Parser::parseNamespace(std::string s) {
    int loc = this->peek()->line; this->next();
    std::string name = this->peek()->value; this->next();

    if(this->peek()->type != TokType::Rbra) this->error("expected token '{'!");
    this->next();

    std::vector<Node*> nNodes;
    Node* currNode = nullptr;
    while((currNode = this->parseTopLevel(s)) != nullptr) nNodes.push_back(currNode);

    if(this->peek()->type != TokType::Lbra) this->error("expected token '}'!");
    this->next();

    return new NodeNamespace(name, nNodes, loc);
}

std::vector<FuncArgSet> Parser::parseFuncArgSets() {
    std::vector<FuncArgSet> args;
    if(this->peek()->type != TokType::Rpar) return args;
    this->next();
    while(this->peek()->type != TokType::Lpar) {
        Type* type = this->parseType();
        std::string name = this->peek()->value;
        this->next();
        args.push_back(FuncArgSet{.name = name, .type = type, .internalTypes = {type}});
        if(this->peek()->type == TokType::Comma) this->next();
    }
    return args;
}

Node* Parser::parseBuiltin(std::string f) {
    std::string name = this->peek()->value;
    if(name == "@if") return this->parseIf(f, true);

    std::vector<Node*> args;
    NodeBlock* block;
    bool isTopLevel = false;

    this->next();
    if(this->peek()->type == TokType::Rpar) {
        this->next();
        while(this->peek()->type != TokType::Lpar) {
            args.push_back(this->parseExpr(f));
            if(this->peek()->type == TokType::Comma) this->next();
        } this->next();
    }

    if(this->peek()->type == TokType::Rbra) {
        if(f != "") block = this->parseBlock(f);
        else {
            block = new NodeBlock({});
            this->next();
            Node* currNode = nullptr;
            while((currNode = this->parseTopLevel(f)) != nullptr) block->nodes.push_back(currNode);
            this->next();
            isTopLevel = true;
        }
    }
    else block = new NodeBlock({});

    NodeBuiltin* nb = new NodeBuiltin(name, args, this->peek()->line, block);
    nb->isTopLevel = isTopLevel;
    return nb;
}

Node* Parser::parseOperatorOverload(Type* type, std::string s) {
    this->next();
    std::string name = s + "(" + this->peek()->value + ")";
    Token* _t = this->peek();
    this->next();

    if(this->peek()->type != TokType::Rpar) {
        Token* _t2 = this->peek();
        name = s + "(" + _t->value+_t2->value + ")";
        this->next();
        if(this->peek()->type != TokType::Rpar) {
            name = s + "("+_t->value+_t2->value + this->peek()->value + ")";
            this->next();
        }
    }

    std::vector<FuncArgSet> args;

    if(this->peek()->type == TokType::Rpar) {
        this->next();
        while(this->peek()->type != TokType::Lpar) {
            Type* type = this->parseType();
            args.push_back(FuncArgSet{.name = this->peek()->value, .type = type, .internalTypes = {type}});
            this->next();
            if(this->peek()->type == TokType::Comma) this->next();
        } this->next();
    }

    if(this->peek()->type == TokType::ShortRet) {
        NodeBlock* nb = new NodeBlock({});
        this->next();
        nb->nodes.push_back(new NodeRet(this->parseExpr("operator"), _t->line));
        if(this->peek()->type == TokType::Semicolon) this->next();
        return new NodeFunc(name, args, nb, false, {}, _t->line, type, {});
    }

    this->next();
    NodeBlock* nb = this->parseBlock("operator");
    if(this->peek()->type == TokType::ShortRet) {
        this->next();
        nb->nodes.push_back(new NodeRet(this->parseExpr("operator"), _t->line));
    }
    return new NodeFunc(name, args, nb, false, {}, _t->line, type, {});
}

Node* Parser::parseDecl(std::string s, std::vector<DeclarMod> _mods) {
    std::vector<DeclarMod> mods = _mods;
    int loc = 0;
    std::string name = "";
    bool isExtern = false;
    bool isVolatile = false;

    if(this->peek()->value == "extern") {isExtern = true; this->next();}
    if(this->peek()->value == "volatile") {isVolatile = true; this->next();}

    if(this->peek()->type == TokType::Rpar) {
        this->next();
        while(this->peek()->type != TokType::Lpar) {
            if(this->peek()->type == TokType::Builtin) {
                NodeBuiltin* nb = (NodeBuiltin*)this->parseBuiltin();
                mods.push_back(DeclarMod{.name = "@" + nb->name, .value = "", .genValue = nb});
                if(this->peek()->type == TokType::Comma) this->next();
                continue;
            }
            std::string name = this->peek()->value; this->next();
            std::string value = "";
            if(this->peek()->type == TokType::ValSel) {
               value = this->tokens[this->idx + 1]->value;
               this->idx += 2;
            }
            mods.push_back(DeclarMod{.name = name, .value = value});
            if(this->peek()->type == TokType::Comma) this->next();
            else if(this->peek()->type == TokType::Lpar) break;
        }
        this->next();
    }

    int currIdx = this->idx;
    auto type = this->parseType();
    if(instanceof<TypeCall>(type)) {
        this->idx = currIdx;
        Node* n = this->parseAtom(s);
        if(this->peek()->type == TokType::Semicolon) this->next();
        return n;
    }
    if(this->peek()->value == "operator") return this->parseOperatorOverload(type, s);
    if(this->peek()->value == "~" && this->tokens[this->idx+1]->value == "with") {this->next(); name = "~with";}
    else {
        if(this->peek()->type != TokType::Identifier) this->error("a declaration name must be identifier!");
        name = this->peek()->value;
        if(isBasicType(name)) this->error("a declaration cannot be named as basic types!");
    }
    loc = this->peek()->line;

    this->next();

    if(this->peek()->type != TokType::Less && this->peek()->type != TokType::Rpar && this->peek()->type != TokType::Rbra
    && this->peek()->type != TokType::ShortRet && this->peek()->type != TokType::Equ && this->peek()->type != TokType::Semicolon) {
        this->error("expected token ';'!", loc);
        return nullptr;
    }

    if(this->peek()->type == TokType::Rbra && s.find("__RAVE_PARSER_FUNCTION_") != std::string::npos) {
        this->error("function declarations cannot be inside other functions!", loc);
        return nullptr;
    }

    std::vector<std::string> templates;
    if(this->peek()->type == TokType::Less) {
        this->next();
        while(this->peek()->type != TokType::More) {
            templates.push_back(this->peek()->value);
            this->next();
            if(this->peek()->type == TokType::Comma) this->next();
        } this->next();
    }

    if(this->peek()->type == TokType::Rpar) {
        std::vector<FuncArgSet> args = this->parseFuncArgSets();
        if(this->peek()->type == TokType::Lpar) this->next();
        if(this->peek()->type == TokType::Rbra) this->next();
        if(this->peek()->type == TokType::ShortRet) {
            this->next();
            Node* expr = this->parseExpr();
            if(this->peek()->type != TokType::Lpar) {
                if(this->peek()->type != TokType::Semicolon) this->error("expected token ';'!");
                this->next();
            }
            return new NodeFunc(name, args, new NodeBlock({new NodeRet(expr, loc)}), isExtern, mods, loc, type, templates);
        }
        else if(this->peek()->type == TokType::Semicolon) {
            this->next();
            return new NodeFunc(name, args, new NodeBlock({}), isExtern, mods, loc, type, templates);
        }
        NodeBlock* block = this->parseBlock(name);
        if(this->peek()->type == TokType::ShortRet) {
            this->next();
            Node* n = this->parseExpr();
            if(this->peek()->type == TokType::Semicolon) next();

            if(instanceof<TypeVoid>(type)) block->nodes.push_back(n);
            else block->nodes.push_back(new NodeRet(n, loc));
        }
        return new NodeFunc(name, args, block, isExtern, mods, loc, type, templates);
    }
    else if(this->peek()->type == TokType::Rbra) {
        NodeBlock* block = this->parseBlock("__RAVE_PARSER_FUNCTION_" + name);
        if(this->peek()->type == TokType::ShortRet) {
            this->next();
            Node* n = this->parseExpr();
            if(this->peek()->type == TokType::Semicolon) this->next();
            if(instanceof<TypeVoid>(type)) block->nodes.push_back(n);
            else block->nodes.push_back(new NodeRet(n, this->peek()->line));
        }
        return new NodeFunc(name, {}, block, isExtern, mods, loc, type, templates);
    }
    else if(this->peek()->type == TokType::ShortRet) {
        this->next();
        Node* n = this->parseExpr();
        if(this->peek()->type != TokType::Lpar) {
            if(this->peek()->type != TokType::Semicolon) this->error("expected token ';'!");
            this->next();
        }

        if(!instanceof<TypeVoid>(type)) return new NodeFunc(name, {}, new NodeBlock({new NodeRet(n, loc)}), isExtern, mods, loc, type, templates);
        return new NodeFunc(name, {}, new NodeBlock({n}), isExtern, mods, loc, type, templates);
    }
    else if(this->peek()->type == TokType::Semicolon) {
        this->next();
        return new NodeVar(name, nullptr, isExtern, instanceof<TypeConst>(type), (s==""), mods, loc, type, isVolatile);
    }
    else if(this->peek()->type == TokType::Equ) {
        this->next();
        Node* n = this->parseExpr();
        if(this->peek()->type != TokType::Lpar) {
            if(this->peek()->type != TokType::Semicolon) this->error("expected token ';'!");
            this->next();
        }
        return new NodeVar(name, n, isExtern, instanceof<TypeConst>(type), (s == ""), mods, loc, type, isVolatile);
    }
    NodeBlock* _b = this->parseBlock(name);
    return new NodeFunc(name, {}, _b, isExtern, mods, loc, type, templates);
}

Node* Parser::parseCmpXchg(std::string s) {
    int loc = this->peek()->line;
    this->next();
    Node* ptr = this->parseExpr(s);
    if(this->peek()->type == TokType::Comma) this->next();
    Node* val1 = this->parseExpr(s);
    if(this->peek()->type == TokType::Comma) this->next();
    Node* val2 = this->parseExpr(s);
    if(this->peek()->type == TokType::Lpar) this->next();
    return new NodeCmpxchg(ptr, val1, val2, loc);
}

Node* Parser::parseConstantStructure(std::string structName) {
    int loc = this->peek()->line;

    this->next();
    std::vector<Node*> values;

    while(this->peek()->type != TokType::Lbra) {
        values.push_back(parseExpr(structName));
        if(this->peek()->type == TokType::Comma) this->next();
        else if(this->peek()->type != TokType::Lbra) this->error("expected token '}'!");
    }
    this->next();

    return new NodeConstStruct(structName, values, loc);
}

Node* Parser::parseAtom(std::string f) {
    Token* t = this->peek();
    this->next();
    int size = this->peek()->value.size();
    if(t->type == TokType::Number) {
        if(size > 0 && this->peek()->type == TokType::Identifier) {
            std::string iden = this->peek()->value;

            if(iden == "u" || iden == "l" || iden == "c" || iden == "s") {
                NodeInt* _int = new NodeInt(BigInt(t->value));
                this->next();

                if(iden == "u") _int->isUnsigned = true;
                else if(iden == "l") _int->isMustBeLong = true;
                else if(iden == "c") _int->isMustBeChar = true;
                else if(iden == "s") _int->isMustBeShort = true;
                return _int;
            }
            else if(iden == "f" || iden == "d" || iden == "h" || iden == "bh") {
                NodeFloat* nfloat = new NodeFloat(std::stod(t->value));
                this->next();

                if(iden == "f") nfloat->isMustBeFloat = true;
                else if(iden == "d") nfloat->type = basicTypes[BasicType::Double];
                else if(iden == "h") nfloat->type = basicTypes[BasicType::Half];
                else if(iden == "bh") nfloat->type = basicTypes[BasicType::Bhalf];
                return nfloat;
            }
        }
        return new NodeInt(BigInt(t->value));
    }
    if(t->type == TokType::FloatNumber) {
        if(size > 0 && this->peek()->type == TokType::Identifier) {
            std::string suffix = this->peek()->value;
            this->next();

            if(suffix == "d") return new NodeFloat(std::stod(t->value), basicTypes[BasicType::Double]);
            else if(suffix == "f") {
                NodeFloat* nfloat = new NodeFloat(std::stod(t->value), basicTypes[BasicType::Float]);
                nfloat->isMustBeFloat = true;
                return nfloat;
            }
            else if(suffix == "h") return new NodeFloat(std::stod(t->value), basicTypes[BasicType::Half]);
            else if(suffix == "bh") return new NodeFloat(std::stod(t->value), basicTypes[BasicType::Bhalf]);
        }
        return new NodeFloat(std::stod(t->value));
    }
    if(t->type == TokType::HexNumber) {
        long long number;
        std::stringstream ss;
        ss << std::hex << t->value;
        ss >> number;
        return new NodeInt(BigInt(number));
    }
    if(t->type == TokType::True) return new NodeBool(true);
    if(t->type == TokType::False) return new NodeBool(false);
    if(t->type == TokType::String) {
        if(this->peek()->type == TokType::Identifier && this->peek()->value == "w") {
            this->next();
            return new NodeString(this->tokens[this->idx-2]->value, true);
        }
        return new NodeString(t->value,false);
    }
    if(t->type == TokType::Char) {
        if(this->peek()->type == TokType::Identifier && this->peek()->value == "w") {
            this->next();
            return new NodeChar(this->tokens[this->idx-2]->value, true);
        }
        return new NodeChar(t->value, false);
    }
    if(this->peek()->type == TokType::Rbra) return parseConstantStructure(t->value);
    if(t->type == TokType::Identifier) {
        if(t->value == "null") return new NodeNull(nullptr, t->line);
        else if(t->value == "cast") {
            if(this->peek()->type != TokType::Rpar) this->error("expected token '('!");
            this->next();
            Type* ty = this->parseType();
            if(this->peek()->type != TokType::Lpar) this->error("expected token ')'!");
            this->next();
            Node* expr = this->parseExpr();
            return new NodeCast(ty, expr, t->line);
        }
        else if(t->value == "sizeof") {
            if(this->peek()->type != TokType::Rpar) this->error("expected token '('!");
            this->next();
            NodeType* val = new NodeType(this->parseType(), t->line);
            if(this->peek()->type != TokType::Lpar) this->error("expected token ')'!");
            this->next();
            return new NodeSizeof(val, t->line);
        }
        else if(t->value == "itop") {
            if(this->peek()->type != TokType::Rpar) this->error("expected token '('!");
            this->next();
            Type* type = this->parseType();
            if(this->peek()->type != TokType::Comma) this->error("expected token ','!");
            this->next();
            Node* val = this->parseExpr();
            if(this->peek()->type != TokType::Lpar) this->error("expected token ')'!");
            this->next();
            return new NodeItop(val, type, t->line);
        }
        else if(t->value == "ptoi") {
            if(this->peek()->type != TokType::Rpar) this->error("expected token '('!");
            this->next();
            Node* val = this->parseExpr();
            if(this->peek()->type != TokType::Lpar) this->error("expected token ')'!");
            this->next();
            return new NodePtoi(val, t->line);
        }
        else if(t->value == "cmpxchg") return parseCmpXchg(f);
        else if(t->value == "asm") {
            if(this->peek()->type != TokType::Rpar) this->error("expected token '('!");
            this->next();
            Type* type = nullptr;
            if(this->peek()->type != TokType::String) {
                type = this->parseType();
                if(this->peek()->type == TokType::Comma) this->next();
            }
            else type = typeVoid;
            std::string line = this->peek()->value;
            std::string additions = "";
            std::vector<Node*> args;
            this->next();

            if(this->peek()->type == TokType::Comma) {
                this->next();
                additions = this->peek()->value;
                this->next();
            }
            if(this->peek()->type == TokType::Comma) {
                this->next();
                while(this->peek()->type != TokType::Lpar) {
                    args.push_back(this->parseExpr(f));
                    if(this->peek()->type != TokType::Lpar) this->next();
                }
            }
            if(this->peek()->type != TokType::Lpar) this->error("expected token ')'!");
            this->next();
            return new NodeAsm(line, true, type, additions, args, t->line);
        }
        else if(this->peek()->type == TokType::Less) {
            if(this->isTemplate()) {
                this->next();
                std::string all = t->value + "<";
                int countOfL = 0;
                while(countOfL != -1) {
                    all += this->peek()->value;
                    if(this->peek()->type == TokType::Less) countOfL += 1;
                    else if(this->peek()->type == TokType::More) countOfL -= 1;
                    this->next();
                }
                if(this->peek()->type == TokType::More) this->next();
                if(this->peek()->type == TokType::Rbra) return parseConstantStructure(all);
                return this->parseCall(new NodeIden(all, this->peek()->line));
            }
        }
        else if(this->peek()->type == TokType::Rpar) {
            this->idx -= 1;
            if(isDefinedLambda()) return parseLambda();
            else this->idx += 1;
        }

        if(isBasicType(t->value)) {
            this->idx -= 1;
            return new NodeType(parseType(), t->line);
        }

        return new NodeIden(t->value, this->peek()->line);
    }
    if(t->type == TokType::Rpar) {
        auto e = this->parseExpr();
        if(this->peek()->type != TokType::Lpar) this->error("expected token ')'!");
        this->next();
        return e;
    }
    if(t->type == TokType::Rarr) {
        std::vector<Node*> values;
        while(this->peek()->type != TokType::Larr) {
            values.push_back(this->parseExpr());
            if(this->peek()->type == TokType::Comma) this->next();
            else if(this->peek()->type != TokType::Larr) this->error("expected token ']'!");
        }
        this->next();
        return new NodeArray(t->line, values);
    }
    if(t->type == TokType::Builtin) {
        std::string name = t->value;
        std::vector<Node*> args;
        if(this->peek()->type == TokType::Rpar) {
            this->next();
            while(this->peek()->type != TokType::Lpar) {
                if(isBasicType(this->peek()->value) || this->tokens[this->idx+1]->value == "*") {
                    Type* pType = this->parseType(true);
                    if(this->peek()->type != TokType::Comma && this->peek()->type != TokType::Lpar) {
                        char op= this->peek()->type;
                        Node* node;
                        if(this->tokens[this->idx+1]->type == TokType::Builtin) node = this->parseAtom(f);
                        else {
                            this->next();
                            node = new NodeType(this->parseType(true), this->peek()->line);
                        }
                        args.push_back(new NodeBinary(op, new NodeType(pType, this->peek()->line), node, this->peek()->line));
                    }
                    else args.push_back(new NodeType(pType, this->peek()->line));
                }
                else args.push_back(this->parseExpr(f));
                if(this->peek()->type == TokType::Comma) this->next();
            }
            this->next();
        }

        NodeBlock* block;
        if(this->peek()->type == TokType::Rbra) block = parseBlock(f);
        else block = new NodeBlock({});
        if(this->peek()->type == TokType::Identifier && this->tokens[this->idx-1]->type != TokType::Lbra && this->tokens[this->idx-1]->type != TokType::Semicolon) {
            std::string name = this->peek()->value;
            Node* value = nullptr;
            this->next();
            if(this->peek()->type == TokType::Equ) {
                this->next();
                value = this->parseExpr(f);
            }
            return new NodeVar(name, value, false, false, false, {}, this->peek()->line, new TypeBuiltin(name, args, block));
        }
        return new NodeBuiltin(name, args, t->line, block);
    }
    if(t->type == TokType::Destructor) return new NodeUnary(t->line, TokType::Destructor, this->parseExpr());
    this->error("expected a number, true/false, char, variable or expression. Got: '" + t->value + "' on " + std::to_string(t->line) + " line.");
    return nullptr;
}

Node* Parser::parseStruct(std::vector<DeclarMod> mods) {
    int loc = this->peek()->line;

    this->next();
    if(this->peek()->type != TokType::Identifier) this->error("a declaration name must be identifier!");
    std::string name = this->peek()->value;
    this->next();

    std::vector<std::string> templateNames;
    if(this->peek()->type == TokType::Less) {
        this->next();
        while(this->peek()->type != TokType::More) {
            templateNames.push_back(this->parseType()->toString());
            if(this->peek()->type == TokType::Comma) this->next();
        }
        this->next();
    }

    std::string _exs = "";
    if(this->peek()->type == TokType::ValSel) {
        next();
        _exs = this->peek()->value;
        next();
    }

    if(this->peek()->type != TokType::Rbra) this->error("expected token '{'!");
    this->next(); // skip {
    
    std::vector<Node*> nodes;
    Node* currNode = nullptr;
    while((currNode = this->parseTopLevel(name)) != nullptr) nodes.push_back(currNode);

    if(this->peek()->type != TokType::Lbra) this->error("expected token '}'!");
    this->next(); // skip }

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
    while(parser->peek()->type != TokType::More) {
        buffer += parser->peek()->value;
        parser->next();
        if(parser->peek()->type == TokType::Divide) {buffer += "/"; parser->next();}
        else if(parser->peek()->type != TokType::Divide && parser->peek()->type != TokType::More) parser->error("expected token '/' or '>'!");
    }
    return exePath + buffer;
}

Node* Parser::parseImport() {
    int loc = this->peek()->line;
    this->next();
    std::vector<std::string> files;

    if(this->peek()->type != TokType::Less && this->peek()->type != TokType::String) this->error("expected token '<' or string!");

    while(this->peek()->type == TokType::Less || this->peek()->type == TokType::String) {
        if(this->peek()->type == TokType::Less) files.push_back(getGlobalFile(this) + ".rave");
        else files.push_back(getDirectory2(AST::mainFile) + "/" + this->peek()->value + ".rave");
        this->next();
    }
    if(this->peek()->type == TokType::Semicolon) this->next();

    if(files.size() > 0) {
        std::vector<NodeImport*> imports;
        for(int i=0; i<files.size(); i++) imports.push_back(new NodeImport(files[i], std::vector<std::string>(), loc));
        return new NodeImports(imports, loc);
    }
    return new NodeImport(file, std::vector<std::string>(), this->peek()->line);
}

Node* Parser::parseAliasType() {
    int loc = this->peek()->line;

    this->next();
    if(this->peek()->type != TokType::Identifier) this->error("a type name must be identifier!");
    std::string name = this->peek()->value;
    this->next();

    if(this->peek()->type != TokType::Equ) this->error("expected token '='!");
    this->next();

    Type* childType = this->parseType();
    if(this->peek()->type == TokType::Semicolon) this->next();
    return new NodeAliasType(name, childType, loc);
}

Type* Parser::parseTypeAtom() {
    if(this->peek()->type == TokType::Identifier) {
        std::string id = this->peek()->value;
        this->next();
        if(id == "void") return typeVoid;
        else if(id == "auto") return new TypeAuto();
        else if(id == "const") {
            if(this->peek()->type == TokType::Rpar) this->next();
            else this->error("expected token '('!");
                Type* _t = this->parseType();
            if(this->peek()->type == TokType::Lpar) this->next();
            else this->error("expected token ')'!");
            return new TypeConst(_t); 
        }
        else return getType(id);
    }
    else if(this->peek()->type == TokType::Builtin) {
        std::vector<Node*> args;
        NodeBlock* block;
        Token* info = this->peek(); this->idx += 2;
        while(this->peek()->type != TokType::Lpar) {
            args.push_back(this->parseExpr());
            if(this->peek()->type == TokType::Comma) this->next();
        }
        if(this->peek()->type == TokType::Rbra) return new TypeBuiltin(info->value, args, parseBlock(""));
        return new TypeBuiltin(info->value, args, new NodeBlock({}));
    }
    else this->error("expected a type name, not '" + this->peek()->value + "'!");
    return nullptr;
}

std::vector<TypeFuncArg*> Parser::parseFuncArgs() {
    std::vector<TypeFuncArg*> buffer;
    if(this->peek()->type == TokType::Rpar) this->next();
    else this->error("expected token '('!");
    while(this->peek()->type != TokType::Lpar) {
        Type* ty = this->parseType();
        std::string name = "";
        if(this->peek()->type == TokType::Identifier) {name = this->peek()->value; this->next();}
        buffer.push_back(new TypeFuncArg(ty,name));

        if(this->peek()->type == TokType::Comma) this->next();
        else if(this->peek()->type != TokType::Lpar) this->error("expected token ')'!");
    } this->next();
    return buffer;
}

Type* Parser::parseType(bool cannotBeTemplate) {
    if(this->peek()->type == TokType::Builtin && this->peek()->value == "@value") {
        // @value(type, name) is used for entering values right into structure template.
        this->next();
        if(this->peek()->type != TokType::Rpar) this->error("expected token '('!");
        this->next();

        Type* mainType = this->parseType();
        if(this->peek()->type != TokType::Comma) this->error("expected token ','!");
        this->next();

        if(this->peek()->type != TokType::Identifier) this->error("expected identifier!");
        std::string name = this->peek()->value;
        this->next();

        if(this->peek()->type != TokType::Lpar) this->error("expected token ')'!");
        this->next();

        return new TypeTemplateMemberDefinition(mainType, name);
    }

    Type* ty = this->parseTypeAtom();
    while(
        this->peek()->type == TokType::Multiply || this->peek()->type == TokType::Rarr ||
        this->peek()->type == TokType::Rpar || (this->peek()->type == TokType::Less && !cannotBeTemplate)
    ) {
        if(this->peek()->type == TokType::Multiply) {this->next(); ty = new TypePointer(ty);}
        else if(this->peek()->type == TokType::Rarr) {
            Node* count = nullptr;
            this->next();
            if(peek()->type != TokType::Larr) count = parseExpr();
            
            if(this->peek()->type != TokType::Larr) {
                if(this->tokens[this->idx + 1]->type != TokType::Equ) this->error("expected token ']'!");
            }
            else this->next();

            ty = new TypeArray(count, ty);
        }
        else if(this->peek()->type == TokType::Rpar) ty = new TypeFunc(ty, this->parseFuncArgs(), false);
        else {
            std::vector<Type*> tTypes;
            std::string tTypesString = "";
            this->next();
            while(this->peek()->type != TokType::More) {
                if(this->peek()->type == TokType::Number || this->peek()->type == TokType::HexNumber || this->peek()->type == TokType::FloatNumber) {
                    Node* number = this->parseAtom();
                    tTypes.push_back(new TypeTemplateMember(number->getType(), number));
                }
                else tTypes.push_back(this->parseType(cannotBeTemplate));

                if(this->peek()->type == TokType::Comma) this->next();
                else if(this->peek()->type != TokType::More) this->error("expected token '>'!");
            } this->next();
            for(int i=0; i<tTypes.size(); i++) tTypesString += tTypes[i]->toString() + ",";
            ty = new TypeStruct(ty->toString() + "<" + tTypesString.substr(0, tTypesString.size() - 1) + ">", tTypes);
            if(this->peek()->type == TokType::Rpar) ty = new TypeCall(((TypeStruct*)ty)->name,this->parseFuncCallArgs());
        }
    }
    return ty;
}

bool Parser::isSlice() {
    int idx = this->idx;
    this->next();
    int countOfRarr = 1;
    while(countOfRarr != 0) {
        if(this->peek()->type == TokType::Rarr) countOfRarr += 1;
        else if(this->peek()->type == TokType::Larr) countOfRarr -= 1;
        else if(this->peek()->type == TokType::SliceOper) {
            this->idx = idx;
            return true;
        }
        next();
    }
    this->idx = idx;
    return false;
}

bool Parser::isTemplate() {
    const int startIdx = this->idx;
    const Token* nextToken = this->tokens[this->idx + 1];

    if(nextToken->type == TokType::Number || nextToken->type == TokType::HexNumber ||
        nextToken->type == TokType::FloatNumber || nextToken->type == TokType::String ||
        nextToken->type == TokType::Rarr
    ) return false;

    this->next();
    char peekType = this->peek()->type;
    if(peekType == TokType::Number || peekType == TokType::String ||
        peekType == TokType::HexNumber || peekType == TokType::FloatNumber) {
        this->idx = startIdx;
        return false;
    }

    this->next();
    peekType = this->peek()->type;
    if(peekType == TokType::Multiply || peekType == TokType::Comma ||
        peekType == TokType::Rarr || peekType == TokType::More || peekType == TokType::Less) {
        if(peekType == TokType::Multiply) {
            this->next();
            peekType = this->peek()->type;
            if(peekType == TokType::Multiply || peekType == TokType::Rarr ||
                peekType == TokType::Less || peekType == TokType::More || peekType == TokType::Comma) {
                this->idx = startIdx;
                return true;
            }
        }
        else if(peekType == TokType::Comma || peekType == TokType::More || peekType == TokType::Less) {
            this->idx = startIdx;
            return true;
        }
        else if(peekType == TokType::Rarr) {
            while(this->peek()->type == TokType::Rarr) {
                this->next();
                // TODO: Alias support
                if(this->peek()->type != TokType::Number) {
                    this->idx = startIdx;
                    return false;
                }
                this->next();
                this->next();
            }
            peekType = this->peek()->type;
            if(peekType == TokType::Multiply || peekType == TokType::Less ||
                peekType == TokType::More || peekType == TokType::Comma) {
                this->idx = startIdx;
                return true;
            }
        }
    }

    this->idx = startIdx;
    return false;
}

Node* Parser::parsePrefix(std::string f) {
    static const std::vector<char> operators = {
        TokType::GetPtr,
        TokType::Multiply,
        TokType::Minus,
        TokType::Ne,
    };

    if(std::find(operators.begin(), operators.end(), this->peek()->type) != operators.end()) {
        auto tok = this->peek();
        this->next();
        auto tok2 = this->peek();
        this->next();
        if(tok2->value == "[" || tok2->value == ".") {
            this->idx -= 1;
            return new NodeUnary(tok->line, tok->type, this->parseSuffix(this->parseAtom(f), f));
        }
        else this->idx -= 1;
        return new NodeUnary(tok->line, tok->type, this->parsePrefix(f));
    }

    return this->parseAtom(f);
}

Node* Parser::parseSuffix(Node* base, std::string f) {
    while(this->peek()->type == TokType::Rpar
         || this->peek()->type == TokType::Rarr
         || this->peek()->type == TokType::Dot
    ) {
        if(this->peek()->type == TokType::Rpar) base = this->parseCall(base);
        else if(this->peek()->type == TokType::Rarr) {
            if(this->isSlice()) base = parseSlice(base, f);
            else {
                base = new NodeIndex(base, this->parseIndexes(), this->peek()->line);
            }
        }
        else if(this->peek()->type == TokType::Dot) {
            this->next();

            bool isPtr = (this->peek()->type == TokType::GetPtr);
            if(isPtr) this->next();

            std::string field = this->peek()->value;
            this->next();

            if(this->peek()->type == TokType::Rpar) base = this->parseCall(new NodeGet(base, field, isPtr, this->peek()->line));
            else if(isPtr && this->peek()->type == TokType::Rarr) {
                base = new NodeGet(base, field, this->peek()->type == TokType::Equ || isPtr, this->peek()->line);
                ((NodeGet*)base)->isPtrForIndex = true;
            }
            else if(this->peek()->type == TokType::Less) {
                if(this->isTemplate()) {
                    std::vector<Type*> types;
                    std::string sTypes = "<";
                    this->next();
                    while(this->peek()->type != TokType::More) {
                        types.push_back(this->parseType());
                        sTypes += types[types.size() - 1]->toString() + ",";
                        if(this->peek()->type == TokType::Comma) this->next();
                    }
                    sTypes = sTypes.substr(0, sTypes.size() - 1) + ">";
                    this->next();
                    if(this->peek()->type == TokType::Rpar) base = this->parseCall(new NodeGet(base, field+sTypes, isPtr, this->peek()->line));
                    else {
                        error("Assert into parseSuffix!");
                        return nullptr;
                    }
                }
                else base = new NodeGet(base, field, this->peek()->type == TokType::Equ || isPtr, this->peek()->line);
            }
            else base = new NodeGet(base, field, this->peek()->type == TokType::Equ || isPtr, this->peek()->line);
        }
    }
    return base;
}

std::vector<Node*> Parser::parseIndexes() {
    std::vector<Node*> buffer;

    while(this->peek()->type == TokType::Rarr) {
        this->next();
        buffer.push_back(this->parseExpr());
        this->next();
    }

    return buffer;
}

Node* Parser::parseCall(Node* func) {
    if(instanceof<NodeInt>(func)) this->error("a function name must be identifier!");

    return new NodeCall(this->peek()->line, func, this->parseFuncCallArgs());
}

Node* Parser::parseBasic(std::string f) {return this->parseSuffix(this->parsePrefix(f), f);}

Node* Parser::parseExpr(std::string f) {
    std::vector<Token*> operatorStack;
    std::vector<Node*> nodeStack;

    nodeStack.push_back(parseBasic(f));

    while(operators.find(peek()->type) != operators.end()) {
        auto tok = peek();

        next();

        int prec = operators[tok->type];

        while(!operatorStack.empty() && prec <= operators[operatorStack.back()->type]) {
            auto rhs = nodeStack.back(); nodeStack.pop_back();
            auto lhs = nodeStack.back(); nodeStack.pop_back();

            nodeStack.push_back(new NodeBinary(operatorStack.back()->type, lhs, rhs, operatorStack.back()->line));
            operatorStack.pop_back();
        }

        operatorStack.push_back(tok);
        nodeStack.push_back(this->parseBasic(f));
    }

    while(!operatorStack.empty()) {
        auto rhs = nodeStack.back(); nodeStack.pop_back();
        auto lhs = nodeStack.back(); nodeStack.pop_back();

        nodeStack.push_back(new NodeBinary(operatorStack.back()->type, lhs, rhs, operatorStack.back()->line));
        operatorStack.pop_back();
    }

    return nodeStack.back();
}

Node* Parser::parseWhile(std::string f) {
    int line = this->peek()->line;
    this->next();

    Node* cond = nullptr;
    if(this->peek()->type == TokType::Rbra) cond = new NodeBool(true);
    else {
        this->next();
        cond = this->parseExpr(f);
        if(this->peek()->type == TokType::Lpar) this->next();
    }

    return new NodeWhile(cond, parseStmt(f), line);
}

Node* Parser::parseDefer(bool isFunctionScope, std::string f) {
    int line = this->peek()->line;
    this->next();
    return new NodeDefer(parseStmt(f), line, isFunctionScope);
}

Node* Parser::parseFor(std::string f) {
    int line = this->peek()->line;
    this->idx += 2;

    std::vector<Node*> presets;
    std::vector<Node*> afters;
    Node* cond;
    int curr = 0;

    if(this->peek()->type == TokType::Semicolon && this->tokens[this->idx + 1]->type == TokType::Semicolon) {
        // Infinite loop
        this->idx += 3;

        Node* stmt = this->parseStmt(f);
        if(!instanceof<NodeBlock>(stmt)) stmt = new NodeBlock(std::vector<Node*>({stmt}));
        return new NodeWhile(new NodeBool(true), (NodeBlock*)stmt, line);
    }

    while(this->peek()->type != TokType::Lpar) {
        if(this->peek()->type == TokType::Semicolon) {
            curr += 1;
            this->next();
        }
        if(this->peek()->type == TokType::Comma) this->next();

        if(curr == 0) {
            if(this->tokens[this->idx + 1]->type == TokType::Equ) presets.push_back(this->parseExpr(f));
            else {
                Type* type = this->parseType();
                std::string name = this->peek()->value;
                this->next();
                if(this->peek()->type == TokType::Equ) {
                    this->next();
                    Node* value = this->parseExpr(f);
                    presets.push_back(new NodeVar(name, value, false, false, false, {}, this->peek()->line, type));
                    if(this->peek()->type != TokType::Semicolon && this->peek()->type != TokType::Comma) curr += 1;
                }
                else presets.push_back(new NodeVar(name, nullptr, false, false, false, {}, this->peek()->line, type));
            }
        }
        else if(curr == 1) cond = this->parseExpr(f);
        else afters.push_back(this->parseExpr(f));
    }
    this->next();

    Node* stmt = this->parseStmt(f);
    if(!instanceof<NodeBlock>(stmt)) stmt = new NodeBlock(std::vector<Node*>({stmt}));
    return new NodeFor(presets, cond, afters, (NodeBlock*)stmt, line);
}

Node* Parser::parseForeach(std::string f) {
    int line = this->peek()->line;
    this->idx += 2;

    NodeIden* elName = new NodeIden(this->peek()->value, line);
    this->idx += 1;

    if(this->peek()->type != TokType::Semicolon) {
        this->error("expected token ';'!");
        return nullptr;
    } this->idx += 1;

    Node* dataVar = this->parseExpr(f);
    
    if(this->peek()->type == TokType::Lpar) {
        this->idx += 1;
        Node* stmt = this->parseStmt(f);
        if(!instanceof<NodeBlock>(stmt)) stmt = new NodeBlock(std::vector<Node*>({stmt}));
        return new NodeForeach(elName, dataVar, nullptr, (NodeBlock*)stmt, line);
    }

    if(this->peek()->type != TokType::Semicolon) {
        this->error("expected token ';'!");
        return nullptr;
    } this->idx += 1;

    Node* lengthVar = this->parseExpr(f);

    this->idx += 1;

    Node* stmt = this->parseStmt(f);
    if(!instanceof<NodeBlock>(stmt)) stmt = new NodeBlock(std::vector<Node*>({stmt}));
    return new NodeForeach(elName, dataVar, lengthVar, (NodeBlock*)stmt, line);
}

bool Parser::isTemplateVariable() {
    int oldIdx = this->idx;
    int count = 1;

    while(this->peek()->type != TokType::Less) this->next();
    this->next();

    while(count > 0) {
        if(this->peek()->type == TokType::Less) count += 1;
        else if(this->peek()->type == TokType::More) count -= 1;
        
        this->next();
    }

    if(this->peek()->type == TokType::Rpar) {
        // Check for possible lambda

        this->next();

        count = 1;

        while(count > 0) {
            if(this->peek()->type == TokType::Rpar) count += 1;
            else if(this->peek()->type == TokType::Lpar) count -= 1;

            this->next();
        }

        bool result = this->peek()->type == TokType::Identifier || this->peek()->type == TokType::Rarr || this->peek()->type == TokType::Multiply;

        this->idx = oldIdx;
        return result;
    }

    bool result = this->peek()->type == TokType::Identifier || this->peek()->type == TokType::Rarr || this->peek()->type == TokType::Multiply;

    this->idx = oldIdx;
    return result;
}

Node* Parser::parseStmt(std::string f) {
    if(this->peek()->type == TokType::Rbra) return this->parseBlock(f);
    if(this->peek()->type == TokType::Semicolon) {this->next(); return this->parseStmt(f);}
    if(this->peek()->type == TokType::Eof) return nullptr;
    if(this->peek()->type == TokType::Identifier) {
        std::string id = this->peek()->value;

        if(this->tokens[this->idx + 1]->type == TokType::Less) {
            if(isTemplateVariable()) return this->parseDecl(f);
            return this->parseExpr(f);
        }

        if(id == "extern" || id == "volatile" || id == "const") return this->parseDecl(f);
        if(id == "if") return this->parseIf(f, false);
        if(id == "while") return this->parseWhile(f);
        if(id == "for") return this->parseFor(f);
        if(id == "foreach") return this->parseForeach(f);
        if(id == "break") return this->parseBreak();
        if(id == "continue") return this->parseContinue();
        if(id == "switch") return this->parseSwitch(f);
        if(id == "fdefer") return this->parseDefer(true, f);
        if(id == "defer") return this->parseDefer(false, f);

        if(this->tokens[this->idx+1]->type == TokType::Rarr && this->tokens[this->idx+4]->type != TokType::Equ
           && !TokType::isParent(this->tokens[this->idx+4]->type) && !TokType::isCompoundAssigment(this->tokens[this->idx+4]->type)
           && this->tokens[this->idx+4]->type != TokType::Rarr && this->tokens[this->idx+4]->type != TokType::Dot) {
            if(this->tokens[this->idx+2]->type == TokType::Number && this->tokens[this->idx+3]->type == TokType::Larr) return this->parseDecl(f);
        }
        
        this->next();

        if(this->peek()->type != TokType::Identifier) {
            if(this->peek()->type == TokType::Multiply) {this->idx -= 1; return this->parseDecl(f);}
            else if(this->peek()->type == TokType::Rarr || this->peek()->type == TokType::Rpar) {
                if(isBasicType(id)) {this->idx -= 1; return this->parseDecl(f);}
            } this->idx -= 1;
            if(this->peek()->type == TokType::Builtin) return this->parseBuiltin(f);
            Node* expr = this->parseExpr(f);
            if(this->peek()->type != TokType::Lbra) {
                if(this->peek()->type != TokType::Semicolon) this->error("expected token ';'!");
                this->next();
            }
            else this->next();
            return expr;
        } this->idx -= 1;

        return this->parseDecl(f);
    }

    if(this->peek()->type == TokType::Rpar) {
        std::vector<DeclarMod> mods;
        this->next();
        while(this->peek()->type != TokType::Lpar) {
            if(this->peek()->type == TokType::Builtin) {
                NodeBuiltin* nb = (NodeBuiltin*)this->parseBuiltin(f);
                mods.push_back(DeclarMod{.name = "@" + nb->name, .value = "", .genValue = nb});
                if(this->peek()->type == TokType::Comma) this->next();
                continue;
            }

            if(this->peek()->type != TokType::Identifier) this->error("expected identifier!");
            this->next();

            std::string name = this->peek()->value;
            std::string value = "";
            if(this->peek()->type == TokType::ValSel) {
                this->next();
                value = this->peek()->value;
                this->next();
            }

            mods.push_back(DeclarMod{.name = name, .value = value});

            if(this->peek()->type == TokType::Comma) this->next();
            else if(this->peek()->type == TokType::Lpar) break;
        } this->next();

        return parseDecl(f, mods);
    }

    if(this->peek()->type == TokType::Builtin) return this->parseBuiltin(f);

    Node* expr = this->parseExpr(f);
    if(this->peek()->type == TokType::Lbra) this->next();
    else {
        if(this->peek()->type != TokType::Semicolon) this->error("expected token ';'!");
        this->next();
    }

    return expr;
}

Node* Parser::parseBreak() {
    this->next();
    Node* nbreak = new NodeBreak(this->peek()->line);
    this->next();
    return nbreak;
}

Node* Parser::parseContinue() {
    this->next();
    Node* ncontinue = new NodeContinue(this->peek()->line);
    this->next();
    return ncontinue;
}

Node* Parser::parseAnyLevelBlock(std::string f) {
    if(f != "") {
        if(this->peek()->type != TokType::Rbra) return this->parseStmt(f);
        return this->parseBlock(f);
    }

    if(this->peek()->type != TokType::Rbra) return this->parseTopLevel(f);

    NodeBlock* block = new NodeBlock({});
    
    this->next();
    while(this->peek()->type != TokType::Lbra) block->nodes.push_back(this->parseTopLevel(f));
    this->next();

    return block;
}

Node* Parser::parseIf(std::string f, bool isStatic) {
    int line = this->peek()->line;
    this->next();

    bool isLikely = false;
    bool isUnlikely = false;

    if(this->peek()->value == "likely") {isLikely = true; this->next();}
    else if(this->peek()->value == "unlikely") {isUnlikely = true; this->next();}

    if(this->peek()->type != TokType::Rpar) this->error("expected token '('!");

    this->next();
    Node* cond = this->parseExpr();
    if(this->peek()->type != TokType::Lpar) this->error("expected token ')'!");
    this->next();

    NodeIf* _if = nullptr;

    Node* body = this->parseAnyLevelBlock(f);

    if(this->peek()->value == "else" || (isStatic && this->peek()->value == "@else")) {
        this->next();
    
        if(this->peek()->value == "likely") {isUnlikely = true; this->next();}
        else if(this->peek()->value == "unlikely") {isLikely = true; this->next();}
    
        _if = new NodeIf(cond, body, this->parseAnyLevelBlock(f), line, isStatic);
    }
    else _if = new NodeIf(cond, body, nullptr, line, isStatic);

    _if->isLikely = isLikely;
    _if->isUnlikely = isUnlikely;

    if(isStatic) return new NodeComptime(_if);

    return _if;
}

std::pair<Node*, Node*> Parser::parseCase(std::string f) {
    this->next();
    if(this->peek()->type != TokType::Rpar) this->error("expected token '('!");
    this->next();

    Node* cond = this->parseExpr(f);
    if(this->peek()->type != TokType::Lpar) this->error("expected token ')'!");
    this->next();

    Node* block = this->parseStmt(f);
    return std::pair<Node*, Node*>(cond, block);
}

Node* Parser::parseSwitch(std::string f) {
    int line = this->peek()->line;
    this->next(); this->next();
    Node* expr = this->parseExpr();
    this->next();

    std::vector<std::pair<Node*, Node*>> statements;
    Node* _default = nullptr;

    this->next();
    while(this->peek()->type != TokType::Lbra && this->peek()->type != TokType::Eof) {
        if(this->peek()->value == "case") statements.push_back(this->parseCase(f));
        else if(this->peek()->value == "default") {
            this->next();
            _default = this->parseStmt(f);
        }
        while(this->peek()->type == TokType::Semicolon) this->next();
    }
    if(this->peek()->type != TokType::Eof) this->next();

    return new NodeSwitch(expr, _default, statements, line);
}

NodeBlock* Parser::parseBlock(std::string s) {
    std::vector<Node*> nodes;
    if(this->peek()->type == TokType::Rbra) this->next();
    while(this->peek()->type != TokType::Lbra && this->peek()->type != TokType::Eof) {
        nodes.push_back(this->parseStmt(s));
        while(this->peek()->type == TokType::Semicolon) this->next();
    }
    if(this->peek()->type != TokType::Eof) this->next();
    return new NodeBlock(nodes);
}

Node* Parser::parseSlice(Node* base, std::string f) {
    int loc = peek()->line;
    this->next();
    Node* start = parseExpr(f);
    if(peek()->type != TokType::SliceOper) error("expected token \"..\"!");
    this->next();
    Node* end = parseExpr(f);
    if(peek()->type != TokType::Larr) error("expected token ']'!");
    this->next();
    return new NodeSlice(base, start, end, loc);
}

bool Parser::isDefinedLambda(bool updateIdx) {
    const auto& nextToken = this->tokens[this->idx + 1];
    if(nextToken->type != TokType::Rpar && nextToken->type != TokType::Multiply) return false;

    int oldIdx = this->idx;
    int cntOfRpars = 1;

    for(int i=this->idx + 2; i<this->tokens.size(); i++) {
        const auto& token = this->tokens[i];
        
        if(token->type == TokType::Lpar) {
            if(--cntOfRpars == 0) {
                bool result = (i + 1 < this->tokens.size() && (this->tokens[i + 1]->type == TokType::Rbra || this->tokens[i + 1]->type == TokType::ShortRet));
                if(!updateIdx) this->idx = i;
                return result;
            }
        }
        else if (token->type == TokType::Rpar) ++cntOfRpars;
    }

    return false;
}

Node* Parser::parseLambda() {
    int loc = peek()->line;
    Type* type = this->parseType();
    std::string name = "";

    if(peek()->type == TokType::Identifier) {
        name = peek()->value;
        next();
    }

    if(peek()->type == TokType::ShortRet) {
        this->next();
        Node* value = this->parseExpr();
        return new NodeLambda(loc, (TypeFunc*)type,new NodeBlock({new NodeRet(value, loc)}), name);
    }

    NodeBlock* b = this->parseBlock("lambda");
    if(peek()->type == TokType::ShortRet) {
        next();
        Node* value = this->parseExpr();
        b->nodes.push_back(new NodeRet(value, loc));
    }
    return new NodeLambda(loc, (TypeFunc*)type, b, name);
}

std::vector<Node*> Parser::parseFuncCallArgs() {
    std::vector<Node*> buffer;
    if(this->peek()->type == TokType::Rpar) this->next();
    else this->error("expected token '('!");
    while(this->peek()->type != TokType::Lpar) {
        if(this->peek()->type == TokType::Identifier) {
            if(this->isDefinedLambda()) buffer.push_back(this->parseLambda());
            else if(isBasicType(this->peek()->value)) buffer.push_back(new NodeType(this->parseType(), this->peek()->line));
            else buffer.push_back(this->parseExpr());
        }
        else buffer.push_back(this->parseExpr());
        if(this->peek()->type == TokType::Comma) this->next();
    } this->next();
    return buffer;
}