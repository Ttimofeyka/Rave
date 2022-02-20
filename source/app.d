import std.stdio;
import lexer;
import tokens;
import std.conv : to;
import core.stdc.stdlib : exit;
import cmd;
import std.file : readText;
import preproc;
import parser;
import gen, llvm;

void main(string[] args)
{
  	LLVMInitializeNativeTarget();

	input(args);
	auto lexer = new Lexer(source_file, readText(source_file));
	auto preproc = new Preprocessor(lexer.getTokens());

	// preproc.getTokens().debugPrint();

	auto parser = new Parser(preproc.getTokens());
	auto nodes = parser.parseProgram();
	
	writeln("-------------- Declarations --------------");
	foreach(decl; parser.getDecls()) {
		writeln(decl.toString());
	}

	writeln("------------------ AST -------------------");
	for(int i = 0; i < nodes.length; ++i) {
		nodes[i].debugPrint(0);
	}

	auto ctx = new GenerationContext();
	ctx.gen(nodes[0]);
}
