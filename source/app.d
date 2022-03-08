import std.stdio;
import std.conv : to;
import std.path : buildNormalizedPath, absolutePath;
import core.stdc.stdlib : exit;
import std.file : readText;
// import parser.analyzer, parser.mparser, parser.ast, parser.typesystem, parser.generator.gen, llvm;
import lexer.mlexer;
import lexer.tokens;
import lexer.preproc;
import parser.util;
import compiler.compiler;
import user.jsondoc;
import std.getopt;

void main(string[] args)
{
	stackMemory = new StackMemoryManager();

	string outputFile = "a.o";
	string stdlibIncPath = buildNormalizedPath(absolutePath("./stdlib"));
	string outputType = "i686-linux";
	bool debugMode = false;
	bool generateDocJson = false;
	string docJsonFile = "docs.json";

	auto helpInformation = getopt(
	    args,
	    "o", "Output file", &outputFile,
	    "debug", "Debug mode", &debugMode,
	    "stdinc", "Stdlib include path (':' in @inc)", &stdlibIncPath,
		"type", "Output file type", &outputType,
		"doc-json", "Generate JSON documentation file", &generateDocJson,
		"doc-json-file", "JSON documentation file name", &docJsonFile,
	);
	
    if(helpInformation.helpWanted)
    {
	    defaultGetoptPrinter("Help", helpInformation.options);
	    return;
    }

    string[] sourceFiles = args[1..$].dup;
    string sourceFile = sourceFiles[0];

  	LLVMInitializeNativeTarget();
	
	TList[string] defines;
	defines["_FILE"] = new TList();
	defines["_FILE"].insertBack(new Token(SourceLocation(0, 0, sourceFile), '"' ~ sourceFile ~ '"'));
	defines["_OFILE"] = new TList();
	defines["_OFILE"].insertBack(new Token(SourceLocation(0, 0, outputFile), '"' ~ outputFile ~ '"'));
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

	preproc.getTokens().debugPrint();

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

	

	if(generateDocJson) {
		auto jsonDocGen = new JSONDocGenerator();

		foreach(node; nodes) {
			jsonDocGen.generate(nodes[i]);
		}

		jsonDocGen.output(docJsonFile);
	}

	if(!hadErrors) {
		writeln("------------------ Generating -------------------");
		genctx.gen(semaScope, nodes, outputFile, debugMode);
		if(genctx.sema.errorCount > 0) {
			genctx.sema.flushErrors();
		}
	}
	else {
		writeln("Failed with ", errorCount, " error(s).");
		stackMemory.cleanup();
		exit(1);
	}

	stackMemory.cleanup();
}
