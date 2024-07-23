BIN = rave

LLVM_VERSION ?= 16

LLVM_LIB = -lLLVM-$(LLVM_VERSION)

FLAGS ?= -O3

LLVM_STATIC ?= 0

COMPILER ?= $(CXX)

ifdef OS
	BIN = rave.exe
	SRC = $(patsubst ".\\%",$ .\src\\%, $(shell getFiles))
	LLVM_LIB = ./LLVM/lib/LLVM-C.lib `llvm-config --cflags`
else
	ifeq (, $(shell which llvm-config-16))
 		LLVM_VERSION = 15
 	endif
	ifeq ($(LLVM_STATIC), 1)
		LLVM_LIB = `llvm-config-$(LLVM_VERSION) --ldflags --link-static --libs --system-libs --cxxflags` -static
	else
		LLVM_LIB = -lLLVM-$(LLVM_VERSION) `llvm-config-$(LLVM_VERSION) --cxxflags`
	endif
	SRC = $(shell find . -name *.cpp)
endif

OBJ = $(SRC:%.cpp=%.o)

all: $(BIN)

$(BIN): $(OBJ)
	$(COMPILER) $(OBJ) -o $@ $(LLVM_LIB) -DLLVM_VERSION=$(LLVM_VERSION) -lstdc++fs
%.o: %.cpp
	$(COMPILER) -c $< -o $@ -DLLVM_VERSION=$(LLVM_VERSION) -std=c++17 -Wno-deprecated $(FLAGS) $(LLVM_LIB) -fexceptions -lstdc++fs

clean:
ifdef OS
	del /s src\*.o
else
	rm -rf src/*.o src/parser/*.o src/parser/nodes/*.o src/lexer/*.o
endif
