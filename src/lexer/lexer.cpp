/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "../include/lexer/tokens.hpp"
#include "../include/lexer/lexer.hpp"
#include "../include/utils.hpp"
#include <string>
#include <iostream>
#include <vector>
#include <stdint.h>
#include <regex>
#include <unordered_set>

std::string Lexer::replaceAllEscapes(std::string buffer, bool isChar) {
    std::string str = replaceAll(replaceAll(replaceAll(replaceAll(replaceAll(buffer, "\\r", "\r"), "\\n", "\n"), "\\'", "\'"), "\\\"", "\""), "\\t", "\t");
    return str;
}

std::string Lexer::getIdentifier() {
    static std::unordered_set<char> specialChars = {
        '+', '-', '*', '/', '>', '<', ',', '.', ';', '(', ')', '[', ']', '&',
        '\'', '"', '~', '=', '{', '}', '!', ' ', '\n', '\r', ':', '@'
    };

    std::string buffer = "";
    while(specialChars.find(peek()) == specialChars.end()) {
        buffer += peek();
        idx += 1;
        if(peek() == ':' && this->text[this->idx + 1] == ':') {
            buffer += "::";
            idx += 2;
        }
    }
    return buffer;
}

std::string Lexer::getString() {
    idx += 1;
    std::string buffer, buffer2 = "";
    while(peek() != '"') {
        if(peek() == '\\' && text[idx+1] == '"') {buffer += "\""; idx += 2;}
        else if(peek() == '\\' && isdigit(text[idx + 1])) {
            buffer2 += peek(); idx += 1;
            while(isdigit(peek())) {buffer2 += peek(); idx += 1;}
            buffer += replaceAllEscapes(buffer2);
            buffer2 = "";
        }
        else {buffer += peek(); idx += 1;}
    }
    if(peek() == '"') this->idx += 1;
    return replaceAllEscapes(buffer, false);
}

std::string Lexer::getChar() {
    idx += 1;
    std::string buffer = "", buffer2 = "";
    while(peek() != '\'') {
        if(peek() == '\\' && text[idx+1] == '\'') {buffer += "'"; idx += 2;}
        else if(peek() == '\\' && isdigit(text[idx+1])) {
            buffer2 += peek(); idx += 1;
            while(isdigit(peek())) {buffer2 += peek(); idx += 1;}
            buffer += replaceAllEscapes(buffer2);
            buffer2 = "";
        }
        else if(peek() == '\\' && text[idx+1] == '\\') {buffer += "\\"; idx += 2;}
        else {buffer += peek(); idx += 1;}
    } next();
    return replaceAllEscapes(buffer);
}

std::string Lexer::getDigit(char numType) {
    std::string buffer = "";
    if(numType == TokType::Number) while(isdigit(peek())) {buffer += peek(); idx += 1;}
    else {
        idx += 2;
        while(isdigit(peek()) || peek() == 'A' || peek() == 'B' || peek() == 'C' || peek() == 'D' || peek() == 'E' || peek() == 'F') {
            buffer += peek();
            idx += 1;
        }
    }
    return buffer;
}

Lexer::Lexer(std::string text, int offset) {
    this->text = text + " ";
    this->line = 0 - offset;
    while(idx < this->text.size()) {
        while(peek() == '\n' || peek() == '\r' || peek() == ' ' || peek() == '\t') {
            if(peek() == '\n') line += 1;
            idx += 1;
        }
        if(peek() == '\0' || peek() == 0 || peek() == EOF) break;
        switch(peek()) {
            case '+':
                if(next() == '=') {tokens.push_back(new Token(TokType::PluEqu, "+=", line)); idx += 1;}
                else if(peek() == '+') {tokens.push_back(new Token(TokType::PluEqu)); tokens.push_back(new Token(TokType::Number, "1")); idx += 1;}
                else tokens.push_back(new Token(TokType::Plus, "+", line));
                break;
            case '-':
                if(next() == '=') {tokens.push_back(new Token(TokType::MinEqu, "-=", line)); idx += 1;}
                else if(peek() == '-') {tokens.push_back(new Token(TokType::MinEqu)); tokens.push_back(new Token(TokType::Number, "1")); idx += 1;}
                else if(peek() == '>') {tokens.push_back(new Token(TokType::SlicePtrOper, "->", line)); idx += 1;}
                else tokens.push_back(new Token(TokType::Minus, "-", line));
                break;
            case '*':
                if(next() == '=') {tokens.push_back(new Token(TokType::MulEqu, "*=", line)); idx += 1;}
                else tokens.push_back(new Token(TokType::Multiply, "*", line));
                break;
            case '/':
                next();
                if(peek() == '=') {tokens.push_back(new Token(TokType::DivEqu, "/=", line)); idx += 1;}
                else if(peek() == '/') {while(peek() != '\n') idx += 1;}
                else if(peek() == '*') {
                    idx += 1;
                    while(peek() != '*' || this->text[this->idx+1] != '/') {
                        if(peek() == '\n') line += 1;
                        if(idx+1 >= text.size()) break;
                        this->next();
                    }
                    this->idx += 2;
                }
                else tokens.push_back(new Token(TokType::Divide, "/", line));
                break;
            case '&':
                if(next() == '&') {tokens.push_back(new Token(TokType::And, "&&", line)); idx += 1;}
                else tokens.push_back(new Token(TokType::GetPtr, "&", line));
                break;
            case '|':
                if(next() == '|') {tokens.push_back(new Token(TokType::Or, "||", line)); idx += 1;}
                break;
            case '(': tokens.push_back(new Token(TokType::Rpar,"(",line)); idx += 1; break;
            case ')': tokens.push_back(new Token(TokType::Lpar,")",line)); idx += 1; break;
            case '{': tokens.push_back(new Token(TokType::Rbra,"{",line)); idx += 1; break;
            case '}': tokens.push_back(new Token(TokType::Lbra,"}",line)); idx += 1; break;
            case '[': tokens.push_back(new Token(TokType::Rarr,"[",line)); idx += 1; break;
            case ']': tokens.push_back(new Token(TokType::Larr,"]",line)); idx += 1; break;
            case ',': tokens.push_back(new Token(TokType::Comma,",",line)); idx += 1; break;
            case ':': tokens.push_back(new Token(TokType::ValSel,":",line)); idx += 1; break;
            case ';': tokens.push_back(new Token(TokType::Semicolon,";",line)); idx += 1; break;
            case '%': tokens.push_back(new Token(TokType::Rem,"%",line)); idx += 1; break;
            case '@': idx += 1; tokens.push_back(new Token(TokType::Builtin,"@"+getIdentifier(),line)); break;
            case '"': tokens.push_back(new Token(TokType::String,getString(),line)); break;
            case '\'': tokens.push_back(new Token(TokType::Char,getChar(),line)); break;
            case '.':
                if(next() == '.') {tokens.push_back(new Token(TokType::SliceOper, "..", line)); idx += 1;}
                else tokens.push_back(new Token(TokType::Dot, ".", line));
                break;
            case '~':
                if((idx+5) < this->text.size()) {
                    int oldIdx = idx;
                    if(next() == 't' && next() == 'h' && next() == 'i' && next() == 's') {
                        if(next() != '.') {tokens.push_back(new Token(TokType::Identifier, "~this", line)); idx += 1;}
                        else {tokens.push_back(new Token(TokType::Destructor, "~", line)); idx = oldIdx + 1;}
                    }
                    else {tokens.push_back(new Token(TokType::Destructor, "~", line)); idx = oldIdx + 1;}
                }
                else {tokens.push_back(new Token(TokType::Destructor, "~", line)); idx += 1;}
                break;
            case '>':
                if(next() == '=') {tokens.push_back(new Token(TokType::MoreEqual, ">=", line)); idx += 1;}
                else if(peek() == '.') {tokens.push_back(new Token(TokType::BitRight, ">>", line)); idx += 1;}
                else tokens.push_back(new Token(TokType::More, ">", line));
                break;
            case '<':
                if(next() == '=') {tokens.push_back(new Token(TokType::LessEqual, "<=", line)); idx += 1;}
                else if(peek() == '.') {tokens.push_back(new Token(TokType::BitLeft, "<.", line)); idx += 1;}
                else tokens.push_back(new Token(TokType::Less, "<", line));
                break;
            case '=':
                if(next() == '=') {tokens.push_back(new Token(TokType::Equal, "==", line)); idx += 1;}
                else if(peek() == '>') {tokens.push_back(new Token(TokType::ShortRet, "=>", line)); idx += 1;}
                else tokens.push_back(new Token(TokType::Equ, "=", line));
                break;
            case '!':
                if(next() == '=') {tokens.push_back(new Token(TokType::Nequal, "!=", line)); idx += 1;}
                else if(peek() == '!') {tokens.push_back(new Token(TokType::BitXor, "!!", line)); idx += 1;}
                else tokens.push_back(new Token(TokType::Ne, "!", line));
                break;
            default:
                if(isdigit(peek())) {
                    idx += 1;
                    if(peek() != 'x') {
                        bool isFloat = false;
                        std::string buffer = "";
                        idx -= 1;
                        while(isdigit(this->peek()) || (this->peek() == '.' && this->text[this->idx+1] != '.')) {
                            if(this->peek() == '.') {
                                isFloat = true;
                                buffer += ".";
                                this->idx += 1;
                                continue;
                            }
                            buffer += peek();
                            this->idx += 1;
                        }
                        this->tokens.push_back(new Token(isFloat ? TokType::FloatNumber : TokType::Number, buffer, line));
                    }
                    else {idx -= 1; tokens.push_back(new Token(TokType::HexNumber, getDigit(TokType::HexNumber), line));}
                }
                else {
                    std::string identifier = getIdentifier();
                    if(identifier == "true") tokens.push_back(new Token(TokType::True, "true", line));
                    else if(identifier == "false") tokens.push_back(new Token(TokType::False, "false", line));
                    else tokens.push_back(new Token(TokType::Identifier, identifier, line));
                }
                break;
        }
    }
    tokens.push_back(new Token(TokType::Eof, "EOF", -1));
}

std::string tokenToString(char type) {
    switch(type) {
        case TokType::More: return "'>'";
        case TokType::MoreEqual: return "'>='";
        case TokType::Less: return "'<'";
        case TokType::LessEqual: return "'<='";
        case TokType::Equal: return "'=='";
        case TokType::Semicolon: return "';'";
        case TokType::Nequal: return "'!='";
        case TokType::Equ: return "'='";
        case TokType::Lpar: return "')'";
        case TokType::Rpar: return "'('";
        case TokType::String: return "string";
        case TokType::Char: return "char";
        case TokType::Identifier: return "identifier";
    }
    return "type " + std::to_string(type);
}