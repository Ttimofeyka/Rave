module logger;

import std.stdio;
import core.stdc.stdlib : exit;

void error(string str) {
    writeln("ERROR: "~str);
    exit(1);
}
void warning(string str) {
    writeln("WARNING: "~str);
}