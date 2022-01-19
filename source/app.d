public import lexer;
public import logger;
public import tokens;
import std.conv : to;
import std.stdio;

void main()
{
	Lexer lexer = new Lexer(
	"
	def main {
		int a=0;
	}
	");
	for(int i=0; i<lexer.get_tokenlist().length; i++) {
		writeln(
			"TYPE: "~to!string(lexer.get_tokenlist().get_token(i).get_type())~"\n"~
			"VALUE: "~lexer.get_tokenlist().get_token(i).get_val()
			~"\n"
		);
	}
}