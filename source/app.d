import std.stdio;
import std.conv : to;
import std.path : buildNormalizedPath, absolutePath;
import core.stdc.stdlib : exit;
import std.file : readText;
import parser.analyzer, parser.mparser, parser.ast, parser.typesystem, parser.generator.gen, llvm;
import lexer.mlexer;
import lexer.tokens;
import lexer.preproc;
import parser.util;
import compiler.compiler;
import user.jsondoc;
import std.getopt;
import std.process;
import std.file : thisExePath;
import std.array;
import std.string;
import std.file : remove;
import std.conv : to;

void main(string[] args)
{
	stackMemory = new StackMemoryManager();

	string outputFile = "a.o";
	string stdlibIncPath = buildNormalizedPath(absolutePath("./stdlib"));
	string outputType = "";
	bool debugMode = false;
	bool debugModeFull = false;
	bool generateDocJson = false;
	string docJsonFile = "docs.json";
	bool nolink = false;
	bool not_delete_o = false;

	auto helpInformation = getopt(
	    args,
	    "o", "Output file", &outputFile,
	    "debug", "Debug mode", &debugMode,
	    "debug-full", "Full debug mode", &debugModeFull,
	    "stdinc", "Stdlib include path (':' in @inc)", &stdlibIncPath,
		"type", "Output file(platform) type", &outputType,
		"doc-json", "Generate JSON documentation file", &generateDocJson,
		"doc-json-file", "JSON documentation file name", &docJsonFile,
		"nolink", "Disable auto-link", &nolink,
		"no-rm-obj", "Don't remove object file", &not_delete_o
	);
	
    if(helpInformation.helpWanted)
    {
	    defaultGetoptPrinter("Help", helpInformation.options);
	    return;
    }

	if(outputType == "") {
		outputType = to!string(fromStringz(LLVMGetDefaultTargetTriple()));
	}

    string[] sourceFiles = args[1..$].dup;
    string sourceFile = sourceFiles[0];

  	LLVMInitializeNativeTarget();
	
	TList[string] defines;
	defines["_MFILE"] = new TList();
	defines["_MFILE"].insertBack(new Token(SourceLocation(0,0,""), sourceFile));
	defines["_FILE"] = new TList();
	defines["_FILE"].insertBack(new Token(SourceLocation(0, 0, sourceFile), sourceFile));
	defines["_OFILE"] = new TList();
	defines["_OFILE"].insertBack(new Token(SourceLocation(0, 0, outputFile), outputFile));
	defines["_PLATFORM"] = new TList();
	defines["_PLATFORM"].insertBack(new Token(SourceLocation(0,0,""),"\""~outputType~"\""));
	defines["true"] = new TList();
	defines["true"].insertBack(new Token(SourceLocation(0,0,""),"1"));
	defines["false"] = new TList();
	defines["false"].insertBack(new Token(SourceLocation(0,0,""),"0"));

	if(debugMode) defines["_DEBUG"] = new TList();
	else defines["_NDEBUG"] = new TList();

	// writeln("------------------ Lexer -------------------");
	auto lexer = new Lexer(sourceFile, readText(sourceFile));
	auto preproc = new Preprocessor(lexer.getTokens(), stdlibIncPath, defines);

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
	if(outputType.indexOf("x86_64") >=0 && outputType.indexOf("linux") >=0) outputType = "x86_64-linux";
	else if(outputType.indexOf("i386") >=0 || outputType.indexOf("i686") >=0) {
		outputType = "i686-linux";
	}
	if(!nolink){
		string platformFileType;
		if(outputType.indexOf("linux") >= 0) platformFileType = ".o";
		string path_to_rt = thisExePath()
			.replace("/rave.exe","/")
			.replace("/rave","/")~"rt/"~outputType~"/rt"~platformFileType;
		executeShell("ld.lld "~outputFile~".o "~path_to_rt~" -o "~outputFile);
		if(!not_delete_o) remove(outputFile~".o");
	}
}
