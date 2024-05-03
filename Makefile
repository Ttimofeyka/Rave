BIN = rave

LLVM_VERSION ?= 16

LLVM_LIB = -lLLVM-$(LLVM_VERSION)

FLAGS = -O2

LLVM_STATIC ?= 0

COMPILER = $(CXX)

ifdef OS
	BIN = rave.exe
	SRC = $(patsubst ".\\%",$ .\src\\%, $(shell FORFILES /P src /S /M *.cpp /C "CMD /C ECHO @relpath"))
	LLVM_LIB = ./LLVM/lib/LLVM-C.lib
else
	ifeq (, $(shell which llvm-config-16))
 		LLVM_VERSION = 15
 	endif
	ifeq ($(LLVM_STATIC), 1)
		LLVM_LIB = `llvm-config-$(LLVM_VERSION) --ldflags --link-static --libs --system-libs` -static
	else
		LLVM_LIB = -lLLVM-$(LLVM_VERSION)
	endif
	SRC = $(shell find . -name *.cpp)
endif

OBJ = $(SRC:%.cpp=%.o)

all: $(BIN)

$(BIN): $(OBJ)
	$(COMPILER) $(OBJ) -o $@ $(LLVM_LIB) -DLLVM_VERSION=$(LLVM_VERSION)
%.o: %.cpp
	$(COMPILER) -c $< -o $@ -DLLVM_VERSION=$(LLVM_VERSION) -std=c++11 -Wno-deprecated $(FLAGS)

clean:
ifdef OS
	del /s src\*.o
else
	rm -rf src/*.o src/parser/*.o src/parser/nodes/*.o src/lexer/*.o
endif
