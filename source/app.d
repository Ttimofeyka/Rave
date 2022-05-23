import std.stdio;
import std.conv : to;
import std.path : buildNormalizedPath, absolutePath;
import core.stdc.stdlib : exit;
import parser.analyzer, parser.mparser, parser.ast, parser.typesystem, parser.generator.gen;
import lexer.mlexer;
import lexer.tokens;
import lexer.preproc;
import parser.util;
import compiler.compiler;
import user.jsondoc;
import std.getopt;
import std.process;
import std.array;
import std.string;
import std.file : remove;
import std.conv : to;
import std.file;
import llvm;
import std.path;

string stdlibIncPath = "";
string outputSimpleType = "";
bool debugMode = false;
bool debugModeFull = false;
bool generateDocJson = false;
string docJsonFile = "docs.json";
bool not_delete_o = false;
int optLevel = 1;
TList[string] defines;
TList[string] macros;
string[][string] macrosDN;
bool[string] incF;

string linker;

string platformFileType = "";
bool noprelude = false;

void compile(string linkFiles, bool nolink, string sourceFile, string outputType, string outputFile) {
	stdlibIncPath = buildNormalizedPath(absolutePath("./stdlib"));

	// writeln("------------------ Lexer -------------------");
	string srcF = readText(sourceFile);
	if(!noprelude) srcF = "@inc \"std/base\"\n" ~ srcF;
	auto lexer = new Lexer(sourceFile, srcF);
	auto preproc = new Preprocessor(lexer.getTokens(), stdlibIncPath, defines, macros, incF, macrosDN);

	if(preproc.hadErrors) {
		writeln("Failed with 1 or more errors.");
		stackMemory.cleanup();
		exit(1);
	}

	auto parser = new Parser(preproc.getTokens());
	auto nodes = parser.parseProgram();

	if(parser.hadErrors) {
		writeln("Failed with 1 or more errors.");
		stackMemory.cleanup();
		exit(1);
	}

	Compiler comp = new Compiler(parser.getDecls(), nodes);
	comp.debugInfo.debugMode = debugMode;
	comp.debugInfo.printAst = debugModeFull;
	comp.optLevel = optLevel;
	comp.defs = defines.dup;
	CompilerError err = comp.generate(outputType, outputFile~".o");

	if(err.hadError) {
		stackMemory.cleanup();
		exit(1);
	}

	if(generateDocJson) {
		auto jsonDocGen = new JSONDocGenerator();
		foreach(node; nodes) jsonDocGen.generate(node);
		jsonDocGen.output(docJsonFile);
	}

	stackMemory.cleanup();

	// Linking with runtime
	
	if(!nolink) {
		auto cmd = executeShell(linker~" "~outputFile~platformFileType~" "~linkFiles~" -o "~outputFile);
		if(cmd.status != 0) writeln(cmd.output);
		if(!not_delete_o) remove(outputFile~platformFileType);
	}
}

void main(string[] args)
{
	string outputType = "";
	string outputFile = "a.o";
	bool nolink = false;
	bool isshared = false;
	bool noentry = false;
	stackMemory = new StackMemoryManager();

	auto helpInformation = getopt(
	    args,
	    "o", "Output file", &outputFile,
	    "debug", "Debug mode", &debugMode,
	    "debug-full", "Full debug mode", &debugModeFull,
	    "stdinc", "Stdlib include path (':' in @inc)", &stdlibIncPath,
		"type", "Output file(platform) type", &outputType,
		"doc-json", "Generate JSON documentation file", &generateDocJson,
		"doc-json-file", "JSON documentation file name", &docJsonFile,
		"nolink", "Disable auto-link with runtime", &nolink,
		"no-rm-obj", "Don't remove object file", &not_delete_o,
		"shared", "Linking as shared library", &isshared,
		"noentry", "Linking without begin.o", &noentry,
		"opt", "Optimization level", &optLevel,
		"linker", "Set linker", &linker,
		"noprelude", "Disable automatically include std/base", &noprelude,
	);
	
    if(helpInformation.helpWanted)
    {
	    defaultGetoptPrinter("Help", helpInformation.options);
	    return;
    }

	if(linker == null) {
		version(linux) { linker = "ld.lld";}
		version(Windows) { linker = "lld-link";}
		version(OSX) { linker = "ld64.lld";}
	}
	
	if(outputType == "") {
		outputType = to!string(fromStringz(LLVMGetDefaultTargetTriple()));
	}

	if(outputType.indexOf("linux") != -1) {
		if(outputType.indexOf("x86_64") != -1) {
			outputSimpleType = "x86_64-linux";
		}
		else outputSimpleType = "i686-linux";
		platformFileType = ".o";
	}
	else if(outputType.indexOf("win") != -1) {
		if(outputType.indexOf("x86_64") != -1) {
			outputSimpleType = "win64";
		}
		else outputSimpleType = "win32";
		platformFileType = ".obj";
	}

    string[] sourceFiles = args[1..$].dup;
    string sourceFile = sourceFiles[0];

	defines["_MFILE"] = new TList();
	defines["_MFILE"].insertBack(new Token(SourceLocation(0,0,""), sourceFile));
	defines["_FILE"] = new TList();
	defines["_FILE"].insertBack(new Token(SourceLocation(0, 0, sourceFile), sourceFile));
	defines["_OFILE"] = new TList();
	defines["_OFILE"].insertBack(new Token(SourceLocation(0, 0, outputFile), outputFile));
	defines["_PLATFORM"] = new TList();
	defines["_PLATFORM"].insertBack(new Token(SourceLocation(0,0,""),"\""~outputType~"\""));
	defines["true"] = new TList();
	defines["true"].insertBack(new Token(SourceLocation(0,0,""),"cast"));
	defines["true"].insertBack(new Token(SourceLocation(0,0,""),"(", TokType.tok_lpar));
	defines["true"].insertBack(new Token(SourceLocation(0,0,""),"bool", TokType.tok_id));
	defines["true"].insertBack(new Token(SourceLocation(0,0,""),")", TokType.tok_rpar));
	defines["true"].insertBack(new Token(SourceLocation(0,0,""),"1",TokType.tok_num)); //
	defines["false"] = new TList();
	defines["false"].insertBack(new Token(SourceLocation(0,0,""),"cast"));
	defines["false"].insertBack(new Token(SourceLocation(0,0,""),"(", TokType.tok_rpar));
	defines["false"].insertBack(new Token(SourceLocation(0,0,""),"bool", TokType.tok_id));
	defines["false"].insertBack(new Token(SourceLocation(0,0,""),")", TokType.tok_rpar));
	defines["false"].insertBack(new Token(SourceLocation(0,0,""),"0",TokType.tok_num)); //

	if(outputType.indexOf("linux") != -1) {
		defines["_LINUX"] = new TList();
		defines["_LINUX"].insertBack(new Token(SourceLocation(0,0,""),""));
	}
	else if(outputType.indexOf("windows") != -1) {
		defines["_WINDOWS"] = new TList();
		defines["_WINDOWS"].insertBack(new Token(SourceLocation(0,0,""),""));
	}

	if(outputType.indexOf("i686") != -1) {
		defines["_I686"] = new TList();
		defines["_I686"].insertBack(new Token(SourceLocation(0,0,""),""));
	}

	if(debugMode) defines["_DEBUG"] = new TList();
	else defines["_NDEBUG"] = new TList();

	string path_to_rt = dirName(thisExePath())~"/rt/"~outputSimpleType~"/";

	string linkF = path_to_rt~"rt"~platformFileType~" ";
	if(!noentry) linkF ~= path_to_rt~"begin"~platformFileType~" ";
	if(isshared) linkF ~= "-shared ";
	if("_WINDOWS" in defines) linkF ~= "lib/kernel32.lib ";

  	LLVMInitializeNativeTarget();

	if(sourceFiles.length > 1) {
		string[] to_remove;
		for(int i=1; i<sourceFiles.length; i++) {
			string currF = sourceFiles[i];
			if( // If object file or library just add to link-files
				currF[$-1..$] == "o"
				||
				currF[$-1..$] == "a"
				||
				(currF.length > 2
				&&
				(currF[$-3..$] == "dll" 
			|| currF[$-3..$] == "lib"))) linkF ~= sourceFiles[i] ~ " ";
			else { // Else compile source file
				compile("",true,sourceFiles[i],outputType,sourceFiles[i]);
				noprelude = true;
				linkF ~= sourceFiles[i]~platformFileType ~ " ";
				to_remove ~= sourceFiles[i]~platformFileType;
			}
		}
		compile(linkF, false, sourceFile, outputType, outputFile);

		if(!not_delete_o) {
			for(int i=0; i<to_remove.length; i++) {
				remove(to_remove[i]);
			}
		}
	}
	else compile(linkF, nolink, sourceFile, outputType, outputFile);
}