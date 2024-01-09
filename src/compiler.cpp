/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "./include/compiler.hpp"
#include "./include/parser/ast.hpp"
#include <iostream>
#include <fstream>
#include <chrono>
#include <ctime>
#include <regex>
#include "./include/lexer/lexer.hpp"
#include "./include/parser/parser.hpp"
#include "./include/llvm-c/Target.h"
#include "./include/llvm-c/TargetMachine.h"
#include "./include/llvm-c/Core.h"
#include "./include/llvm-c/Analysis.h"
#include "./include/llvm-c/IRReader.h"
#include "./include/llvm-c/Orc.h"
#include "./include/llvm-c/OrcEE.h"
#include "./include/llvm-c/Remarks.h"
#include "./include/llvm-c/Linker.h"
#include "./include/llvm-c/Transforms/PassBuilder.h"
#include "./include/llvm-c/Transforms/InstCombine.h"
#include "./include/llvm-c/Transforms/Utils.h"
#include "./include/llvm-c/Transforms/Vectorize.h"
#include "./include/llvm-c/Transforms/PassManagerBuilder.h"
#include "./include/llvm-c/Transforms/IPO.h"
#include "./include/llvm-c/Transforms/Scalar.h"
#include "./include/llvm-c/Target.h"

#ifdef _WIN32
   #include <io.h> 
   #define access    _access_s
#else
   #include <unistd.h>
#endif

std::string Compiler::linkString;
std::string Compiler::outFile;
std::string Compiler::outType;
genSettings Compiler::settings;
nlohmann::json Compiler::options;
double Compiler::lexTime = 0.0;
double Compiler::parseTime = 0.0;
double Compiler::genTime = 0.0;
std::vector<std::string> Compiler::files;
std::vector<std::string> Compiler::toImport;
bool Compiler::debugMode;

std::string getDirectory(std::string file) {
    return file.substr(0, file.find_last_of("/\\"));
}

struct ShellResult {
    std::string output;
    int status;
};

ShellResult exec(std::string cmd) {
    char buffer[256];
    std::string result = "";
    FILE* pipe = popen(cmd.c_str(), "r");
    if(!pipe) throw std::runtime_error("popen() failed!");
    while(fgets(buffer, 255, pipe) != NULL) result += buffer;
    return ShellResult{.output = result, .status = WEXITSTATUS(pclose(pipe))};
}

void Compiler::error(std::string message) {
    std::cout << "\033[0;31mError: "+message+"\033[0;0m\n";
    std::exit(1);
}

void Compiler::initialize(std::string outFile, std::string outType, genSettings settings, std::vector<std::string> files) {
    Compiler::outFile = outFile;
    Compiler::outType = outType;
    Compiler::settings = settings;
    Compiler::files = files;
    
    if(access((exePath+"options.json").c_str(), 0) == 0) {
        std::ifstream fOptions(exePath+"options.json");
        Compiler::options = nlohmann::json::parse(fOptions);
        if(fOptions.is_open()) fOptions.close();
    }
    else {
        std::ofstream fOptions(exePath+"options.json");
        #if defined(_WIN32) || defined(WIN32)
            fOptions << "{\n\t\"compiler\": \"clang\"\n}" << std::endl;
        #else
            ShellResult result = exec("which clang");
            if(result.status != 0) fOptions << "{\n\t\"compiler\": \"gcc\"\n}" << std::endl;
            else fOptions << "{\n\t\"compiler\": \"clang\"\n}" << std::endl;
        #endif
        if(fOptions.is_open()) fOptions.close();
    }
    Compiler::linkString = Compiler::options["compiler"].template get<std::string>()+" ";

    if(Compiler::settings.isPIE) linkString += "-fPIE ";
    if(Compiler::settings.noStd) linkString += "-nostdlib ";
    if(Compiler::settings.noEntry) linkString += "--no-entry ";
}

void Compiler::clearAll() {
    AST::varTable.clear();
    AST::funcTable.clear();
    AST::aliasTable.clear();
    AST::structTable.clear();
    AST::structsNumbers.clear();
    AST::methodTable.clear();
    AST::importedFiles.clear();
    AST::condStack.clear();
    AST::aliasTypes.clear();
    generator->structures.clear();
    generator->globals.clear();
    generator->functions.clear();
    generator->currentBuiltinArg = 0;
    generator->currBB = nullptr;
    currScope = nullptr;
}

void Compiler::compile(std::string file) {
    std::ifstream fContent(file);
    std::string content = "";

    char c;
    while(fContent.get(c)) content += c;

    int offset = 0;
    std::string oldContent = content;

    std::string ravePlatform = "UNKNOWN";
    std::string raveOs = "UNKNOWN";

    if(outType != "") {
        if(outType.find("i686") != std::string::npos || outType.find("i386") != std::string::npos) ravePlatform = "X86";
        else if(outType.find("x86_64") != std::string::npos || outType.find("win64") != std::string::npos) ravePlatform = "X86_64";
        else if(outType.find("x32") != std::string::npos) ravePlatform = "X32";
        else if(outType.find("aarch64") != std::string::npos || outType.find("arm64") != std::string::npos) ravePlatform = "AARCH64";
        else if(outType.find("arm") != std::string::npos) ravePlatform = "ARM";
        else if(outType.find("mips") != std::string::npos) ravePlatform = "MIPS";
        else if(outType.find("mips64") != std::string::npos) ravePlatform = "MIPS64";
        else if(outType.find("powerpc") != std::string::npos) ravePlatform = "POWERPC";
        else if(outType.find("powerpc64") != std::string::npos) ravePlatform = "POWERPC64";
        else if(outType.find("sparc") != std::string::npos) ravePlatform = "SPARC";
        else if(outType.find("sparcv9") != std::string::npos) ravePlatform = "SPARCV9";
        else if(outType.find("s390x") != std::string::npos) ravePlatform = "S390X";
        else if(outType.find("wasm") != std::string::npos) ravePlatform = "WASM";
        else if(outType.find("avr") != std::string::npos) {
            ravePlatform = "AVR";
            raveOs = "ARDUINO";
        }

        if(outType.find("win32") != std::string::npos) raveOs = "WINDOWS32";
        else if(outType.find("win64") != std::string::npos) raveOs = "WINDOWS64";
        else if(outType.find("linux") != std::string::npos) raveOs = "LINUX";
        else if(outType.find("darwin") != std::string::npos || outType.find("macos") != std::string::npos) raveOs = "DARWIN";
        else if(outType.find("android") != std::string::npos) raveOs = "ANDROID";
        else if(outType.find("ios") != std::string::npos) raveOs = "IOS";
    }
    else {
        ravePlatform = RAVE_PLATFORM;
        raveOs = RAVE_OS;
    }
    content = "alias __RAVE_PLATFORM = \""+ravePlatform+"\"; ";
    content = "alias __RAVE_OS = \""+raveOs+"\"; "+content;
    if(!Compiler::settings.noPrelude && file != "std/prelude.rave" && file != "std/memory.rave") {
        content = content+" import <std/prelude> <std/memory>";
    }
    content = content+"\n"+oldContent;

    AST::mainFile = Compiler::files[0];

    auto start = std::chrono::system_clock::now();
    Lexer* lexer = new Lexer(content, offset);
    auto end = std::chrono::system_clock::now();
    Compiler::lexTime += std::chrono::duration_cast<std::chrono::milliseconds>(end-start).count();

    //for(int i=0; i<lexer->tokens.size(); i++) std::cout << "Token '" << lexer->tokens[i]->value << "', type: " << std::to_string(lexer->tokens[i]->type) << std::endl;
    
    start = end;
    Parser* parser = new Parser(lexer->tokens, file);
    parser->parseAll();
    end = std::chrono::system_clock::now();
    for(int i=0; i<parser->nodes.size(); i++) parser->nodes[i]->check();
    Compiler::parseTime += std::chrono::duration_cast<std::chrono::milliseconds>(end-start).count();

    start = end;
    generator = new LLVMGen(file, Compiler::settings);

    if(Compiler::outType.empty()) {
        if(RAVE_OS == "LINUX") {
            if(RAVE_PLATFORM == "X86_64") Compiler::outType = "linux-gnu-pc-x86_64";
            else if(RAVE_PLATFORM == "X86") Compiler::outType = "linux-gnu-pc-i686";
            else if(RAVE_PLATFORM == "AARCH64") Compiler::outType = "linux-gnu-aarch64";
            else if(RAVE_PLATFORM == "ARM") Compiler::outType = "linux-gnu-armv7";
            else Compiler::outType = "linux-unknown";
        }
        else if(RAVE_OS == "WINDOWS") {
            if(RAVE_PLATFORM == "X86_64") Compiler::outType = "x86_64-win32-gnu";
            else Compiler::outType = "i686-win32-gnu";
        }
        else Compiler::outType = "unknown";
    }

    LLVMInitializeX86TargetInfo();
    LLVMInitializeAArch64TargetInfo();
    LLVMInitializePowerPCTargetInfo();
    LLVMInitializeMipsTargetInfo();
    LLVMInitializeARMTargetInfo();
    LLVMInitializeAVRTargetInfo();

    LLVMInitializeX86Target();
    LLVMInitializeAArch64Target();
    LLVMInitializePowerPCTarget();
    LLVMInitializeMipsTarget();
    LLVMInitializeARMTarget();
    LLVMInitializeAVRTarget();

    LLVMInitializeX86AsmParser();
    LLVMInitializeAArch64AsmParser();
    LLVMInitializePowerPCAsmParser();
    LLVMInitializeMipsAsmParser();
    LLVMInitializeARMAsmParser();
    LLVMInitializeAVRAsmParser();

    LLVMInitializeX86AsmPrinter();
    LLVMInitializeAArch64AsmPrinter();
    LLVMInitializePowerPCAsmPrinter();
    LLVMInitializeMipsAsmPrinter();
    LLVMInitializeARMAsmPrinter();
    LLVMInitializeAVRAsmPrinter();

    LLVMInitializeX86TargetMC();
    LLVMInitializeAArch64TargetMC();
    LLVMInitializePowerPCTargetMC();
    LLVMInitializeMipsTargetMC();
    LLVMInitializeARMTargetMC();
    LLVMInitializeAVRTargetMC();

    char* errors = nullptr;
    LLVMTargetRef target;
    char* triple = LLVMNormalizeTargetTriple(Compiler::outType.c_str());
    if(errors != nullptr) {
        Compiler::error("Normalize target triple: "+std::string(errors));
        std::exit(1);
    }
    else LLVMDisposeErrorMessage(errors);
    LLVMSetTarget(generator->lModule, triple);

    LLVMGetTargetFromTriple(triple, &target, &errors);
    if(errors != nullptr) {
        Compiler::error("Target from triple: "+std::string(errors));
        std::exit(1);
    }
    else LLVMDisposeErrorMessage(errors);

    for(int i=0; i<parser->nodes.size(); i++) parser->nodes[i]->generate();

    if(Compiler::settings.optLevel > 0) {
        LLVMPassManagerRef pm = LLVMCreatePassManager();
        LLVMAddAlwaysInlinerPass(pm);
        LLVMAddInstructionCombiningPass(pm);

        LLVMPassManagerBuilderRef pmb = LLVMPassManagerBuilderCreate();
        LLVMPassManagerBuilderSetOptLevel(pmb, Compiler::settings.optLevel);
        LLVMPassManagerBuilderPopulateModulePassManager(pmb, pm);
        LLVMRunPassManager(pm, generator->lModule);
    }

    LLVMTargetMachineRef machine = LLVMCreateTargetMachine(
		target,
		triple,
		"generic",
		"",
		LLVMCodeGenLevelDefault,
        (Compiler::settings.isPIC ? LLVMRelocPIC : LLVMRelocDynamicNoPic),
	LLVMCodeModelDefault);

    generator->targetData = LLVMCreateTargetDataLayout(machine);
    char* targetDataStr = LLVMCopyStringRepOfTargetData(generator->targetData);
    LLVMSetDataLayout(generator->lModule, targetDataStr);
    LLVMDisposeMessage(targetDataStr);

    LLVMTargetMachineEmitToFile(machine, generator->lModule, (std::regex_replace(file, std::regex(".rave"), ".o")).c_str(), LLVMObjectFile, &errors);
    if(errors != nullptr) {
        Compiler::error("Target machine emit to file: "+std::string(errors));
        std::exit(1);
    }
    end = std::chrono::system_clock::now();
    Compiler::genTime += std::chrono::duration_cast<std::chrono::milliseconds>(end-start).count();

    for(int i=0; i<AST::importedFiles.size(); i++) {
        if(std::find(Compiler::toImport.begin(), Compiler::toImport.end(), AST::importedFiles[i]) == Compiler::toImport.end()) Compiler::toImport.push_back(AST::importedFiles[i]);
    }

    Compiler::clearAll();
}

void Compiler::compileAll() {
    AST::debugMode = Compiler::debugMode;
    std::vector<std::string> toRemove;
    for(int i=0; i<Compiler::files.size(); i++) {
        if(access(Compiler::files[i].c_str(), 0) != 0) {
            Compiler::error("file '"+Compiler::files[i]+"' does not exists!");
            return;
        }
        if(Compiler::files[i].size() > 2
            && (Compiler::files[i].substr(Compiler::files[i].size()-2, Compiler::files[i].size()) == ".a"
             ||Compiler::files[i].substr(Compiler::files[i].size()-2, Compiler::files[i].size()) == ".o")) {
                Compiler::linkString += Compiler::files[i]+" ";
        }
        else {
            Compiler::compile(Compiler::files[i]);
            if(Compiler::settings.emitLLVM) {
                char* err;
                LLVMPrintModuleToFile(generator->lModule, (Compiler::files[i]+".ll").c_str(), &err);
            }
            std::string compiledFile = std::regex_replace(Compiler::files[i], std::regex(".rave"), ".o");
            Compiler::linkString += compiledFile+" ";
            if(!Compiler::settings.saveObjectFiles) toRemove.push_back(compiledFile);
        }
    }
    for(int i=0; i<AST::addToImport.size(); i++) {
        std::string fname = replaceAll(AST::addToImport[i], ">", "");
        if(std::count(Compiler::toImport.begin(), Compiler::toImport.end(), fname) == 0
        && std::count(Compiler::files.begin(), Compiler::files.end(), fname) == 0) {
            Compiler::toImport.push_back(fname);
        }
    }
    for(int i=0; i<Compiler::toImport.size(); i++) {
        if(access(Compiler::toImport[i].c_str(), 0) != 0) {
            Compiler::error("file '"+Compiler::files[i]+"' does not exists!");
            return;
        }
        if(Compiler::files[i].size() > 2 &&
            Compiler::toImport[i].substr(Compiler::toImport[i].size()-2, Compiler::toImport[i].size()) == ".a" ||
            Compiler::toImport[i].substr(Compiler::toImport[i].size()-2, Compiler::toImport[i].size()) == ".o") {
                Compiler::linkString += Compiler::toImport[i]+" ";
        }
        else {
            compile(Compiler::toImport[i]);
            if(Compiler::settings.emitLLVM) {
                char* err;
                LLVMPrintModuleToFile(generator->lModule, (Compiler::toImport[i]+".ll").c_str(), &err);
            }
            std::string compiledFile = std::regex_replace(Compiler::toImport[i], std::regex(".rave"), ".o");
            linkString += compiledFile+" ";
            toRemove.push_back(compiledFile);
        }
    }

    Compiler::outFile = (Compiler::outFile == "") ? "a" : getDirectory(Compiler::files[0])+"/"+Compiler::outFile;
    if(Compiler::settings.onlyObject) Compiler::linkString += "-r ";
    if(Compiler::settings.isStatic) Compiler::linkString += "-static ";
    if(Compiler::settings.isPIC) Compiler::linkString += "-no-pie ";

    Compiler::linkString += " "+Compiler::settings.linkParams+" ";
    if(outType != "") {
        /*if(Compiler::options["compiler"].template get<std::string>().find("gcc") == std::string::npos) {
            Compiler::linkString += "-target "+Compiler::outType+" ";
            //if("linker".into(options.object) Compiler::options) linkString = linkString ~ " -fuse-ld="~options.object["linker"].str~" ";
            Compiler::linkString += " -fuse-ld=lld";
        }*/
    }

    ShellResult result = exec(Compiler::linkString+" -o "+Compiler::outFile);
    if(result.status != 0) {
        Compiler::error("error when linking!\nLinking string: '"+Compiler::linkString+" -o "+Compiler::outFile+"'");
        std::exit(result.status);
        return;
    }
    for(int i=0; i<toRemove.size(); i++) std::remove(toRemove[i].c_str());
    std::cout << "Time spent by lexer: " << std::to_string(Compiler::lexTime) << "ms\nTime spent by parser: " << std::to_string(Compiler::parseTime) << "ms\nTime spent by generator: " << std::to_string(Compiler::genTime) << "ms\n";
}