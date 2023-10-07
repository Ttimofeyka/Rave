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
	$(COMPILER) $(OBJ) -o $@ $(LLVM_LIB) -DLLVM_$(LLVM_VERSION) -I/usr/include/llvm-c-15 -I/usr/include/llvm-c-15/llvm-c
%.o: %.c
	$(COMPILER) -c $< -o $@ -DLLVM_$(LLVM_VERSION) -std=c++11 -Wno-deprecated -I/usr/include/llvm-c-15