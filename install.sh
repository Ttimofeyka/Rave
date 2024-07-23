#!/usr/bin/sh

# Rave Dependencies Installer

SUDO=sudo
if [ -z "$(command -v sudo)" ]; then
    SUDO=doas
fi

if [ -f "/etc/arch-release" ]; then
    echo Arch Linux detected.
    yay -S llvm16 clang16
    echo Done.
    exit 0
fi


if [ -f "/etc/debian_version" ]; then
    echo Debian-based detected.
    $SUDO apt install llvm-16 llvm-16-dev clang-16
    echo Done.
    exit 0
fi


if [ ! -z "$(command -v xbps-install)" ]; then
    echo Void Linux detected.
    $SUDO xbps-install -S clang llvm15
    ./llvm-patch.sh
    echo Done.
    exit 0
fi

echo Unsupported platform detected.
