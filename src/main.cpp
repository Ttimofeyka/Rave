/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <iostream>
#include <fstream>
#include <regex>
#include "./include/lexer/lexer.hpp"
#include "./include/parser/parser.hpp"
#include "./include/compiler.hpp"

std::string outFile = "";
std::string outType = "";
std::vector<std::string> files;
genSettings options;
std::string exePath;
bool helpCalled = false;

genSettings analyzeArguments(std::vector<std::string>& arguments) {
    genSettings settings;
    for(int i=0; i<arguments.size(); i++) {
        if(arguments[i] == "-o" || arguments[i] == "--out") {outFile = arguments[i+1]; i += 1;}
        else if(arguments[i] == "-np" || arguments[i] == "--noPrelude") settings.noPrelude = true;
        else if(arguments[i] == "-eml" || arguments[i] == "--emitLLVM" || arguments[i] == "-emit-llvm") settings.emitLLVM = true;
        else if(arguments[i] == "-l" || arguments[i] == "--link") {settings.linkParams += "-l"+arguments[i+1]+" "; i += 1;}
        else if(arguments[i] == "-rcs" || arguments[i] == "--recompileStd") settings.recompileStd = true;
        else if(arguments[i] == "-c") {settings.onlyObject = true; settings.isStatic = true;}
        else if(arguments[i] == "-ne" || arguments[i] == "--noEntry") settings.noEntry = true;
        else if(arguments[i] == "-ns" || arguments[i] == "--noStd") settings.noStd = true;
        else if(arguments[i] == "-nc" || arguments[i] == "--noChecks") settings.noChecks = true;
        else if(arguments[i] == "-O0") settings.optLevel = 0;
        else if(arguments[i] == "-O1") settings.optLevel = 1;
        else if(arguments[i] == "-O2") settings.optLevel = 2;
        else if(arguments[i] == "-O3") settings.optLevel = 3;
        else if(arguments[i] == "-Ofast") {settings.optLevel = 3; settings.noChecks = true;}
        else if(arguments[i].find("-O") == 0) settings.optLevel = 1;
        else if(arguments[i] == "-s" || arguments[i] == "--shared") {settings.linkParams += "-shared "; settings.isPIE = true; settings.isPIC = false;}
        else if(arguments[i] == "-sof" || arguments[i] == "--saveObjectFiles") settings.saveObjectFiles = true;
        else if(arguments[i] == "-dw" || arguments[i] == "--disableWarnings") settings.disableWarnings = true;
        else if(arguments[i] == "--debug") Compiler::debugMode = true;
        else if(arguments[i] == "-t" || arguments[i] == "--target") {outType = arguments[i+1]; i += 1;}
        else if(arguments[i] == "-h" || arguments[i] == "--help") helpCalled = true;
        else if(arguments[i][0] == '-') settings.linkParams += arguments[i]+" ";
        else files.push_back(arguments[i]);
    }
    return settings;
}

int main(int argc, char** argv) {
    std::vector<std::string> arguments;
    exePath = getExePath();
    Compiler::debugMode = false;

    for(int i=1; i<argc; i+=1) arguments.push_back(std::string(argv[i]));
    options = analyzeArguments(arguments);

    if(helpCalled) {
        std::string help = std::string("Usage: rave [files] [options]")
        + "\nOptions:"
        + "\n\t--out (-o) <file> - Place the output into <file>."
        + "\n\t--target (-t) <target> - Change the platform type to <target>."
        + "\n\t--emitLLVM, -emit-llvm (-eml) - Create output files with LLVM IR."
        + "\n\t--recompileStd (-rcs) - Recompile the std with flags and replacing the cache version (if available)."
        + "\n\t-O0, -O1, -O2, -O3, -Ofast - Optimization levels (in ascending order of output program speed)."
        + "\n\t--link (-l) <library> - Add the <library> library to the linker."
        + "\n\t--noStd (-ns) - Do not link with standart library."
        + "\n\t--noEntry (-ne) - Passes information to the compiler that there is no start point (main) in the code."
        + "\n\t--noPrelude (-np) - Disable automatic import of <std/prelude> and <std/memory>."
        + "\n\t--noChecks (-nc) - Disables runtime checks (automatically at -Ofast)."
        + "\n\t--saveObjectfiles (-sof) - Save the object files after compilation."
        + "\n\t--disableWarnings (-dw) - Disables warnings."
        + "\n\t--shared (-s) - Creates a shared output files."
        + "\n\t-c - Do not create an output file (only object files)."
        + "\nFor bug reporting, you can use Issues at https://github.com/Ttimofeyka/Rave.";
        std::cout << help << std::endl;
        return 0;
    }

    if(files.size() == 0) {
        if(options.recompileStd) {
            #if defined(__linux__)
            Compiler::initialize(outFile, outType, options, {""});
            auto stdFiles = filesInDirectory("std");
            for(int i=0; i<stdFiles.size(); i++) {
                if(stdFiles[i].find(".ll") == std::string::npos && stdFiles[i].find(".rave") != std::string::npos) {
                    std::string compiledFile = std::regex_replace("std/"+stdFiles[i], std::regex("\\.rave"), ".o");
                    Compiler::compile("std/"+stdFiles[i]);
                    if(options.emitLLVM) {
                        char* err;
                        LLVMPrintModuleToFile(generator->lModule, ("std/"+stdFiles[i]+".ll").c_str(), &err);
                    }
                    std::ifstream src(compiledFile, std::ios::binary);
                    std::ofstream dst(std::regex_replace(compiledFile, std::regex("\\.o"), std::string(".") + Compiler::outType + ".o"));
                    dst << src.rdbuf();
                }
            }
            std::cout << "Time spent by lexer: " << std::to_string(Compiler::lexTime) << "ms\nTime spent by parser: " << std::to_string(Compiler::parseTime) << "ms\nTime spent by generator: " << std::to_string(Compiler::genTime) << "ms" << std::endl;
            #else
            // Platform without standart library caching support
            std::cout << "\033[0;33mWarning: your host platform does not support caching of the standard library.\033[0;0m" << std::endl;
            #endif
            return 0;
        }
        std::cout << "\033[0;31mError: no files to compile!\033[0;0m" << std::endl;
        std::exit(1);
    }

    Compiler::initialize(outFile, outType, options, files);
    Compiler::compileAll();
    return 0;
}