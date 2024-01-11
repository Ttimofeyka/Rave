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
    std::cout << "\033[0;31mError in '"+this->file+"' file at "+std::to_string(this->peek()->line)+" line: "+msg+"\033[0;0m" << std::endl;
    std::exit(1);
}

void Parser::warning(std::string msg) {
    std::cout << "\033[0;33mWarning in '"+this->file+"' file at "+std::to_string(this->peek()->line)+" line: "+msg+"\033[0;0m" << std::endl;
}

Token* Parser::peek() {return this->tokens[this->idx];}
Token* Parser::next() {this->idx += 1; return this->tokens[this->idx];}
Token* Parser::expect(char type) {
    if(this->peek()->type != type) this->error("expected token "+tokenToString(type)+", not "+tokenToString(this->peek()->type));
    else this->next();
    return this->peek();
}

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
        Token* tok = this->peek();
        this->idx += 2;

        std::vector<Node*> args;
        while(this->peek()->type != TokType::Lpar) {
            args.push_back(this->parseExpr(s));
            if(this->peek()->type == TokType::Comma) this->next();
        } this->next();
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
                mods.push_back(DeclarMod{.name = ("@"+nb->name), .value = "", .genValue = nb});
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
    long loc = this->peek()->line; this->next();
    std::string name = this->peek()->value; this->next();
    this->expect(TokType::Rbra);
    std::vector<Node*> nNodes;
    Node* currNode = nullptr;
    while((currNode = this->parseTopLevel(s)) != nullptr) nNodes.push_back(currNode);
    this->expect(TokType::Lbra);
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
        args.push_back(FuncArgSet{.name = name, .type = type});
        if(this->peek()->type == TokType::Comma) this->next();
    }
    return args;
}

Node* Parser::parseBuiltin(std::string f) {
    std::string name = this->peek()->value;
    std::vector<Node*> args;
    NodeBlock* block;
    bool isTopLevel = false;

    this->idx += 2;
    while(this->peek()->type != TokType::Lpar) {
        args.push_back(this->parseExpr(f));
        if(this->peek()->type == TokType::Comma) this->next();
    } this->next();
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
    std::string name = s+"("+this->peek()->value+")";
    Token* _t = this->peek();
    this->next();

    if(this->peek()->type != TokType::Rpar) {
        Token* _t2 = this->peek();
        name = s+"("+_t->value+_t2->value+")";
        this->next();
        if(this->peek()->type != TokType::Rpar) {
            name = s+"("+_t->value+_t2->value+this->peek()->value+")";
            this->next();
        }
    }

    std::vector<FuncArgSet> args;

    if(this->peek()->type == TokType::Rpar) {
        this->next();
        while(this->peek()->type != TokType::Lpar) {
            Type* type = this->parseType();
            args.push_back(FuncArgSet{.name = this->peek()->value, .type = type});
            this->next();
            if(this->peek()->type == TokType::Comma) this->next();
        } this->next();
    }

    if(this->peek()->type == TokType::ShortRet) {
        NodeBlock* nb = new NodeBlock({});
        this->next();
        nb->nodes.push_back(new NodeRet(this->parseExpr(), name, _t->line));
        if(this->peek()->type == TokType::Semicolon) this->next();
        return new NodeFunc(name, args, nb, false, {}, _t->line, type, {});
    }
    this->next();
    NodeBlock* nb = this->parseBlock();
    if(this->peek()->type == TokType::ShortRet) {
        this->next();
        nb->nodes.push_back(new NodeRet(this->parseExpr(), name, _t->line));
    }
    return new NodeFunc(name, args, nb, false, {}, _t->line, type, {});
}

Node* Parser::parseDecl(std::string s, std::vector<DeclarMod> _mods) {
    std::vector<DeclarMod> mods = _mods;
    long loc = 0;
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
                mods.push_back(DeclarMod{.name = "@"+nb->name, .value = "", .genValue = nb});
                if(this->peek()->type == TokType::Comma) this->next();
                continue;
            }
            std::string name = this->peek()->value; this->next();
            std::string value = "";
            if(this->peek()->type == TokType::ValSel) {
                this->next(); value = this->peek()->value; this->next();
            }
            mods.push_back(DeclarMod{.name = name, .value = value});
            if(this->peek()->type == TokType::Comma) this->next();
            else if(this->peek()->type == TokType::Lpar) break;
        }
        this->next();
    }

    long currIdx = this->idx;
    auto type = this->parseType();
    if(instanceof<TypeCall>(type)) {
        this->idx = currIdx;
        Node* n = this->parseAtom(s);
        if(this->peek()->type == TokType::Semicolon) this->next();
        return n;
    }
    if(this->peek()->value == "operator") return this->parseOperatorOverload(type, s);
    if(this->peek()->value == "~" && this->tokens[this->idx+1]->value == "with") {this->next(); name = "~with";}
    else name = this->peek()->value;
    loc = this->peek()->line;

    this->next();

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
            if(this->peek()->type != TokType::Lpar) expect(TokType::Semicolon);
            //_templateNamesF = [];
            return new NodeFunc(name, args, new NodeBlock({new NodeRet(expr, name, loc)}), isExtern, mods, loc, type, templates);
        }
        else if(this->peek()->type == TokType::Semicolon) {
            this->next();
            //_templateNamesF = [];
            return new NodeFunc(name, args, new NodeBlock({}), isExtern, mods, loc, type, templates);
        }
        NodeBlock* block = this->parseBlock(name);
        if(this->peek()->type == TokType::ShortRet) {
            this->next();
            Node* n = this->parseExpr();
            if(this->peek()->type == TokType::Semicolon) next();

            if(instanceof<TypeVoid>(type)) block->nodes.push_back(n);
            else block->nodes.push_back(new NodeRet(n, name, loc));
        }
        //_templateNamesF = [];
        return new NodeFunc(name, args, block, isExtern, mods, loc, type, templates);
    }
    else if(this->peek()->type == TokType::Rbra) {
        NodeBlock* block = this->parseBlock(name);
        if(this->peek()->type == TokType::ShortRet) {
            this->next();
            Node* n = this->parseExpr();
            if(this->peek()->type == TokType::Semicolon) this->next();
            if(instanceof<TypeVoid>(type)) block->nodes.push_back(n);
            else block->nodes.push_back(new NodeRet(n,name,this->peek()->line));
        }
        //_templateNamesF = [];
        return new NodeFunc(name,{},block,isExtern,mods,loc,type,templates);
    }
    else if(this->peek()->type == TokType::ShortRet) {
        this->next();
        Node* n = this->parseExpr();
        if(this->peek()->type != TokType::Lpar) expect(TokType::Semicolon);
        //_templateNamesF = [];

        if(!instanceof<TypeVoid>(type)) return new NodeFunc(name,{},new NodeBlock({new NodeRet(n,name,loc)}),isExtern,mods,loc,type,templates);
        return new NodeFunc(name,{},new NodeBlock({n}),isExtern,mods,loc,type,templates);
    }
    else if(this->peek()->type == TokType::Semicolon) {
        this->next();
        return new NodeVar(name, nullptr, isExtern, instanceof<TypeConst>(type), (s==""), mods, loc, type, isVolatile);
    }
    else if(this->peek()->type == TokType::Equ) {
        this->next();
        Node* n = this->parseExpr();
        if(this->peek()->type != TokType::Lpar) this->expect(TokType::Semicolon);
        return new NodeVar(name, n, isExtern, instanceof<TypeConst>(type), (s==""), mods, loc, type, isVolatile);
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

Node* Parser::parseAtom(std::string f) {
    Token* t = this->peek();
    this->next();
    int size = this->peek()->value.size();
    if(t->type == TokType::Number) {
        if(size > 0 && std::tolower(this->peek()->value[0]) == 'u') {
            this->next();
            NodeInt* _int = new NodeInt(BigInt(t->value));
            _int->isUnsigned = true;
            return _int;
        }
        else if(size > 0 && std::tolower(this->peek()->value[0]) == 'l') {
            this->next();
            NodeInt* _int = new NodeInt(BigInt(t->value));
            _int->isMustBeLong = true;
            return _int;
        }
        return new NodeInt(BigInt(t->value));
    }
    if(t->type == TokType::FloatNumber) {
        if(size > 0 && std::tolower(this->peek()->value[0]) == 'd') {
            this->next();
            return new NodeFloat(std::stod(t->value), true);
        }
        return new NodeFloat(std::stof(t->value));
    }
    if(t->type == TokType::HexNumber) {
        long number;
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
    if(t->type == TokType::Identifier) {
        if(t->value == "null") return new NodeNull(nullptr, t->line);
        else if(t->value == "cast") {
            this->next();
            Type* ty = this->parseType();
            this->next(); // skip )
            Node* expr = this->parseExpr();
            return new NodeCast(ty, expr, t->line);
        }
        else if(t->value == "sizeof") {
            next();
            NodeType* val = new NodeType(this->parseType(), t->line);
            this->next();
            return new NodeSizeof(val, t->line);
        }
        else if(t->value == "itop") {
            this->next();
            Type* type = this->parseType();
            this->next();
            Node* val = this->parseExpr();
            this->next();
            return new NodeItop(val, type, t->line);
        }
        else if(t->value == "ptoi") {
            this->next();
            Node* val = this->parseExpr();
            this->next();
            return new NodePtoi(val, t->line);
        }
        else if(t->value == "cmpxchg") return parseCmpXchg(f);
        else if(t->value == "asm") {
            this->next();
            Type* type = nullptr;
            if(this->peek()->type != TokType::String) {
                type = this->parseType();
                if(this->peek()->type == TokType::Comma) this->next();
            }
            else type = new TypeVoid();
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
            if(this->peek()->type == TokType::Lpar) this->next();
            return new NodeAsm(line, true, type, additions, args, t->line);
        }
        else if(this->peek()->type == TokType::Less) {
            if(this->tokens[this->idx+1]->type == TokType::More || isBasicType(this->tokens[this->idx+1]->value) || this->tokens[this->idx+2]->type == TokType::Less || this->tokens[this->idx+2]->type == TokType::More || this->tokens[this->idx+2]->type == TokType::Comma || this->tokens[this->idx+2]->type == TokType::Multiply || this->tokens[this->idx+2]->type == TokType::Rarr) {
                this->next();
                std::string all = t->value+"<";
                int countOfL = 0;
                while(countOfL != -1) {
                    all += this->peek()->value;
                    if(this->peek()->type == TokType::Less) countOfL += 1;
                    else if(this->peek()->type == TokType::More) countOfL -= 1;
                    this->next();
                }
                if(this->peek()->type == TokType::More) this->next();
                return this->parseCall(new NodeIden(all, this->peek()->line));
            }
        }
        else if(this->peek()->type == TokType::Rpar) {
            this->idx -= 1;
            if(isDefinedLambda()) {
                return parseLambda();
            } else this->idx += 1;
        }
        return new NodeIden(t->value, this->peek()->line);
    }
    if(t->type == TokType::Rpar) {
        auto e = this->parseExpr();
        this->expect(TokType::Lpar);
        return e;
    }
    if(t->type == TokType::Rarr) {
        std::vector<Node*> values;
        while(this->peek()->type != TokType::Larr) {
            values.push_back(this->parseExpr());
            if(this->peek()->type == TokType::Comma) next();
        }
        this->next();
        return new NodeArray(t->line, values);
    }
    if(t->type == TokType::Builtin) {
        this->next();
        std::string name = t->value;
        std::vector<Node*> args;
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
    this->error("Expected a number, true/false, char, variable or expression. Got: '"+t->value+"' on "+std::to_string(t->line)+" line.");
    return nullptr;
}

Node* Parser::parseStruct(std::vector<DeclarMod> mods) {
    long loc = this->peek()->line;
    this->next();
    std::string name = this->peek()->value;
    this->next();

    std::vector<std::string> templateNames;
    if(this->peek()->type == TokType::Less) {
        this->next();
        while(this->peek()->type != TokType::More) {
            templateNames.push_back(this->peek()->value);
            this->next();
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
    expect(TokType::Rbra); // skip {
    std::vector<Node*> nodes;
    Node* currNode = nullptr;
    while((currNode = this->parseTopLevel(name)) != nullptr) nodes.push_back(currNode);
    expect(TokType::Lbra); // skip }

    return new NodeStruct(name, nodes, loc, _exs, templateNames, mods);
}

Node* Parser::parseUsing() {

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
    }
    return exePath+buffer;
}

Node* Parser::parseImport() {
    long loc = this->peek()->line;
    this->next();
    std::vector<std::string> files;

    while(this->peek()->type == TokType::Less || this->peek()->type == TokType::String) {
        if(this->peek()->type == TokType::Less) files.push_back(getGlobalFile(this)+".rave");
        else files.push_back(getDirectory2(AST::mainFile)+"/"+this->peek()->value+".rave");
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
    long loc = this->peek()->line;
    this->next();
    std::string name = this->peek()->value;
    this->next();
    this->expect(TokType::Equ);
    Type* childType = this->parseType();
    if(this->peek()->type == TokType::Semicolon) this->next();
    return new NodeAliasType(name, childType, loc);
}

Type* Parser::parseTypeAtom() {
    if(this->peek()->type == TokType::Identifier) {
        std::string id = this->peek()->value;
        this->next();
        if(id == "void") return new TypeVoid();
        else if(id == "auto") return new TypeAuto();
        else if(id == "const") {
            if(this->peek()->type == TokType::Rpar) this->next();
                Type* _t = this->parseType();
            if(this->peek()->type == TokType::Lpar) this->next();  
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
    else this->error("expected a typename, not '"+this->peek()->value+"'!");
    return nullptr;
}

std::vector<TypeFuncArg*> Parser::parseFuncArgs() {
    std::vector<TypeFuncArg*> buffer;
    if(this->peek()->type == TokType::Rpar) this->next();
    while(this->peek()->type != TokType::Lpar) {
        Type* ty = this->parseType();
        std::string name = "";
        if(this->peek()->type == TokType::Identifier) {name = this->peek()->value; this->next();}
        buffer.push_back(new TypeFuncArg(ty,name));
        if(this->peek()->type == TokType::Comma) this->next();
    } this->next();
    return buffer;
}

Type* Parser::parseType(bool cannotBeTemplate) {
    Type* ty = this->parseTypeAtom();
    while(
        this->peek()->type == TokType::Multiply || this->peek()->type == TokType::Rarr ||
        this->peek()->type == TokType::Rpar || (this->peek()->type == TokType::Less && !cannotBeTemplate)
    ) {
        if(this->peek()->type == TokType::Multiply) {this->next(); ty = new TypePointer(ty);}
        else if(this->peek()->type == TokType::Rarr) {
            int cnt = 0;
            this->next();
            if(this->peek()->type == TokType::Number) cnt = std::stoi(this->peek()->value);
            this->next();
            this->expect(TokType::Larr);
            ty = new TypeArray(cnt, ty);
        }
        else if(this->peek()->type == TokType::Rpar) ty = new TypeFunc(ty, this->parseFuncArgs());
        else {
            std::vector<Type*> tTypes;
            std::string tTypesString = "";
            this->next();
            while(this->peek()->type != TokType::More) {
                tTypes.push_back(this->parseType(cannotBeTemplate));
                if(this->peek()->type == TokType::Comma) this->next();
            } this->next();
            for(int i=0; i<tTypes.size(); i++) tTypesString += tTypes[i]->toString()+",";
            ty = new TypeStruct(ty->toString()+"<"+tTypesString.substr(0,tTypesString.size()-1)+">",tTypes);
            if(this->peek()->type == TokType::Rpar) ty = new TypeCall(((TypeStruct*)ty)->name,this->parseFuncCallArgs());
        }
    }
    return ty;
}

bool Parser::isSlice() {
    long idx = this->idx;
    this->next();
    int countOfRarr = 1;
    while(countOfRarr != 0) {
        if(this->peek()->type == TokType::Rarr) countOfRarr += 1;
        else if(this->peek()->type == TokType::Larr) countOfRarr -= 1;
        else if(this->peek()->type == TokType::SliceOper || this->peek()->type == TokType::SlicePtrOper) {
            this->idx = idx;
            return true;
        }
        next();
    }
    this->idx = idx;
    return false;
}

bool Parser::isTemplate() {
    long idx = this->idx;
    if(this->tokens[this->idx+1]->type == TokType::Number || this->tokens[this->idx+1]->type != TokType::HexNumber
    || this->tokens[this->idx+1]->type != TokType::FloatNumber || this->tokens[this->idx+1]->type != TokType::String
    || this->tokens[this->idx+1]->type != TokType::Rarr) return false;
    this->next();
    if(this->peek()->type == TokType::Number || this->peek()->type == TokType::String || this->peek()->type == TokType::HexNumber || this->peek()->type == TokType::FloatNumber) {
        this->idx = idx;
        return false;
    }
    this->next();
    if(this->peek()->type == TokType::Multiply || this->peek()->type == TokType::Comma || this->peek()->type == TokType::Rarr || this->peek()->type == TokType::More || this->peek()->type == TokType::Less) {
        if(this->peek()->type == TokType::Multiply) {
            this->next();
            if(this->peek()->type == TokType::Multiply || this->peek()->type == TokType::Rarr || this->peek()->type == TokType::Less || this->peek()->type == TokType::More || this->peek()->type == TokType::Comma) {
                this->idx = idx;
                return true;
            }
            this->idx = idx;
            return false;
        }
        else if(this->peek()->type == TokType::Comma || this->peek()->type == TokType::More || this->peek()->type == TokType::Less) {
            this->idx = idx;;
            return true;
        }
        else if(this->peek()->type == TokType::Rarr) {
            while(this->peek()->type == TokType::Rarr) {
                this->next();
                if(this->peek()->type != TokType::Number) { // TODO: Alias support
                    this->idx = idx;
                    return false;
                }
                this->next();
                this->next();
            }
            if(this->peek()->type == TokType::Multiply || this->peek()->type == TokType::Less || this->peek()->type == TokType::More || this->peek()->type == TokType::Comma) {
                this->idx = idx;
                return true;
            }
            this->idx = idx;
            return false;
        }
    }
    this->idx = idx;
    return false;
}

Node* Parser::parsePrefix(std::string f) {
    const std::vector<char> operators = {
        TokType::GetPtr,
        TokType::Multiply,
        TokType::Minus,
        TokType::Ne,
        TokType::Destructor,
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
            if(this->isSlice()) base = parseSlice(base,f);
            else base = new NodeIndex(base, this->parseIndexes(), this->peek()->line);
        }
        else if(this->peek()->type == TokType::Dot) {
            this->next();
            bool isPtr = (this->peek()->type == TokType::GetPtr);
            if(isPtr) this->next();
            std::string field = this->peek()->value; this->next();
            if(this->peek()->type == TokType::Rpar) base = this->parseCall(new NodeGet(base, field, isPtr, this->peek()->line));
            else if(this->peek()->type == TokType::Less) {
                if(this->isTemplate()) {
                    std::vector<Type*> types;
                    std::string sTypes = "<";
                    this->next();
                    while(this->peek()->type != TokType::More) {
                        types.push_back(this->parseType());
                        sTypes += types[types.size()-1]->toString()+",";
                        if(this->peek()->type == TokType::Comma) this->next();
                    }
                    sTypes = sTypes.substr(0,sTypes.size()-1)+">";
                    this->next();
                    if(this->peek()->type == TokType::Rpar) base = this->parseCall(new NodeGet(base, field+sTypes, isPtr, this->peek()->line));
                    else {
                        generator->error("Assert into parseSuffix!", this->peek()->line);
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

Node* Parser::parseCall(Node* func) {return new NodeCall(this->peek()->line, func, this->parseFuncCallArgs());}

Node* Parser::parseBasic(std::string f) {return this->parseSuffix(this->parsePrefix(f),f);}

Node* Parser::parseExpr(std::string f) {
    std::vector<Token*> operatorStack;
	std::vector<Node*> nodeStack;
	uint32_t nodeStackSize = 0;
	uint32_t operatorStackSize = 0;

	nodeStack.insert(nodeStack.begin(), this->parseBasic(f));
	nodeStackSize += 1;

    while(operators.find(this->peek()->type) != operators.end()) {
		if(operatorStack.empty()) {
			operatorStack.insert(operatorStack.begin(), this->peek());
			operatorStackSize += 1;
            this->next();
		}
		else {
			auto tok = this->peek();
            this->next();
			auto type = tok->type;
			int prec = operators[type];
			while(operatorStackSize > 0 && prec <= operators[operatorStack.front()->type]) {
				auto lhs = nodeStack.front(); nodeStack.erase(nodeStack.begin());
				auto rhs = nodeStack.front(); nodeStack.erase(nodeStack.begin());

				nodeStack.insert(nodeStack.begin(), new NodeBinary(operatorStack.front()->type, lhs, rhs, tok->line));
				nodeStackSize -= 1;

				operatorStack.erase(operatorStack.begin());
				operatorStackSize -= 1;
			}

			operatorStack.insert(operatorStack.begin(), tok);
			operatorStackSize += 1;
		}

		nodeStack.insert(nodeStack.begin(), this->parseBasic(f));
		nodeStackSize += 1;
	}

	while(!operatorStack.empty()) {
		auto rhs = nodeStack.front(); nodeStack.erase(nodeStack.begin());
		auto lhs = nodeStack.front(); nodeStack.erase(nodeStack.begin());

		nodeStack.insert(nodeStack.begin(), new NodeBinary(operatorStack.front()->type, lhs, rhs, operatorStack.front()->line));
		nodeStackSize -= 1;

		operatorStack.erase(operatorStack.begin());
		operatorStackSize -= 1;
	}

    return nodeStack.front();
}

Node* Parser::parseWhile(std::string f) {
    long line = this->peek()->line;
    this->next();

    Node* cond = nullptr;
    if(this->peek()->type == TokType::Rbra) cond = new NodeBool(true);
    else {
        this->next();
        cond = this->parseExpr(f);
        if(this->peek()->type == TokType::Lpar) this->next();
    }

    auto body_ = parseStmt(f);
    return new NodeWhile(cond, body_, line, f);
}

Node* Parser::parseFor(std::string f) {
    int line = this->peek()->line;
    this->idx += 2;

    std::vector<Node*> presets;
    std::vector<Node*> afters;
    NodeBinary* cond;

    int curr = 0;

    while(this->peek()->type != TokType::Lpar) {
        if(this->peek()->type == TokType::Semicolon) {
            curr += 1;
            this->next();
        }
        if(this->peek()->type == TokType::Comma) this->next();

        if(curr == 0) {
            if(this->tokens[this->idx+1]->value == "=") presets.push_back(this->parseExpr(f));
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
        else if(curr == 1) cond = (NodeBinary*)this->parseExpr(f);
        else afters.push_back(this->parseExpr(f));
    }
    this->next();

    Node* stmt = this->parseStmt(f);
    if(!instanceof<NodeBlock>(stmt)) stmt = new NodeBlock(std::vector<Node*>({stmt}));
    return new NodeFor(presets, cond, afters, (NodeBlock*)stmt, f, this->peek()->line);
}

Node* Parser::parseStmt(std::string f) {
    bool isStatic = false;
    if(this->peek()->value == "static") {isStatic = true; this->next();}

    if(this->peek()->type == TokType::Rbra) return this->parseBlock(f);
    if(this->peek()->type == TokType::Semicolon) {this->next(); return this->parseStmt(f);}
    if(this->peek()->type == TokType::Eof) return nullptr;
    if(this->peek()->type == TokType::Identifier && (this->tokens[this->idx+1]->type == TokType::Less)) return this->parseDecl(f);
    if(this->peek()->value == "try") {}
    if(this->peek()->type == TokType::Identifier) {
        std::string id = this->peek()->value;
        if(id == "if") return this->parseIf(f, isStatic);
        if(id == "while") return this->parseWhile(f);
        if(id == "for") return this->parseFor(f);
        if(id == "break") return this->parseBreak();
        if(id == "switch") return this->parseSwitch(f);
        if(id == "extern" || id == "volatile") return this->parseDecl(f);
        if(this->tokens[this->idx+1]->type == TokType::Rarr && this->tokens[this->idx+4]->type != TokType::Equ
           && this->tokens[this->idx+4]->type != TokType::Lpar && this->tokens[this->idx+4]->type != TokType::Rpar
           && this->tokens[this->idx+4]->type != TokType::PluEqu && this->tokens[this->idx+4]->type != TokType::MinEqu
           && this->tokens[this->idx+4]->type != TokType::MulEqu && this->tokens[this->idx+4]->type != TokType::DivEqu) {
            if(this->tokens[this->idx+2]->type == TokType::Number && this->tokens[this->idx+3]->type == TokType::Larr) return this->parseDecl(f);
        } this->next();
        if(this->peek()->type != TokType::Identifier) {
            if(this->peek()->type == TokType::Multiply) {this->idx -= 1; return this->parseDecl(f);}
            else if(this->peek()->type == TokType::Rarr || this->peek()->type == TokType::Rpar) {
                if(
                    id == "void" || id == "bool" || id == "int" || id == "uint" ||
                    id == "short" || id == "ushort" || id == "char" || id == "uchar" ||
                    id == "long" || id == "ulong" || id == "cent" || id == "ucent"|| id == "const"
                ) {this->idx -= 1; return this->parseDecl(f);}
            } this->idx -= 1;
            if(this->peek()->type == TokType::Builtin) return this->parseBuiltin(f);
            Node* expr = this->parseExpr(f);
            if(this->peek()->type != TokType::Lbra) this->expect(TokType::Semicolon);
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
                mods.push_back(DeclarMod{.name = "@"+nb->name, .value = "", .genValue = nb});
                if(this->peek()->type == TokType::Comma) this->next();
                continue;
            }
            std::string name = this->expect(TokType::Identifier)->value;
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
    else this->expect(TokType::Semicolon);
    return expr;
}

Node* Parser::parseBreak() {
    this->next();
    Node* nbreak = new NodeBreak(this->peek()->line);
    this->next();
    return nbreak;
}

Node* Parser::parseIf(std::string f, bool isStatic) {
    long line = this->peek()->line;
    this->next(); this->next();
    Node* cond = this->parseExpr();
    this->next();
    Node* body = this->parseStmt(f);
    if(this->peek()->value == "else") {
        this->next();
        return new NodeIf(cond, body,  this->parseStmt(f), line, f, isStatic);
    }
    return new NodeIf(cond, body, nullptr, line, f, isStatic);
}

std::pair<Node*, Node*> Parser::parseCase(std::string f) {
    this->next();
    Node* cond = this->parseExpr(f);
    this->next();
    Node* block = this->parseBlock(f);
    return std::pair<Node*, Node*>(cond, block);
}

Node* Parser::parseSwitch(std::string f) {
    long line = this->peek()->line;
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
            _default = this->parseBlock(f);
        }
        while(this->peek()->type == TokType::Semicolon) this->next();
    }
    if(this->peek()->type != TokType::Eof) this->next();

    return new NodeSwitch(expr, _default, statements, line, f);
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
    return nullptr;
}

bool Parser::isDefinedLambda(bool updateIdx) {
    if(this->tokens[this->idx+1]->type != TokType::Rpar) return false;
    long oldIdx = this->idx;
    this->next();

    int cntOfRpars = 1;
    while(this->peek()->type != TokType::Lpar) {
        this->next();
        if(this->peek()->type == TokType::Lpar) {
            cntOfRpars -= 1;
            if(cntOfRpars == 0) break;
            this->next();
        }
        else if(this->peek()->type == TokType::Rpar) cntOfRpars += 1;
    }
    this->next();
    bool result = (this->peek()->type == TokType::Rbra || this->peek()->type == TokType::ShortRet);
    if(updateIdx) this->idx = oldIdx;
    return result;
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
        return new NodeLambda(loc, (TypeFunc*)type,new NodeBlock({new NodeRet(value, "lambda", loc)}), name);
    }

    NodeBlock* b = this->parseBlock("lambda");
    if(peek()->type == TokType::ShortRet) {
        next();
        Node* value = this->parseExpr();
        b->nodes.push_back(new NodeRet(value, "lambda", loc));
    }
    return new NodeLambda(loc, (TypeFunc*)type, b, name);
}

std::vector<Node*> Parser::parseFuncCallArgs() {
    std::vector<Node*> buffer;
    if(this->peek()->type == TokType::Rpar) this->next();
    while(this->peek()->type != TokType::Lpar) {
        if(this->peek()->type == TokType::Identifier) {
            if(this->peek()->value == "bool" || this->peek()->value == "char" || this->peek()->value == "uchar"
             ||this->peek()->value == "short" || this->peek()->value == "ushort" || this->peek()->value == "int"
             ||this->peek()->value == "uint" || this->peek()->value == "long" || this->peek()->value == "ulong"
             ||this->peek()->value == "cent" || this->peek()->value == "ucent" || this->peek()->value == "void") {
                if(this->isDefinedLambda()) buffer.push_back(this->parseLambda());
                else buffer.push_back(new NodeType(this->parseType(), this->peek()->line));
            }
            else if(this->isDefinedLambda()) buffer.push_back(this->parseLambda());
            else buffer.push_back(this->parseExpr());
        }
        else buffer.push_back(this->parseExpr());
        if(this->peek()->type == TokType::Comma) this->next();
    } this->next();
    return buffer;
}