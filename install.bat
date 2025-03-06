@echo off
choco install make mingw wget 7zip.install

wget --tries=inf https://github.com/terralang/llvm-build/releases/download/llvm-17.0.5/clang+llvm-17.0.5-x86_64-windows-msvc17.7z

7z x clang+llvm-17.0.5-x86_64-windows-msvc17.7z -oclang+llvm-17.0.5-x86_64-windows-msvc17

if not exist "LLVM" mkdir LLVM
if not exist "LLVM/bin" xcopy /e /v clang+llvm-17.0.5-x86_64-windows-msvc17 LLVM

if exist "clang+llvm-17.0.5-x86_64-windows-msvc17" rd /s /q "clang+llvm-17.0.5-x86_64-windows-msvc17"
if exist "clang+llvm-17.0.5-x86_64-windows-msvc17.7z" del "clang+llvm-17.0.5-x86_64-windows-msvc17.7z"

pause