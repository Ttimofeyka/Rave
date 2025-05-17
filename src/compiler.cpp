/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "./include/compiler.hpp"
#include "./include/parser/ast.hpp"
#include "./include/parser/nodes/NodeString.hpp"
#include "./include/parser/nodes/NodeBool.hpp"
#include "./include/parser/nodes/NodeInt.hpp"
#include "./include/parser/nodes/NodeImport.hpp"
#include <iostream>
#include <fstream>
#include <chrono>
#include <ctime>
#include <regex>
#include "./include/lexer/lexer.hpp"
#include "./include/parser/parser.hpp"
#include <llvm-c/Target.h>
#include <llvm-c/TargetMachine.h>
#include <llvm-c/Core.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/IRReader.h>
#include <llvm-c/Orc.h>

#include <llvm-c/OrcEE.h>
#include <llvm-c/Transforms/PassBuilder.h>

#include <llvm-c/Remarks.h>
#include <llvm-c/Linker.h>

#if LLVM_VERSION < 17
#include <llvm-c/Transforms/InstCombine.h>
#include <llvm-c/Transforms/Utils.h>
#include <llvm-c/Transforms/Vectorize.h>
#include <llvm-c/Transforms/PassManagerBuilder.h>
#include <llvm-c/Transforms/IPO.h>
#include <llvm-c/Transforms/Scalar.h>
#endif

#include <llvm-c/Target.h>

#ifdef _WIN32
   #include <io.h>
   #define access    _access_s
   #define WEXITSTATUS(w) (((w) >> 8) & 0377)
#else
   #include <unistd.h>
   #include <sys/wait.h>
#endif

std::string Compiler::linkString;
std::string Compiler::outFile;
std::string Compiler::outType;
std::string Compiler::features;
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

bool endsWith(const std::string &str, const std::string &suffix) {
    if(str.size() < suffix.size()) return false;
    return str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
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
    
    if(access((exePath + "options.json").c_str(), 0) == 0) {
        // If file exists - read it

        std::ifstream fOptions(exePath + "options.json");
        Compiler::options = nlohmann::json::parse(fOptions);
        if(fOptions.is_open()) fOptions.close();
    }
    else {
        // If file does not exist - create it with default settings

        std::ofstream fOptions(exePath + "options.json");

        #if defined(_WIN32)
            std::string compiler = "gcc";
        #else
            std::string compiler = "clang";
            ShellResult result = exec("which clang");
            if(result.status != 0) compiler = "gcc";
        #endif

        fOptions << "{\n\t\"compiler\": \"" << compiler << "\",\n\t"
        << "\"platforms\": {\n\t\t" <<
            "\"X86\": {\n\t\t\t" <<
                "\"POPCNT\": true,\n\t\t\t" <<
                "\"FMA\": true,\n\t\t\t" <<
                "\"F16C\": true,\n\t\t\t" <<
                "\"SSE\": true,\n\t\t\t" <<
                "\"SSE2\": true,\n\t\t\t" <<
                "\"SSE3\": true,\n\t\t\t" <<
                "\"SSSE3\": true,\n\t\t\t" <<
                "\"SSE4_1\": true,\n\t\t\t" <<
                "\"SSE4_2\": true,\n\t\t\t" <<
                "\"SSE4A\": false,\n\t\t\t" <<
                "\"AVX\": true,\n\t\t\t" <<
                "\"AVX2\": true\n\t\t" <<
                "},\n\t\t" <<
            "\"X86_64\": {\n\t\t\t" <<
                "\"POPCNT\": true,\n\t\t\t" <<
                "\"FMA\": true,\n\t\t\t" <<
                "\"F16C\": true,\n\t\t\t" <<
                "\"SSE\": true,\n\t\t\t" <<
                "\"SSE2\": true,\n\t\t\t" <<
                "\"SSE3\": true,\n\t\t\t" <<
                "\"SSSE3\": true,\n\t\t\t" <<
                "\"SSE4_1\": true,\n\t\t\t" <<
                "\"SSE4_2\": true,\n\t\t\t" <<
                "\"SSE4A\": false,\n\t\t\t" <<
                "\"AVX\": true,\n\t\t\t" <<
                "\"AVX2\": true,\n\t\t\t" <<
                "\"AVX512\": false\n\t\t"
                "},\n\t\t" <<
            "\"AARCH64\": {\n\t\t\t" <<
                "\"ASIMD\": true,\n\t\t\t" <<
                "\"FP\": true,\n\t\t\t" <<
                "\"SVE\": false,\n\t\t\t" <<
                "\"SVE2\": false\n\t\t" <<
                "},\n\t\t" <<
            "\"ARM\": {\n\t\t\t" <<
                "\"HALF\": true\n\t\t" <<
                "}\n\t" <<
            "}\n"
        "}" << std::endl;

        if(fOptions.is_open()) fOptions.close();

        std::ifstream rfOptions(exePath + "options.json");
        Compiler::options = nlohmann::json::parse(rfOptions);
        if(rfOptions.is_open()) rfOptions.close();
    }

    Compiler::linkString = Compiler::options["compiler"].template get<std::string>() + " ";

    if(Compiler::settings.isPIE) linkString += "-fPIE ";
    if(Compiler::settings.noStd) linkString += "-nostdlib ";
    if(Compiler::settings.noEntry) linkString += "--no-entry ";

    // Begin of LLVM initializing

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

    // End of LLVM initializing

    basicTypes[BasicType::Bool] = new TypeBasic(BasicType::Bool);
    basicTypes[BasicType::Char] = new TypeBasic(BasicType::Char);
    basicTypes[BasicType::Uchar] = new TypeBasic(BasicType::Uchar);
    basicTypes[BasicType::Short] = new TypeBasic(BasicType::Short);
    basicTypes[BasicType::Ushort] = new TypeBasic(BasicType::Ushort);
    basicTypes[BasicType::Int] = new TypeBasic(BasicType::Int);
    basicTypes[BasicType::Uint] = new TypeBasic(BasicType::Uint);
    basicTypes[BasicType::Long] = new TypeBasic(BasicType::Long);
    basicTypes[BasicType::Ulong] = new TypeBasic(BasicType::Ulong);
    basicTypes[BasicType::Cent] = new TypeBasic(BasicType::Cent);
    basicTypes[BasicType::Ucent] = new TypeBasic(BasicType::Ucent);
    basicTypes[BasicType::Half] = new TypeBasic(BasicType::Half);
    basicTypes[BasicType::Bhalf] = new TypeBasic(BasicType::Bhalf);
    basicTypes[BasicType::Float] = new TypeBasic(BasicType::Float);
    basicTypes[BasicType::Double] = new TypeBasic(BasicType::Double);
    basicTypes[BasicType::Real] = new TypeBasic(BasicType::Real);
    typeVoid = new TypeVoid();
}

// Clears all possible global variables.
void Compiler::clearAll() {
    AST::varTable.clear();
    AST::funcTable.clear();
    AST::aliasTable.clear();
    AST::structTable.clear();
    AST::structsNumbers.clear();
    AST::methodTable.clear();
    AST::importedFiles.clear();
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

    std::string ravePlatform = "UNKNOWN";
    std::string raveOs = "UNKNOWN";

    if(outType != "") {
        if(outType.find("i686") != std::string::npos || outType.find("i386") != std::string::npos) ravePlatform = "X86";
        else if(outType.find("aarch64") != std::string::npos || outType.find("arm64") != std::string::npos) ravePlatform = "AARCH64";
        else if(outType.find("x86_64") != std::string::npos || outType.find("win64") != std::string::npos) ravePlatform = "X86_64";
        else if(outType.find("x86") != std::string::npos) ravePlatform = "X86";
        else if(outType.find("arm") != std::string::npos) ravePlatform = "ARM";
        else if(outType.find("mips64") != std::string::npos) ravePlatform = "MIPS64";
        else if(outType.find("mips") != std::string::npos) ravePlatform = "MIPS";
        else if(outType.find("powerpc64") != std::string::npos) ravePlatform = "POWERPC64";
        else if(outType.find("powerpc") != std::string::npos) ravePlatform = "POWERPC";
        else if(outType.find("sparcv9") != std::string::npos) ravePlatform = "SPARCV9";
        else if(outType.find("sparc") != std::string::npos) ravePlatform = "SPARC";
        else if(outType.find("s390x") != std::string::npos) ravePlatform = "S390X";
        else if(outType.find("wasm") != std::string::npos) ravePlatform = "WASM";
        else if(outType.find("avr") != std::string::npos) {
            ravePlatform = "AVR";
            raveOs = "ARDUINO";
        }

        if(outType.find("win32") != std::string::npos) {
            #ifndef _WIN32
                Compiler::linkString += " --target=i686-pc-windows-gnu ";
            #endif
            raveOs = "WINDOWS";
        }
        else if(outType.find("win64") != std::string::npos || outType.find("windows") != std::string::npos) {
            #ifndef _WIN32
                Compiler::linkString += " --target=x86_64-pc-windows-gnu ";
            #endif
            raveOs = "WINDOWS";
        }
        else if(outType.find("linux") != std::string::npos) raveOs = "LINUX";
        else if(outType.find("freebsd") != std::string::npos) raveOs = "FREEBSD";
        else if(outType.find("darwin") != std::string::npos || outType.find("macos") != std::string::npos) raveOs = "DARWIN";
        else if(outType.find("android") != std::string::npos) raveOs = "ANDROID";
        else if(outType.find("ios") != std::string::npos) raveOs = "IOS";
    }
    else {
        ravePlatform = RAVE_PLATFORM;
        raveOs = RAVE_OS;
    }

    bool littleEndian = true;

    // Note: PowerPC must be rechecked for endianness

    if(ravePlatform == "MIPS64" || ravePlatform == "MIPS") littleEndian = false;

    bool isX86 = ravePlatform == "X86_64" || ravePlatform == "X86";
    bool isAARCH64 = ravePlatform == "AARCH64";
    bool isARM = ravePlatform == "ARM";

    bool popcnt = settings.popcnt && isX86;
    bool fma = settings.fma && isX86;
    bool f16c = settings.f16c && isX86;
    bool sse = settings.sse && isX86;
    bool sse2 = settings.sse2 && isX86;
    bool sse3 = settings.sse3 && isX86;
    bool ssse3 = settings.ssse3 && isX86;
    bool sse4a = settings.sse4a && isX86;
    bool sse4_1 = settings.sse4_1 && isX86;
    bool sse4_2 = settings.sse4_2 && isX86;
    bool avx = settings.avx && isX86;
    bool avx2 = settings.avx2 && isX86;
    bool avx512 = settings.avx512 && isX86;

    bool asimd = settings.asimd && isAARCH64;
    bool fp = settings.fp && isAARCH64;
    bool sve = settings.sve && isAARCH64;
    bool sve2 = settings.sve2 && isAARCH64;

    bool half = settings.half && isARM;

    if(isX86) {
        popcnt = popcnt && Compiler::options["platforms"][ravePlatform]["POPCNT"].template get<bool>();
        fma = fma && Compiler::options["platforms"][ravePlatform]["FMA"].template get<bool>();
        f16c = f16c && Compiler::options["platforms"][ravePlatform]["F16C"].template get<bool>();
        sse = sse && Compiler::options["platforms"][ravePlatform]["SSE"].template get<bool>();
        sse2 = sse2 && Compiler::options["platforms"][ravePlatform]["SSE2"].template get<bool>();
        sse3 = sse3 && Compiler::options["platforms"][ravePlatform]["SSE3"].template get<bool>();
        ssse3 = ssse3 && Compiler::options["platforms"][ravePlatform]["SSSE3"].template get<bool>();
        sse4a = sse4a && Compiler::options["platforms"][ravePlatform]["SSE4A"].template get<bool>();
        sse4_1 = sse4_1 && Compiler::options["platforms"][ravePlatform]["SSE4_1"].template get<bool>();
        sse4_2 = sse4_2 && Compiler::options["platforms"][ravePlatform]["SSE4_2"].template get<bool>();
        avx = avx && Compiler::options["platforms"][ravePlatform]["AVX"].template get<bool>();
        avx2 = avx2 && Compiler::options["platforms"][ravePlatform]["AVX2"].template get<bool>();

        if(ravePlatform == "X86_64") avx512 = avx512 && Compiler::options["platforms"][ravePlatform]["AVX512"].template get<bool>();
    }
    else if(isAARCH64) {
        asimd = asimd && Compiler::options["platforms"]["AARCH64"]["ASIMD"].template get<bool>();
        fp = fp && Compiler::options["platforms"]["AARCH64"]["FP"].template get<bool>();
        sve = sve && Compiler::options["platforms"]["AARCH64"]["SVE"].template get<bool>();
        sve2 = sve2 && Compiler::options["platforms"]["AARCH64"]["SVE2"].template get<bool>();
    }
    else if(isARM) {
        half = half && Compiler::options["platforms"]["ARM"]["HALF"].template get<bool>();
    }

    if(!settings.isNative) {
        Compiler::features = "";

        if(popcnt) Compiler::features += "+popcnt,";
        if(fma) Compiler::features += "+fma,";
        if(f16c) Compiler::features += "+f16c,";
        if(sse) Compiler::features += "+sse,";
        if(sse2) Compiler::features += "+sse2,";
        if(sse3) Compiler::features += "+sse3,";
        if(ssse3) Compiler::features += "+ssse3,";
        if(sse4a) Compiler::features += "+sse4a,";
        if(sse4_1) Compiler::features += "+sse4.1,";
        if(sse4_2) Compiler::features += "+sse4.2,";
        if(avx) Compiler::features += "+avx,";
        if(avx2) Compiler::features += "+avx2,";
        if(avx512) Compiler::features += "+avx512,";

        if(asimd) Compiler::features += "+neon,";
        if(fp) Compiler::features += "+fp-armv8,";
        if(sve) Compiler::features += "+sve,";
        if(sve2) Compiler::features += "+sve2,";

        if(half) Compiler::features += "+fp16,";
        
        if(ravePlatform == "X86_64") Compiler::features += "+64bit,";

        if(Compiler::features.length() > 0) Compiler::features = Compiler::features.substr(0, Compiler::features.length() - 1);
    }
    else Compiler::features = std::string(LLVMGetHostCPUFeatures());

    AST::aliasTable["__RAVE_PLATFORM"] = new NodeString(ravePlatform, false);
    AST::aliasTable["__RAVE_OS"] = new NodeString(raveOs, false);
    AST::aliasTable["__RAVE_OPTIMIZATION_LEVEL"] = new NodeInt(settings.optLevel);
    AST::aliasTable["__RAVE_RUNTIME_CHECKS"] = new NodeBool(!settings.noChecks);

    AST::aliasTable["__RAVE_POPCNT"] = new NodeBool(popcnt);
    AST::aliasTable["__RAVE_FMA"] = new NodeBool(fma);
    AST::aliasTable["__RAVE_F16C"] = new NodeBool(f16c);
    AST::aliasTable["__RAVE_SSE"] = new NodeBool(sse);
    AST::aliasTable["__RAVE_SSE2"] = new NodeBool(sse2);
    AST::aliasTable["__RAVE_SSE3"] = new NodeBool(sse3);
    AST::aliasTable["__RAVE_SSSE3"] = new NodeBool(ssse3);
    AST::aliasTable["__RAVE_SSE4A"] = new NodeBool(sse4a);
    AST::aliasTable["__RAVE_SSE4_1"] = new NodeBool(sse4_1);
    AST::aliasTable["__RAVE_SSE4_2"] = new NodeBool(sse4_2);
    AST::aliasTable["__RAVE_AVX"] = new NodeBool(avx);
    AST::aliasTable["__RAVE_AVX2"] = new NodeBool(avx2);
    AST::aliasTable["__RAVE_AVX512"] = new NodeBool(avx512);

    AST::aliasTable["__RAVE_ASIMD"] = new NodeBool(asimd);
    AST::aliasTable["__RAVE_FP_ARMV8"] = new NodeBool(fp);
    AST::aliasTable["__RAVE_SVE"] = new NodeBool(sve);
    AST::aliasTable["__RAVE_SVE2"] = new NodeBool(sve2);

    AST::aliasTable["__RAVE_HALF"] = new NodeBool(half);

    AST::aliasTable["__RAVE_LITTLE_ENDIAN"] = new NodeBool(littleEndian);
    AST::aliasTable["__RAVE_BIG_ENDIAN"] = new NodeBool(!littleEndian);

    if(ravePlatform == "X86_64" || ravePlatform == "AARCH64" || ravePlatform == "POWERPC64" || ravePlatform == "MIPS64") pointerSize = 64;
    else if(ravePlatform == "AVR") pointerSize = 16;
    else pointerSize = 32;

    AST::mainFile = Compiler::files[0];

    auto start = std::chrono::steady_clock::now();
    Lexer* lexer = new Lexer(content, -1);
    auto end = std::chrono::steady_clock::now();
    Compiler::lexTime += std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    start = end;
    Parser* parser = new Parser(lexer->tokens, file);

    if(!Compiler::settings.noPrelude && !endsWith(file, "std/prelude.rave") && !endsWith(file, "std/memory.rave")) {
        parser->nodes.push_back(new NodeImport(ImportFile{exePath + "std/prelude.rave", true}, {}, -1));
        parser->nodes.push_back(new NodeImport(ImportFile{exePath + "std/memory.rave", true}, {}, -1));
    }

    parser->parseAll();
    end = std::chrono::steady_clock::now();
    for(int i=0; i<parser->nodes.size(); i++) parser->nodes[i]->check();
    Compiler::parseTime += std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    start = end;
    generator = new LLVMGen(file, Compiler::settings, Compiler::options);

    if(settings.outDebugInfo) debugInfo = new DebugGen(settings, file, generator->lModule);
    else debugInfo = nullptr;

    if(raveOs == "LINUX") {
        if(ravePlatform == "X86_64") Compiler::outType = "linux-gnu-pc-x86_64";
        else if(ravePlatform == "X86") Compiler::outType = "linux-gnu-pc-i686";
        else if(ravePlatform == "AARCH64") Compiler::outType = "linux-gnu-aarch64";
        else if(ravePlatform == "ARM") Compiler::outType = "linux-gnu-armv7";
        else Compiler::outType = "linux-unknown";
    }
    else if(raveOs == "FREEBSD") {
        if(ravePlatform == "X86_64") Compiler::outType = "freebsd-gnu-pc-x86_64";
        else if(ravePlatform == "X86") Compiler::outType = "freebsd-gnu-pc-i686";
        else if(ravePlatform == "AARCH64") Compiler::outType = "freebsd-gnu-aarch64";
        else if(ravePlatform == "ARM") Compiler::outType = "freebsd-gnu-armv7";
        else Compiler::outType = "freebsd-unknown";
    }
    else if(raveOs == "WINDOWS") {
        if(ravePlatform == "X86_64") Compiler::outType = "x86_64-pc-windows-gnu";
        else Compiler::outType = "i686-win32-gnu";
    }
    else Compiler::outType = "unknown";

    char* errors = nullptr;
    LLVMTargetRef target;
    char* triple = LLVMNormalizeTargetTriple(Compiler::outType.c_str());
    if(errors != nullptr) {
        Compiler::error("normalize target triple: " + std::string(errors));
        std::exit(1);
    }
    else LLVMDisposeErrorMessage(errors);
    LLVMSetTarget(generator->lModule, triple);

    LLVMGetTargetFromTriple(triple, &target, &errors);
    if(errors != nullptr) {
        Compiler::error("target from triple \"" + std::string(triple) + "\": " + std::string(errors));
        std::exit(1);
    }
    else LLVMDisposeErrorMessage(errors);

    LLVMTargetMachineRef machine = LLVMCreateTargetMachine(
		target,
		triple,
		"generic",
		Compiler::features.c_str(),
		LLVMCodeGenLevelDefault,
        (Compiler::settings.isPIC ? LLVMRelocPIC : LLVMRelocDynamicNoPic),
	LLVMCodeModelDefault);

    generator->targetData = LLVMCreateTargetDataLayout(machine);
    LLVMSetDataLayout(generator->lModule, LLVMCopyStringRepOfTargetData(generator->targetData));

    for(int i=0; i<parser->nodes.size(); i++) parser->nodes[i]->generate();

    #if LLVM_VERSION < 17
    LLVMPassManagerRef pm = LLVMCreatePassManager();

    if(Compiler::settings.optLevel > 0) {
        LLVMAddInstructionCombiningPass(pm);
        LLVMAddStripDeadPrototypesPass(pm);

        if(Compiler::settings.optLevel >= 2) {
            LLVMAddCFGSimplificationPass(pm);
	        LLVMAddJumpThreadingPass(pm);
	        LLVMAddSimplifyLibCallsPass(pm);
	        LLVMAddTailCallEliminationPass(pm);
	        LLVMAddCFGSimplificationPass(pm);
	        LLVMAddReassociatePass(pm);
	        LLVMAddLoopRotatePass(pm);
	        LLVMAddLICMPass(pm);
	        LLVMAddCFGSimplificationPass(pm);
	        LLVMAddLoopIdiomPass(pm);
	        LLVMAddLoopDeletionPass(pm);
	        LLVMAddMergedLoadStoreMotionPass(pm);
	        LLVMAddMemCpyOptPass(pm);
	        LLVMAddSCCPPass(pm);
	        LLVMAddBitTrackingDCEPass(pm);
            LLVMAddCalledValuePropagationPass(pm);
            LLVMAddSLPVectorizePass(pm);
	        LLVMAddLICMPass(pm);
	        LLVMAddAlignmentFromAssumptionsPass(pm);
	        LLVMAddStripDeadPrototypesPass(pm);
            LLVMAddLoopRotatePass(pm);
            LLVMAddLoopVectorizePass(pm);
        }

        LLVMAddCFGSimplificationPass(pm);
        LLVMAddIndVarSimplifyPass(pm);
        LLVMAddScalarReplAggregatesPass(pm);
        LLVMAddLoopVectorizePass(pm);
        LLVMAddConstantMergePass(pm);
        LLVMAddSLPVectorizePass(pm);
	    LLVMAddPromoteMemoryToRegisterPass(pm);
	    LLVMAddMergedLoadStoreMotionPass(pm);
	    LLVMAddAggressiveDCEPass(pm);
	    LLVMAddAlwaysInlinerPass(pm);
	    LLVMAddStripDeadPrototypesPass(pm);
        LLVMAddCFGSimplificationPass(pm);

        LLVMPassManagerBuilderRef pmb = LLVMPassManagerBuilderCreate();
        LLVMPassManagerBuilderSetOptLevel(pmb, Compiler::settings.optLevel);
        LLVMPassManagerBuilderPopulateModulePassManager(pmb, pm);
    }

    LLVMRunPassManager(pm, generator->lModule);
    #else
    LLVMPassBuilderOptionsRef pbOptions = LLVMCreatePassBuilderOptions();

    if(Compiler::settings.optLevel == 1) LLVMRunPasses(generator->lModule, "default<O1>", machine, pbOptions);
    else if(Compiler::settings.optLevel == 2) LLVMRunPasses(generator->lModule, "default<O2>", machine, pbOptions);
    else if(Compiler::settings.optLevel == 3) LLVMRunPasses(generator->lModule, "default<O3>", machine, pbOptions);

    LLVMDisposePassBuilderOptions(pbOptions);
    #endif

    LLVMTargetMachineEmitToFile(machine, generator->lModule, (char*)(std::regex_replace(file, std::regex("\\.rave"), ".o")).c_str(), LLVMObjectFile, &errors);
    if(errors != nullptr) {
        Compiler::error("target machine emit to file: " + std::string(errors));
        std::exit(1);
    }

    end = std::chrono::steady_clock::now();
    Compiler::genTime += std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

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
            Compiler::error("file '" + Compiler::files[i] + "' does not exists!");
            return;
        }
        if(Compiler::files[i].size() > 2 && (endsWith(Compiler::files[i], ".a") || endsWith(Compiler::files[i], ".o") || endsWith(Compiler::files[i], ".lib")))
            Compiler::linkString += Compiler::files[i] + " ";
        else {
            Compiler::compile(Compiler::files[i]);
            if(Compiler::settings.emitLLVM) {
                char* err;
                LLVMPrintModuleToFile(generator->lModule, (Compiler::files[i]+".ll").c_str(), &err);
            }
            std::string compiledFile = std::regex_replace(Compiler::files[i], std::regex("\\.rave"), ".o");
            Compiler::linkString += compiledFile + " ";
            if(!Compiler::settings.saveObjectFiles) toRemove.push_back(compiledFile);
        }
    }

    for(int i=0; i<AST::addToImport.size(); i++) {
        std::string fname = replaceAll(AST::addToImport[i], ">", "");
        if(std::count(Compiler::toImport.begin(), Compiler::toImport.end(), fname) == 0 &&
           std::count(Compiler::files.begin(), Compiler::files.end(), fname) == 0
        ) Compiler::toImport.push_back(fname);
    }

    for(int i=0; i<Compiler::toImport.size(); i++) {
        if(access(Compiler::toImport[i].c_str(), 0) != 0) {
            Compiler::error("file '" + Compiler::files[i] + "' does not exists!");
            return;
        }
        if(Compiler::toImport[i].size() > 2 && (endsWith(Compiler::toImport[i], ".a") || endsWith(Compiler::toImport[i], ".o") || endsWith(Compiler::toImport[i], ".lib")))
            Compiler::linkString += Compiler::toImport[i] + " ";
        else {
            if(
                Compiler::toImport[i].find(exePath + "std/") != std::string::npos && !Compiler::settings.recompileStd &&
                access(std::regex_replace(Compiler::toImport[i], std::regex("\\.rave"), std::string(".") + Compiler::outType + ".o").c_str(), 0) != -1
            ) linkString += std::regex_replace(Compiler::toImport[i], std::regex("\\.rave"), std::string(".") + Compiler::outType + ".o") + " ";
            else {
                compile(Compiler::toImport[i]);
                if(Compiler::settings.emitLLVM) {
                    char* err;
                    LLVMPrintModuleToFile(generator->lModule, (Compiler::toImport[i] + ".ll").c_str(), &err);
                }
                std::string compiledFile = std::regex_replace(Compiler::toImport[i], std::regex("\\.rave"), ".o");
                linkString += compiledFile + " ";

                if(Compiler::toImport[i].find(exePath + "std/") != std::string::npos) {
                    std::ifstream src(compiledFile, std::ios::binary);
                    std::ofstream dst(std::regex_replace(compiledFile, std::regex("\\.o"), std::string(".") + Compiler::outType + ".o"), std::ios::binary);
                    dst << src.rdbuf();
                }

                toRemove.push_back(compiledFile);
            }
        }
    }

    if(Compiler::outFile == "") Compiler::outFile = "a";

    if(Compiler::settings.onlyObject) Compiler::linkString += "-r ";
    if(Compiler::settings.isStatic) Compiler::linkString += "-static ";
    if(Compiler::settings.isPIC) Compiler::linkString += "-no-pie ";

    Compiler::linkString += " " + Compiler::settings.linkParams + " -Wno-unused-command-line-argument ";

    #ifdef _WIN32
        if(Compiler::options["compiler"].template get<std::string>().find("clang") != std::string::npos) Compiler::linkString += " -fuse-ld=ld ";
    #endif

    ShellResult result = exec(Compiler::linkString + " -o " + Compiler::outFile);
    if(result.status != 0) {
        Compiler::error("error when linking!\nLinking string: '" + Compiler::linkString+" -o " + Compiler::outFile + "'");
        std::exit(result.status);
        return;
    }

    for(int i=0; i<toRemove.size(); i++) std::remove(toRemove[i].c_str());

    std::cout << "Time spent by lexer: " << std::to_string(Compiler::lexTime) << "ms\nTime spent by parser: " << std::to_string(Compiler::parseTime) << "ms\nTime spent by generator: " << std::to_string(Compiler::genTime) << "ms" << std::endl;
}
