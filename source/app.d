import std.stdio;
import lexer;
import tokens;
import std.conv : to;
import core.stdc.stdlib : exit;
import cmd;
import std.file : readText;
import preproc;
import parser;
import llvm;

void main(string[] args)
{
  	LLVMInitializeNativeTarget();

	input(args);
	auto lexer = new Lexer(readText(source_file));
	auto preproc = new Preprocessor(lexer.getTokens());

	auto parse = new Parser(preproc.getTokens());
	auto nodes = parse.parseProgram();
	
	for(int i = 0; i < nodes.length; ++i) {
		nodes[i].debugPrint(0);
	}
}
