import std.stdio;
import lexer.mlexer, lexer.tokens;
import std.conv : to;
import std.path : buildNormalizedPath, absolutePath;
import core.stdc.stdlib : exit;
import std.file : readText;
import lexer.preproc;
import parser.analyzer;
import parser.mparser;
import parser.ast;
import parser.typesystem;
import parser.generator.gen, llvm;
import std.getopt;

void implementDefaultTypeContext(AtstTypeContext ctx) {
	ctx.setType("int",    new TypeBasic(BasicType.t_int));
	ctx.setType("short",  new TypeBasic(BasicType.t_short));
	ctx.setType("long",   new TypeBasic(BasicType.t_long));
	ctx.setType("size",   new TypeBasic(BasicType.t_size));
	ctx.setType("char",   new TypeBasic(BasicType.t_char));
	ctx.setType("uint",   new TypeBasic(BasicType.t_uint));
	ctx.setType("ushort", new TypeBasic(BasicType.t_ushort));
	ctx.setType("ulong",  new TypeBasic(BasicType.t_ulong));
	ctx.setType("usize",  new TypeBasic(BasicType.t_usize));
	ctx.setType("uchar",  new TypeBasic(BasicType.t_uchar));
	ctx.setType("float",  new TypeBasic(BasicType.t_float));
}

void main(string[] args)
{
	string outputFile = "a.o";
	string stdlibIncPath = buildNormalizedPath(absolutePath("./stdlib"));
	string outputType = "i686-linux";
	bool debugMode = false;

	auto helpInformation = getopt(
	    args,
	    "o", "Output file", &outputFile,
	    "debug", "Debug mode", &debugMode,
	    "stdinc", "Stdlib include path (':' in #inc)", &stdlibIncPath,
		"type", "Output file type", &outputType
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

	auto parser = new Parser(preproc.getTokens());
	auto nodes = parser.parseProgram();

	auto genctx = new GenerationContext();
	implementDefaultTypeContext(genctx.sema.typeContext);

	auto semaScope = new AnalyzerScope(genctx.sema, genctx);

	writeln("------------------ AST -------------------");
	bool hadErrors = false;
	int errorCount = 0;
	for(int i = 0; i < nodes.length; ++i) {
		// writeln("Analyzing... #", i);
		nodes[i].analyze(semaScope, null);
		if(genctx.sema.errorCount > 0) {
			hadErrors = true;
			errorCount += genctx.sema.errorCount;
			genctx.sema.flushErrors();
			break;
		}
		nodes[i].debugPrint(0);
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
		exit(1);
	}
}
