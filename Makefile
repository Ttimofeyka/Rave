SRC = $(shell find . -name *.cpp)

OBJ = $(SRC:%.cpp=%.o)

BIN = rave

LLVM_LIB = `llvm-config-15 --libs`

LLVM_VERSION = 15

COMPILER = clang++

ifdef OS
	BIN = rave.exe
endif

all: $(BIN)

$(BIN): $(OBJ)
	$(COMPILER) $(OBJ) -o $@ $(LLVM_LIB) -DLLVM_$(LLVM_VERSION)
%.o: %.cpp
	$(COMPILER) -c $< -o $@ -DLLVM_$(LLVM_VERSION) -std=c++11 -Wno-deprecated

clean: 
	rm -rf src/*.o src/parser/*.o src/parser/nodes/*.o src/lexer/*.o