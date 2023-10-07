/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <string>
#include <iostream>
#include <vector>
#include "./tokens.hpp"

class Lexer {
public:
    std::string text = "";
    std::vector<Token*> tokens;
    int64_t idx = 0;
    int64_t line = 0;

    inline char peek() {return text[idx];}
    inline char next() {idx += 1; return text[idx];}
    std::string replaceAllEscapes(std::string buffer, bool isChar = true);
    std::string getIdentifier();
    std::string getString();
    std::string getChar();
    std::string getDigit(char numType);
    Lexer(std::string text, int offset);
    ~Lexer() {for(int i=0; i<this->tokens.size(); i++) delete this->tokens[i];}
};