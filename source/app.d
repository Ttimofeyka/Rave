public import lexer;
public import logger;
public import tokens;
import std.conv : to;
import std.stdio;

void main()
{
	Lexer lexer = new Lexer(
	"
	def main : int {
		ret 0;
	}
	");
}