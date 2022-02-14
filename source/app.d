import std.stdio;
import lexer;
import tokens;
import std.conv : to;
import core.stdc.stdlib : exit;
import cmd;
import std.file : readText;

void main(string[] args)
{
	input(args);
	Lexer lexer = new Lexer(readText(source_file));
	for(int i=0; i<lexer.getTokens().length; i++) {
		TokType type = lexer.getTokens().get(i).type;
		string value = lexer.getTokens().get(i).value;
		if(type==TokType.tok_cmd) {
			writeln("Type: "~to!string(lexer.getTokens().get(i).cmd)~" "~value);
		}
		else writeln("Type: "~to!string(type)~" "~value);
	}
}
