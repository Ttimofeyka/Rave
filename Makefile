BIN = rave

LLVM_VERSION = 16

LLVM_LIB = -lLLVM-$(LLVM_VERSION)

COMPILER = $(CXX)

ifdef OS
	BIN = rave.exe
	SRC = $(patsubst ".\\%",$ .\src\\%, $(shell FORFILES /P src /S /M *.cpp /C "CMD /C ECHO @relpath"))
	LLVM_LIB = ./LLVM-16/lib/LLVM-C.lib
else
	ifeq (, $(shell which llvm-config-16))
 		LLVM_VERSION = 15
		LLVM_LIB = -lLLVM-$(LLVM_VERSION)
 	endif
	SRC = $(shell find . -name *.cpp)
endif

OBJ = $(SRC:%.cpp=%.o)

all: $(BIN)

$(BIN): $(OBJ)
	$(COMPILER) $(OBJ) -o $@ $(LLVM_LIB) -DLLVM_VERSION=$(LLVM_VERSION)
%.o: %.cpp
	$(COMPILER) -c $< -o $@ -DLLVM_VERSION=$(LLVM_VERSION) -std=c++11 -Wno-deprecated

clean:
	rm -rf src/*.o src/parser/*.o src/parser/nodes/*.o src/lexer/*.o