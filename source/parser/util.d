module parser.util;

import core.stdc.stdlib : malloc, realloc, free;

T instanceof(T)(Object o) if(is(T == class)) {
	return cast(T) o;
}

const size_t BASICMM_BLOCK_SIZE = 1024;

@nogc class StackMemoryManager {
	private void* data = null;
	private size_t allocSize = 0, actualSize = 0;

	private void reallocData() {
		if(allocSize >= actualSize) return;
		
		while(allocSize < actualSize)
			allocSize += BASICMM_BLOCK_SIZE;
		
		if(data == null) data = malloc(allocSize);
		else data = realloc(data, allocSize);
	}

	T *createBuffer(T)(size_t size) {
		actualSize += size * T.sizeof;
		reallocData();
		return cast(T*)data;
	}

	void cleanup() {
		if(data != null) free(data);
		allocSize = 0;
		actualSize = 0;
	}
}

StackMemoryManager stackMemory;

string namespacesToGenName(string[] ns, string original, string type) {
	// Example:
	// Original name - foo
	// ns - A,B
	// Name = _nsAnsBvarfoo
	string newname = "_";

	for(int i=0; i<ns.length; i++) {
		newname ~= "ns"~ns[i];
	}

	return (newname ~ type ~ original);
}

string namespacesToVarName(string[] ns, string original) {
	// Example:
	// Original name - foo
	// ns - A,B
	// Name = A::B::foo
	string newname = "";

	for(int i=0; i<ns.length; i++) {
		newname ~= ns[i] ~ "::";
	}

	return (newname ~ original);
}

bool firstNamespaceEqual(string ns, string name) {
	int i = 0;
	while(i<ns.length) {
		if(i>=name.length) return false;
		if(name[i] != ns[i]) return false;
		i += 1;
	}
	return true;
}

string removeFirstNamespace(string name) {
	int i = 0;
	while(name[i] != ':') i += 1;
	i += 2;
	return name[i..$];
}