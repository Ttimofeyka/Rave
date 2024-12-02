BIN = rave

LLVM_VERSION =

LLVM_FLAGS =

LLVM_CONFIG =

FLAGS ?= -O3

LLVM_STATIC ?= 0

COMPILER ?= $(CXX)

WINBUILD = 0
ifdef OS
	ifeq ($(OS),Windows_NT)
		WINBUILD = 1
	endif
endif

ifeq ($(WINBUILD),1)
	LLVM_VERSION = 16
	BIN = rave.exe
	SRC = $(patsubst ".\\%",$ .\src\\%, $(shell getFiles))
	LLVM_COMPILE_FLAGS = `llvm-config --cflags`
	LLVM_LINK_FLAGS = ./LLVM/lib/LLVM-C.lib
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

ifeq ($(WINBUILD),0)
OBJ = $(SRC:%.cpp=%.o)
else
OBJ = $(SRC:%.cpp=%.obj)
endif

all: $(BIN)

ifeq ($(WINBUILD),0)
$(BIN): $(OBJ)
	$(COMPILER) $(OBJ) -o $@ $(LLVM_FLAGS) -DLLVM_VERSION=$(LLVM_VERSION) -lstdc++fs
%.o: %.cpp
	$(COMPILER) -c $< -o $@ -DLLVM_VERSION=$(LLVM_VERSION) -std=c++17 -Wno-deprecated $(FLAGS) $(LLVM_FLAGS) -fexceptions
else
$(BIN): $(OBJ)
	$(COMPILER) $(OBJ) -o $@ $(LLVM_LINK_FLAGS) -DLLVM_VERSION=$(LLVM_VERSION) -lstdc++
%.obj: %.cpp
	$(COMPILER) -c $< -o $@ -DLLVM_VERSION=$(LLVM_VERSION) -std=c++17 -Wno-deprecated $(FLAGS) $(LLVM_COMPILE_FLAGS) -fexceptions
endif

clean:
	rm -rf src/*.o src/parser/*.o src/parser/nodes/*.o src/lexer/*.o \
		src/*.obj src/parser/*.obj src/parser/nodes/*.obj src/lexer/*.obj
