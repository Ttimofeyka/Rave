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
