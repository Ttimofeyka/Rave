#!/usr/bin/sh

# Rave Dependencies Installer

if [ -f "/etc/arch-release" ]; then
    echo Arch Linux detected.
    sudo pacman -S llvm15 clang15
    echo Done.
else
    if [ -f "/etc/debian_version" ]; then
        echo Debian-based detected.
        sudo apt install llvm-15 llvm-15-dev clang-15 gcc-mingw-w64 binutils-mingw-w64
        echo Done.
    else
        echo Unsupported platform detected.
    fi
fi