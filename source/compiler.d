/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

module compiler;

import lexer.lexer;
import lexer.tokens;
import app : CompOpts;
import std.file;
import std.stdio : writeln;
import core.stdc.stdlib : exit;
import parser.parser;
import parser.ast;
import llvm;
import std.string;
import std.process;
import app : files;
import std.datetime, std.datetime.stopwatch;
import std.json, std.file, std.path;
import std.algorithm : canFind;

string hasAnyone(string str, string[] strs) {
    for(int i=0; i<strs.length; i++) {
        if(str.indexOf(strs[i]) != -1) return strs[i];
    }
    return "";
}

class Compiler {
    private string linkString;
    private string outfile;
    private string outtype;
    private CompOpts opts;
    private JSONValue options;
    int lexTime = 0;
    int parseTime = 0;
    int generateTime = 0;
    string[] toImport;

    private void error(string msg) {
        pragma(inline,true);
		    writeln("\033[0;31mError: " ~ msg ~ "\033[0;0m");
		    exit(1);
	}

    this(string outfile, string outtype, CompOpts opts) {
        this.outfile = outfile;
        this.outtype = outtype;
        this.opts = opts;
        try {
            this.options = parseJSON(readText(dirName(thisExePath())~"/options.json"));
        } catch(Error e) {
            write(dirName(thisExePath())~"/options.json","{\n\t\"compiler\": \"clang-11\"\n}");
        }
        this.linkString = options.object["compiler"].str~" ";

        if(opts.isPIE) linkString = linkString~"-fPIE ";
        if(opts.noStd) linkString = linkString~"-nostdlib ";
        if(opts.noEntry) linkString = linkString~"--no-entry ";
    }

    void clearAll() {
        VarTable.clear();
        Generator.Globals.clear();
        Generator.Functions.clear();
        Generator.currentBuiltinArg = 0;
        Generator.currBB = null;
        FuncTable.clear();
        AliasTable.clear();
        StructTable.clear();
        Generator.Structs.clear();
        structsNumbers.clear();
        ConstVars.clear();
        currScope = null;
        MethodTable.clear();
        _importedFiles = [];
        condStack.clear();
        aliasTypes.clear();
    }

    void compile(string file) {
        string content = readText(file);

        int offset = 1;
        string oldContent = content;

        if(outtype.indexOf("i686") != -1 || outtype.indexOf("i386") != -1) content = "alias __RAVE_PLATFORM = \"X86\";";
        else if(outtype.indexOf("x86_64") != -1) content = "alias __RAVE_PLATFORM = \"X86_64\"; ";
        else if(outtype.indexOf("aarch64") != -1) content = "alias __RAVE_PLATFORM = \"AARCH64\"; ";
        else if(outtype.indexOf("mips64") != -1) content = "alias __RAVE_PLATFORM = \"MIPS64\"; ";
        else if(outtype.indexOf("mips") != -1) content = "alias __RAVE_PLATFORM = \"MIPS\"; ";
        else if(outtype.indexOf("powerpc64") != -1) content = "alias __RAVE_PLATFORM = \"POWERPC64\"; ";
        else if(outtype.indexOf("powerpc") != -1) content = "alias __RAVE_PLATFORM = \"POWERPC\"; ";
        else if(outtype.indexOf("sparcv9") != -1) content = "alias __RAVE_PLATFORM = \"SPARCV9\"; ";
        else if(outtype.indexOf("sparc") != -1) content = "alias __RAVE_PLATFORM = \"SPARC\"; ";
        else if(outtype.indexOf("s390x") != -1) content = "alias __RAVE_PLATFORM = \"S390X\"; ";
        else if(outtype.indexOf("wasm") != -1) content = "alias __RAVE_PLATFORM = \"WASM\"; ";
        else content = "alias __RAVE_PLATFORM = \"UNKNOWN\"; ";
        
        if(outtype.indexOf("windows") != -1) content = `alias __RAVE_OS = "WINDOWS"; `~content;
        else if(outtype.indexOf("linux") != -1) content = `alias __RAVE_OS = "LINUX"; `~content;
        else if(outtype.indexOf("darwin") != -1 || outtype.indexOf("macos") != -1) content = `alias __RAVE_OS = "DARWIN"; `~content;
        else content = `alias __RAVE_OS = "UNKNOWN"; `~content;

        content = content~"\n"~oldContent;

        if(!opts.noPrelude && file != "std/prelude.rave" && file != "std/memory.rave") {
            content = "import <std/prelude> <std/memory>\n"~content;
        }

        auto startT = MonoTime.currTime;
        Lexer lex = new Lexer(content,offset);
        auto endT = MonoTime.currTime;
        auto dur = endT - startT;
        lexTime += dur.total!"msecs";

        startT = MonoTime.currTime;

        Parser p = new Parser(lex.getTokens(),offset,file);
        p.currentFile = file;
        p.MainFile = files[0];
        p.parseAll();
        Node[] n = p.getNodes();
        for(int i=0; i<n.length; i++) {
            n[i].check();
        }

        endT = MonoTime.currTime;
        dur = endT - startT;
        parseTime += dur.total!"msecs";

        startT = MonoTime.currTime;
        Generator = new LLVMGen(file,opts);
        if(opts.printAll) writeln("File: "~file);
        for(int i=0; i<n.length; i++) {
            if(opts.printAll) writeln("\t"~n[i].classinfo.name);
            n[i].generate();
        }

        if(_importedFiles.length > 0) {
            foreach(k; _importedFiles) {
                if(!canFind(toImport,k)) {
                    toImport ~= k;
                }
            }
        }

        if(opts.optimizeLevel > 0) {
            LLVMPassManagerRef pm = LLVMCreatePassManager();

            LLVMPassManagerBuilderRef pmb = LLVMPassManagerBuilderCreate();
            LLVMPassManagerBuilderSetOptLevel(pmb,opts.optimizeLevel);
            LLVMPassManagerBuilderPopulateModulePassManager(pmb,pm);
            LLVMRunPassManager(pm,Generator.Module);
        }

        char* errors;
        LLVMTargetRef target;
        char* triple = LLVMNormalizeTargetTriple(toStringz(outtype));
    	LLVMGetTargetFromTriple(triple, &target, &errors);
    	LLVMTargetMachineRef machine = LLVMCreateTargetMachine(
			target,
			triple,
			"generic",
			LLVMGetHostCPUFeatures(),
			 LLVMCodeGenLevelDefault,
			 LLVMRelocDefault,
		LLVMCodeModelDefault);

        LLVMDisposeMessage(errors);
    	LLVMSetTarget(Generator.Module, triple);
    	LLVMTargetDataRef datalayout = LLVMCreateTargetDataLayout(machine);
    	char* datalayout_str = LLVMCopyStringRepOfTargetData(datalayout);
    	LLVMSetDataLayout(Generator.Module, datalayout_str);
    	LLVMDisposeMessage(datalayout_str);
		char* file_ptr = cast(char*)toStringz(file.replace(".rave",".o"));
        //char* m = LLVMPrintModuleToString(Generator.Module);
        //writeln("Module: \n",fromStringz(m));
        LLVMTargetMachineEmitToFile(machine,Generator.Module,file_ptr, LLVMObjectFile, &errors);
        endT = MonoTime.currTime;
        dur = endT - startT;
        generateTime += dur.total!"msecs";

        clearAll();
    }

    void compileAll() {
        import std.path : dirName;
        version(X86) {__X86 = true;}

        string[] toRemove;
        for(int i=0; i<files.length; i++) {
            if(!isFile(files[i])) {
                writeln("Error: file \""~files[i]~"\" doesn't exists!");
                exit(1);
            }
            if((files[i].length>2 && (files[i][$-2..$]==".a" || files[i][$-2..$]==".o"))
                ||
                (files[i].length>4 && (files[i][$-4..$]==".dll" || files[i][$-4..$]==".obj"))
                ||
                (files.length>3 && files[i][$-3..$]==".so")) {
                    linkString ~= files[i]~" ";
            }
            else {
                    compile(files[i]);
                    if(opts.emit_llvm) {
                            char* err;
                            LLVMPrintModuleToFile(Generator.Module, toStringz(files[i]~".ll"), &err);
                    }
                    linkString ~= files[i].replace(".rave",".o ");
                    toRemove ~= files[i].replace(".rave",".o");
            }
        }
        for(int i=0; i<toImport.length; i++) {
            if(!isFile(toImport[i])) {
                writeln("Error: file \""~toImport[i]~"\" doesn't exists!");
                exit(1);
            }
            if((toImport[i].length>2 && (toImport[i][$-2..$]==".a" || toImport[i][$-2..$]==".o"))
                ||
                (toImport[i].length>4 && (toImport[i][$-4..$]==".dll" || toImport[i][$-4..$]==".obj"))
                ||
                (files.length>3 && toImport[i][$-3..$]==".so")) {
                    linkString ~= toImport[i]~" ";
            }
            else {
                    compile(toImport[i]);
                    if(opts.emit_llvm) {
                            char* err;
                            LLVMPrintModuleToFile(Generator.Module, toStringz(toImport[i]~".ll"), &err);
                    }
                    linkString ~= toImport[i].replace(".rave",".o ");
                    toRemove ~= toImport[i].replace(".rave",".o");
            }
        }
        if(outfile == "") outfile = "a";
        else {
            outfile = dirName(files[0])~"/"~outfile;
        }
        if(opts.onlyObject) linkString ~= " -r ";
        linkString ~= " "~opts.linkparams;

        if(outtype != "") linkString = linkString~"-target "~outtype~" ";

        if(!_libraries.empty) {
            for(int i=0; i<_libraries.length; i++) {
                linkString = linkString~"-l"~_libraries[i]~" ";
            }
        }
        auto l = executeShell(linkString~" -o "~outfile);
        if(l.status != 0) {
            writeln("Linking error: "~l.output~"\nLinking string: '"~linkString~" -o "~outfile~"'");
            for(int i=0; i<toRemove.length; i++) {
                if(toRemove[i] != outfile) remove(toRemove[i]);
            }
            exit(l.status);
        }
        for(int i=0; i<toRemove.length; i++) {
            if(toRemove[i] != outfile) remove(toRemove[i]);
        }
        writeln("Time spent by lexer: ",lexTime," ms\nTime spent by parser: ",parseTime," ms\nTime spent by code generator: ",generateTime," ms");
    }
}
