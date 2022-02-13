import std.stdio;
import lexer;
import tokens;
import std.conv : to;

void main()
{
	Lexer lexer = new Lexer(
	`
	int main() {
		ret 0; []
	}
	`);
	for(int i=0; i<lexer.getTokens().length; i++) {
		tok_type type = lexer.getTokens().get(i).type;
		string value = lexer.getTokens().get(i).value;
		if(type==tok_type.tok_cmd) {
			writeln("Type: "~to!string(lexer.getTokens().get(i).cmd)~" "~value);
		}
		else writeln("Type: "~to!string(type)~" "~value);
	}
}
