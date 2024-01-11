@echo off
Powershell.exe -executionpolicy remotesigned -File ./llvm-patch.ps1

choco install make mingw wget

wget --tries=inf https://github.com/terralang/llvm-build/releases/download/llvm-15.0.2/clang+llvm-15.0.2-x86_64-windows-msvc17.7z

if not exist "LLVM-15" mkdir LLVM-15
if not exist "LLVM-15/bin" xcopy /s clang+llvm-15.0.2-x86_64-windows-msvc17 LLVM-15

if exist "clang+llvm-15.0.2-x86_64-windows-msvc17" rd /s /q "clang+llvm-15.0.2-x86_64-windows-msvc17"
if exist "clang+llvm-15.0.2-x86_64-windows-msvc17.7z" del "clang+llvm-15.0.2-x86_64-windows-msvc17.7z"

pause