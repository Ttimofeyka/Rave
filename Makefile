BIN = rave

FLAGS ?= -O3

COMPILER ?= $(CXX)

LLVM_VERSION =
LLVM_FLAGS =
LLVM_CONFIG = llvm-config
LLVM_STATIC ?= 0

WINBUILD = 0
ifdef OS
	ifeq ($(OS),Windows_NT)
		WINBUILD = 1
	endif
endif

ifeq ($(WINBUILD),1)
	BIN = rave.exe
	ifeq ("$(wildcard ~/LLVM)", "")
		LLVM_CONFIG = ./LLVM/bin/llvm-config.exe
	endif
	LLVM_FULL_VERSION = $(shell $(LLVM_CONFIG) --version)
	LLVM_VERSION = $(firstword $(subst ., ,$(LLVM_FULL_VERSION)))
	# We assume that if this config query outputs 'shared' then LLVM can be linked in both ways
	# Otherwise static only
	ifneq (shared, $(shell $(LLVM_CONFIG) --shared-mode))
		LLVM_STATIC = 1
		LLVM_NO_SHARED = 1
	endif
	# the following branch is thoughtful, as LLVM_STATIC has priority over llvm's default mode
	LLVM_STATIC_FLAG1 =
	LLVM_STATIC_FLAG2 =
	ifeq ($(LLVM_STATIC), 1)
		LLVM_STATIC_FLAG1 = --link-static
		LLVM_STATIC_FLAG2 = --system-libs
	endif
	LLVM_LINK_FLAGS = `$(LLVM_CONFIG) $(LLVM_STATIC_FLAG1) $(LLVM_STATIC_FLAG2) --libs --ldflags`
	LLVM_LIBDIR = $(shell $(LLVM_CONFIG) $(LLVM_STATIC_FLAG1) --libdir)
	SRC = $(patsubst ".\\%",$ .\src\\%, $(shell ./getFiles.sh))
	LLVM_COMPILE_FLAGS = `$(LLVM_CONFIG) --cflags`
else
	ifneq (, $(shell command -v llvm-config-16))
 		LLVM_VERSION = 16
		LLVM_CONFIG = llvm-config-16
	else ifneq (, $(shell command -v llvm-config-15))
		LLVM_VERSION = 15
		LLVM_CONFIG = llvm-config-15
	else ifneq (, $(shell command -v llvm-config))
		LLVM_FULL_VERSION = $(shell llvm-config --version)
		LLVM_VERSION = $(firstword $(subst ., ,$(LLVM_FULL_VERSION)))
		LLVM_CONFIG = llvm-config
	endif
	ifeq ($(LLVM_STATIC), 1)
		LLVM_FLAGS = `$(LLVM_CONFIG) --ldflags --link-static --libs --system-libs --cxxflags`
	else
		LLVM_FLAGS = `$(LLVM_CONFIG) --libs --cxxflags`
	endif
	SRC = $(shell find ./src -name *.cpp)
endif

ifeq ($(WINBUILD),0)
OBJ = $(SRC:%.cpp=obj/linux/%.o)
else
OBJ = $(SRC:%.cpp=obj/win/%.o)
endif

all: $(BIN)

ifeq ($(WINBUILD),0)
$(BIN): $(OBJ)
	$(COMPILER) $(OBJ) -o $@ $(LLVM_FLAGS) -DLLVM_VERSION=$(LLVM_VERSION) -lstdc++fs
obj/linux/%.o: %.cpp
	$(shell mkdir -p obj/linux/src/parser/nodes obj/linux/src/lexer)
	$(COMPILER) -c $< -o $@ -DLLVM_VERSION=$(LLVM_VERSION) -std=c++17 -Wno-deprecated $(FLAGS) $(LLVM_FLAGS) -fexceptions
else
$(BIN): $(OBJ)
	$(COMPILER) $(OBJ) -o $@ $(LLVM_LINK_FLAGS) -DLLVM_VERSION=$(LLVM_VERSION) -lstdc++
obj/win/%.o: %.cpp
	$(shell mkdir -p obj/win/src/parser/nodes obj/win/src/lexer)
	$(COMPILER) -c $< -o $@ -DLLVM_VERSION=$(LLVM_VERSION) -std=c++17 -Wno-deprecated $(FLAGS) $(LLVM_COMPILE_FLAGS) -fexceptions
endif

clean:
	rm -rf obj

cleanStd:
	rm -rf std/*.o std/*.ll
