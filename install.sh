#!/usr/bin/sh

# Rave Dependencies Installer

if [ -f "/etc/arch-release" ]; then
    echo Arch Linux detected.
    sudo pacman -S llvm15 clang10
    echo Please, change clang-11 into options.json to clang-10.
    echo If clang-10 does not work, you can use other versions of clang or gcc.
    echo Done.
else
    if [ -f "/etc/debian_version" ]; then
        echo Debian-based detected.
        sudo apt install llvm-15 llvm-15-dev clang-11 lld-11 gcc-mingw-w64 binutils-mingw-w64
        echo Done.
    else
        echo Unsupported platform detected.
    fi
fi