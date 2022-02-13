module logger;

import std.stdio;
import core.stdc.stdlib : exit;

void lexer_error(string s) {
    writeln("Lexer error: "~s);
    exit(-1);
}