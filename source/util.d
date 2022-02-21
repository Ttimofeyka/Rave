module util;

import ast;
import std.conv : to;
import std.string, typesystem;
import llvm;

T instanceof(T)(Object o) if(is(T == class)) {
	return cast(T) o;
}