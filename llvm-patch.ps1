(Get-Content ./src/include/llvm-c/Target.h).Replace('<llvm-15/', '"../../../LLVM-15/include/') | Set-Content ./src/include/llvm-c/Target.h
(Get-Content ./src/include/llvm-c/Target.h).Replace('>', '"') | Set-Content ./src/include/llvm-c/Target.h
