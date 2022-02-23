import std.stdio;
import lexer.mlexer, lexer.tokens;
import std.conv : to;
import std.path : buildNormalizedPath, absolutePath;
import core.stdc.stdlib : exit;
import std.file : readText;
import lexer.preproc;
import parser.mparser;
import parser.gen, llvm;
import std.getopt;

void main(string[] args)
{
	string outputFile = "a.o";
	string stdlibIncPath = buildNormalizedPath(absolutePath("./stdlib"));
	bool debugMode = false;

	auto helpInformation = getopt(
	    args,
	    "o", "Output file", &outputFile,
	    "debug", "Debug mode", &debugMode,
	    "stdinc", "Stdlib include path (':' in #inc)", &stdlibIncPath,
	);
	
    if(helpInformation.helpWanted)
    {
	    defaultGetoptPrinter("Some information about the program.", helpInformation.options);
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

	if(debugMode) defines["_DEBUG"] = new TList();
	else defines["_NDEBUG"] = new TList();
	

	auto lexer = new Lexer(sourceFile, readText(sourceFile));
	auto preproc = new Preprocessor(lexer.getTokens(), stdlibIncPath, defines);

	auto parser = new Parser(preproc.getTokens());
	auto nodes = parser.parseProgram();

	writeln("------------------ AST -------------------");
	for(int i = 0; i < nodes.length; ++i) {
		nodes[i].debugPrint(0);
	}

	auto ctx = new GenerationContext();
	ctx.gen(nodes, outputFile, debugMode);
}
