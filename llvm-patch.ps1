(Get-Content ./src/include/llvm-c/Target.h).Replace('<llvm-16/', '"../../../LLVM/include/') | Set-Content ./src/include/llvm-c/Target.h
(Get-Content ./src/include/llvm-c/Target.h).Replace('<llvm-15/', '"../../../LLVM/include/') | Set-Content ./src/include/llvm-c/Target.h
(Get-Content ./src/include/llvm-c/Target.h).Replace('<llvm-14/', '"../../../LLVM/include/') | Set-Content ./src/include/llvm-c/Target.h
(Get-Content ./src/include/llvm-c/Target.h).Replace('<llvm-13/', '"../../../LLVM/include/') | Set-Content ./src/include/llvm-c/Target.h
(Get-Content ./src/include/llvm-c/Target.h).Replace('<llvm-12/', '"../../../LLVM/include/') | Set-Content ./src/include/llvm-c/Target.h
(Get-Content ./src/include/llvm-c/Target.h).Replace('<llvm-11/', '"../../../LLVM/include/') | Set-Content ./src/include/llvm-c/Target.h
(Get-Content ./src/include/llvm-c/Target.h).Replace('>', '"') | Set-Content ./src/include/llvm-c/Target.h
