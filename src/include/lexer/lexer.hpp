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
#include <inttypes.h>

class Lexer {
public:
    std::string text = "";
    std::vector<Token*> tokens;
    int32_t idx = 0;
    int32_t line = 0;

    inline char peek() {return text[idx];}
    inline char next() {return text[++idx];}

    std::string replaceAllEscapes(std::string buffer, bool isChar = true);
    std::string getIdentifier();
    std::string getString();
    std::string getChar();
    std::string getDigit(char numType);

    Lexer(std::string text, int offset);
    ~Lexer() {for(size_t i=0; i<tokens.size(); i++) delete tokens[i];}
};