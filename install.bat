@echo off
choco install make mingw wget 7zip.install

wget --tries=inf https://github.com/Ttimofeyka/llvm-mingw/releases/download/v1.0/LLVM.7z

7z x LLVM.7z

del LLVM.7z

pause