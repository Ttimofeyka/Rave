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

string hasAnyone(string str, string[] strs) {
    for(int i=0; i<strs.length; i++) {
        if(str.indexOf(strs[i]) != -1) return strs[i];
    }
    return "";
}

class Compiler {
    private string linkString = "clang ";
    private string outfile;
    private string outtype;
    private CompOpts opts;
    int lexTime = 0;
    int parseTime = 0;
    int generateTime = 0;

    private void error(string msg) {
        pragma(inline,true);
		writeln("\033[0;31mError: " ~ msg ~ "\033[0;0m");
		exit(1);
	}

    this(string outfile, string outtype, CompOpts opts) {
        this.outfile = outfile;
        this.outtype = outtype;
        this.opts = opts;

        void addLibs(string platform) {
            //if(!opts.noEntry) linkString = linkString~thisExePath()[0..$-4]~"rt/linux/"~platform~"/crt1.o ";
            //if(!opts.noStd) linkString = linkString~thisExePath()[0..$-4]~"rt/linux/"~platform~"/libc.a ";
            if(opts.isPIE) linkString = linkString~"-fPIE ";
        }

        string p = hasAnyone(outtype,["x86_64","i686","aarch64","mips64"]);

        if(p != "") addLibs(p);
        else error("unknown platform!");
    }

    void clearAll() {
        VarTable.clear();
        Generator.Globals.clear();
        Generator.Functions.clear();
        Generator.currBB = null;
        FuncTable.clear();
        AliasTable.clear();
        StructTable.clear();
        Generator.Structs.clear();
        structsNumbers.clear();
        ConstVars.clear();
        currScope = null;
        MethodTable.clear();
    }

    void compile(string file) {
        string content = readText(file);

        int offset = 1;

        if(outtype.indexOf("i686") != -1) content = "__aliasp __RAVE_X86 = true;\n"~content;
        else if(outtype.indexOf("x86_64") != -1) content = "__aliasp __RAVE_X86_64 = true;\n"~content;
        else if(outtype.indexOf("aarch64") != -1) content = "__aliasp __RAVE_AARCH64 = true;\n"~content;
        else if(outtype.indexOf("mips") != -1) content = "__aliasp __RAVE_MIPS = true;\n"~content;

        if(!opts.noPrelude && file != "std/prelude.rave" && file != "std/memory.rave") {
            content = "import <std/prelude> <std/memory>\n"~content;
        }

        auto startT = MonoTime.currTime;
        Lexer lex = new Lexer(content,offset);
        auto endT = MonoTime.currTime;
        auto dur = endT - startT;
        lexTime += dur.total!"msecs";

        startT = MonoTime.currTime;

        Parser p = new Parser(lex.getTokens(),offset);
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
        Generator = new LLVMGen(file);
        if(opts.printAll) writeln("File: "~file);
        for(int i=0; i<n.length; i++) {
            if(opts.printAll) writeln("\t"~n[i].classinfo.name);
            n[i].generate();
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

        LLVMTargetMachineEmitToFile(machine,Generator.Module,file_ptr, LLVMObjectFile, &errors);

        endT = MonoTime.currTime;
        dur = endT - startT;
        generateTime += dur.total!"msecs";
    
        //char* m = LLVMPrintModuleToString(Generator.Module);
        //writeln("Module: \n",fromStringz(m));

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
                version(linux) {
                    compile(files[i]);
                    /*if(i==0) {
                        if(opts.emit_llvm) {
                            char* err;
                            LLVMPrintModuleToFile(Generator.Module, toStringz(outfile~".ll"), &err);
                        }
                    }*/
                    if(opts.emit_llvm) {
                            char* err;
                            LLVMPrintModuleToFile(Generator.Module, toStringz(files[i]~".ll"), &err);
                    }
                    linkString ~= files[i].replace(".rave",".o ");
                    toRemove ~= files[i].replace(".rave",".o");
                }
            }
        }
        if(outfile == "") outfile = "a";
        else {
            outfile = dirName(files[0])~"/"~outfile;
        }
        if(opts.onlyObject) linkString ~= " -r ";
        linkString ~= " "~opts.linkparams;
        //writeln(linkString~" -o "~outfile);
        auto l = executeShell(linkString~" -o "~outfile);
        if(l.status != 0) writeln("Linking error: "~l.output);
        for(int i=0; i<toRemove.length; i++) {
            if(toRemove[i] != outfile) remove(toRemove[i]);
        }
        writeln("Time spent by the lexer: ",lexTime," ms\nTime spent by parser: ",parseTime," ms\nTime spent by code generator: ",generateTime," ms");
    }
}
