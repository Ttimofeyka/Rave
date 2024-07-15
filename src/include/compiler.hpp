/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include "./utils.hpp"
#include "./json.hpp"

namespace Compiler {
    extern std::string linkString;
    extern std::string outFile;
    extern std::string outType;
    extern genSettings settings;
    extern nlohmann::json options;
    extern double lexTime;
    extern double parseTime;
    extern double genTime;
    extern std::vector<std::string> files;
    extern std::vector<std::string> toImport;
    extern bool debugMode;
    extern std::string features;

    extern void error(std::string message);
    extern void initialize(std::string outFile, std::string outType, genSettings settings, std::vector<std::string> files);
    extern void clearAll();
    extern void compile(std::string file);
    extern void compileAll();
}