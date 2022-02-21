import std.stdio;
import lexer;
import tokens;
import std.conv : to;
import core.stdc.stdlib : exit;
import std.file : readText;
import preproc;
import parser;
import gen, llvm;
import std.getopt;

void main(string[] args)
{
	string outputFile = "a.o";
	bool debugMode = false;

	auto helpInformation = getopt(
		args,
		"o", "Output file", &outputFile,
		"debug", "Debug mode", &debugMode,
	);
	
	if(helpInformation.helpWanted)
	{
		defaultGetoptPrinter("Some information about the program.", helpInformation.options);
		return;
	}

	string[] sourceFiles = args[1..$].dup;
	string sourceFile = sourceFiles[0];

  	LLVMInitializeNativeTarget();

	auto lexer = new Lexer(sourceFile, readText(sourceFile));
	auto preproc = new Preprocessor(lexer.getTokens());

	// preproc.getTokens().debugPrint();

	auto parser = new Parser(preproc.getTokens());
	auto nodes = parser.parseProgram();
	
	/*writeln("-------------- Declarations --------------");
	foreach(decl; parser.getDecls()) {
		writeln(decl.toString());
	}*/

	writeln("------------------ AST -------------------");
	for(int i = 0; i < nodes.length; ++i) {
		nodes[i].debugPrint(0);
	}

	auto ctx = new GenerationContext();
	ctx.gen(nodes, outputFile, debugMode);
}
