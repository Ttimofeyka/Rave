module parser.util;

import parser.ast;
import std.conv : to;
import std.string, parser.typesystem;
import llvm;

T instanceof(T)(Object o) if(is(T == class)) {
	return cast(T) o;
}