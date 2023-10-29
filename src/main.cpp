/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <iostream>
#include "./include/lexer/lexer.hpp"
#include "./include/parser/parser.hpp"
#include "./include/compiler.hpp"

std::string outFile = "";
std::string outType = "";
std::vector<std::string> files;
genSettings options;
std::string exePath;

genSettings analyzeArguments(std::vector<std::string>& arguments) {
    genSettings settings;
    for(int i=0; i<arguments.size(); i++) {
        if(arguments[i] == "-o" || arguments[i] == "--out") {outFile = arguments[i+1]; i += 1;}
        else if(arguments[i] == "-np" || arguments[i] == "--noPrelude") settings.noPrelude = true;
        else if(arguments[i] == "-eml" || arguments[i] == "--emitLLVM" || arguments[i] == "-emit-llvm") settings.emitLLVM = true;
        else if(arguments[i] == "-l" || arguments[i] == "--link") {settings.linkParams += "-l"+arguments[i+1]+" "; i += 1;}
        else if(arguments[i] == "-c") {settings.onlyObject = true; settings.isStatic = true;}
        else if(arguments[i] == "-ne" || arguments[i] == "--noEntry") settings.noEntry = true;
        else if(arguments[i] == "-ns" || arguments[i] == "--noStd") settings.noStd = true;
        else if(arguments[i] == "-opt" || arguments[i] == "-O" || arguments[i] == "--optimizationLevel") {settings.optLevel = std::stoi(arguments[i+1]); i += 1;}
        else if(arguments[i] == "-s" || arguments[i] == "--shared") settings.linkParams += "-shared ";
        else if(arguments[i] == "-sof" || arguments[i] == "--saveObjectFiles") settings.saveObjectFiles = true;
        else if(arguments[i] == "-dw" || arguments[i] == "--disableWarnings") settings.disableWarnings = true;
        else if(arguments[i] == "--debug") Compiler::debugMode = true;
        else if(arguments[i][0] == '-') settings.linkParams += arguments[i]+" ";
        else files.push_back(arguments[i]);
    }
    return settings;
}

int main(int argc, char** argv) {
    exePath = getExePath();
    Compiler::debugMode = false;
    std::vector<std::string> arguments;
    for(int i=1; i<argc; i+=1) arguments.push_back(std::string(argv[i]));
    options = analyzeArguments(arguments);

    Compiler::initialize(outFile, outType, options, files);
    Compiler::compileAll();
    return 0;
}