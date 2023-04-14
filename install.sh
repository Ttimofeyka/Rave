#!/usr/bin/sh

# Rave Dependencies Installer
# original and modified by Ttimofeyka
# modified version by RedLeader167 (JustKenux)
# https://github.com/Ttimofeyka/Rave

if [ -f "/etc/arch-release" ]; then
    echo Arch Linux detected.
    sudo pacman -S llvm14 ldc dub clang10
    echo You need to change LLVM-11 to LLVM-14 in dub.json and change compiler into options.json to clang-10.
    echo If clang-10 does not work, you can use other versions of clang or gcc.
    echo However, in this case, you should change the compiler.json, replacing the old compiler with its own and adding the -no-pie flag in case of possible errors with PIE/PIC.
    echo Done.
else
    if [ -f "/etc/debian_version" ]; then
        echo Debian-based detected.
        echo For compatibility, LLVM-11 is being installed.
        echo If desired, you can later install other versions of LLVM at your discretion.
        sudo apt install llvm-11 llvm-11-dev clang-11
        wget https://downloads.dlang.org/releases/2.x/2.103.0/dmd_2.103.0-0_amd64.deb
        sudo dpkg -i dmd_2.103.0-0_amd64.deb
        rm dmd_2.103.0-0_amd64.deb
        echo Done.
    else
        echo Unsupported platform detected.
    fi
fi
