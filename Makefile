BIN = rave

LLVM_VERSION =

LLVM_FLAGS =

LLVM_CONFIG =

FLAGS ?= -O3

LLVM_STATIC ?= 0

COMPILER ?= $(CXX)

ifdef OS
	LLVM_VERSION = 16
	BIN = rave.exe
	SRC = $(patsubst ".\\%",$ .\src\\%, $(shell getFiles))
	LLVM_FLAGS = ./LLVM/lib/LLVM-C.lib `llvm-config --cflags`
else
	ifneq (, $(shell command -v llvm-config-16))
 		LLVM_VERSION = 16
		LLVM_CONFIG = llvm-config-16
	else
	ifneq (, $(shell command -v llvm-config-15))
		LLVM_VERSION = 15
		LLVM_CONFIG = llvm-config-15
	else
	ifneq (, $(shell command -v llvm-config))
		temp_llvmver = $(shell llvm-config --version)
		LLVM_VERSION = $(firstword $(subst ., ,$(temp_llvmver)))
		LLVM_CONFIG = llvm-config
	endif
	endif
	endif
	ifeq ($(LLVM_STATIC), 1)
		LLVM_FLAGS = `$(LLVM_CONFIG) --ldflags --link-static --libs --system-libs --cxxflags -static`
	else
		LLVM_FLAGS = `$(LLVM_CONFIG) --libs --cxxflags`
	endif
	SRC = $(shell find . -name *.cpp)
endif

OBJ = $(SRC:%.cpp=%.o)

all: $(BIN)

$(BIN): $(OBJ)
	$(COMPILER) $(OBJ) -o $@ $(LLVM_FLAGS) -DLLVM_VERSION=$(LLVM_VERSION) -lstdc++fs
%.o: %.cpp
	$(COMPILER) -c $< -o $@ -DLLVM_VERSION=$(LLVM_VERSION) -std=c++17 -Wno-deprecated $(FLAGS) $(LLVM_FLAGS) -fexceptions -lstdc++fs

clean:
	rm -rf src/*.o src/parser/*.o src/parser/nodes/*.o src/lexer/*.o
