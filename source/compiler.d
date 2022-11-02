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

class Compiler {
    private string linkString = "ld.lld ";
    private string outfile;
    private string outtype;
    private CompOpts opts;

    this(string outfile, string outtype, CompOpts opts) {
        this.outfile = outfile;
        this.outtype = outtype;
        this.opts = opts;
        if(outtype.indexOf("x86_64") != -1) {
            if(!opts.noEntry) linkString = linkString~thisExePath()[0..$-4]~"rt/linux/x86_64/crt1.o ";
            if(!opts.noStd) linkString = linkString~thisExePath()[0..$-4]~"rt/linux/x86_64/libc.a ";
        }
        else if(outtype.indexOf("i686") != -1) {
            if(!opts.noEntry) linkString = linkString~thisExePath()[0..$-4]~"rt/linux/i686/crt1.o ";
            if(!opts.noStd) linkString = linkString~thisExePath()[0..$-4]~"rt/linux/i686/libc.a ";
        }
        else if(outtype.indexOf("aarch64") != -1) {
            if(!opts.noEntry) linkString = linkString~thisExePath()[0..$-4]~"rt/linux/aarch64/crt1.o ";
            if(!opts.noStd) linkString = linkString~thisExePath()[0..$-4]~"rt/linux/aarch64/libc.a ";
        }
        else if(outtype.indexOf("mips64") != -1) {
            if(!opts.noEntry) linkString = linkString~thisExePath()[0..$-4]~"rt/linux/mips64/crt1.o ";
            if(!opts.noStd) linkString = linkString~thisExePath()[0..$-4]~"rt/linux/mips64/libc.a ";
        }
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
        //if(files[0] == file) {
            if(outtype.indexOf("i686") != -1) content = "__aliasp __RAVE_X86 = true;\n"~content;
            else if(outtype.indexOf("x86_64") != -1) content = "__aliasp __RAVE_X86_64 = true;\n"~content;
            else if(outtype.indexOf("aarch64") != -1) content = "__aliasp __RAVE_AARCH64 = true;\n"~content;
            else if(outtype.indexOf("mips") != -1) content = "__aliasp __RAVE_MIPS = true;\n"~content;
        //}

        if(!opts.noPrelude && file != "std/prelude.rave" && file != "std/memory.rave") {
            content = "import <std/prelude>\n import <std/memory>\n"~content;
        }

        Lexer lex = new Lexer(content);
        Parser p = new Parser(lex.getTokens());
        p.currentFile = file;
        p.MainFile = files[0];
        p.parseAll();
        Node[] n = p.getNodes();
        for(int i=0; i<n.length; i++) {
            n[i].check();
        }
        Generator = new LLVMGen(file);
        if(opts.printAll) writeln("File: "~file);
        for(int i=0; i<n.length; i++) {
            if(opts.printAll) writeln("\t"~n[i].classinfo.name);
            n[i].generate();
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
    
        //char* m = LLVMPrintModuleToString(Generator.Module);
        //writeln("Module: \n",fromStringz(m));

        clearAll();
    }

    void compileAll() {
        version(X86) {__X86 = true;}

        if(opts.onlyObject) {
            if(!isFile(files[0])) {
                writeln("Error: file \""~files[0]~"\" doesn't exists!");
                exit(1);
            }
            compile(files[0]);
            if(opts.emit_llvm) {
                char* err;
                LLVMPrintModuleToFile(Generator.Module, toStringz(files[0]~".ll"), &err);
            }
            return;
        }
        string[] toRemove;
        for(int i=0; i<files.length; i++) {
            if(!isFile(files[i])) {
                writeln("Error: file \""~files[i]~"\" doesn't exists!");
                exit(1);
            }
            if(files[i][$-2..$]==".a"||files[i][$-4..$]==".dll"||files[i][$-2..$]==".o"||files[i][$-3..$]==".so") {
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
        linkString ~= " "~opts.linkparams;
        writeln(linkString~" -o "~outfile);
        auto l = executeShell(linkString~" -o "~outfile);
        if(l.status != 0) writeln("Linking error: "~l.output);
        for(int i=0; i<toRemove.length; i++) {
            remove(toRemove[i]);
        }
    }
}
