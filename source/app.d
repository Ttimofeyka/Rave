import std.stdio;
import lexer;
import tokens;
import std.conv : to;
import core.stdc.stdlib : exit;
import cmd;
import std.file : readText;
import preproc;

void main(string[] args)
{
	input(args);
	Lexer lexer = new Lexer(readText(source_file));
	auto preproc = new Preprocessor(lexer.getTokens());

	for(int i=0; i<preproc.getTokens().length; i++) {
		TokType type = preproc.getTokens().get(i).type;
		string value = preproc.getTokens().get(i).value;
		if(type==TokType.tok_cmd) {
			writeln("Type: "~to!string(preproc.getTokens().get(i).cmd)~" "~value);
		}
		else writeln("Type: "~to!string(type)~" "~value);
	}
}
