#!/bin/sh
sed -i 's/<llvm-11\//</g' ./src/include/llvm-c/Target.h
sed -i 's/<llvm-12\//</g' ./src/include/llvm-c/Target.h
sed -i 's/<llvm-13\//</g' ./src/include/llvm-c/Target.h
sed -i 's/<llvm-14\//</g' ./src/include/llvm-c/Target.h
sed -i 's/<llvm-15\//</g' ./src/include/llvm-c/Target.h
sed -i 's/<llvm-16\//</g' ./src/include/llvm-c/Target.h