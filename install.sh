#!/usr/bin/sh

# Rave Dependencies Installer
# original by Ttimofeyka
# modified ver by RedLeader167 (JustKenux)
# https://github.com/Ttimofeyka/Rave


ARCH=0

ISTERMUX="${TERMUX_VERSION:-no}"
if [ -f "/etc/arch-release" ]; then
    ARCH=1
    echo \[ Rave Installer \] Arch Linux detected, switching to ARCH mode.
fi

failure ()
{
    echo \[ Rave Installer \] Failed to install $1 - possible problems\: missing in repos, no connection, other errors?
    echo \[ Rave Installer \] Script terminated.
    exit 1
}

success ()
{
    echo \[ Rave Installer \] Successfully installed $1.
}

i_llvm11 ()
{
    echo \[ Rave Installer \] Installing LLVM-11
    if [ "$ARCH" -eq 1 ]; then
        if sudo pacman -S llvm-11-libs clang10 ; then
            rm options.json
            echo -e -n "{\n    \"compiler\": \"clang-10\"\n}"
            success LLVM-11
        else
            failure LLVM-11
        fi
    else
        if sudo apt install llvm-11 lldb-11 llvm-11-dev libllvm11 llvm-11-runtime clang-11 ; then
            success LLVM-11
        else
            failure LLVM-11
        fi
    fi
}

i_dmd ()
{
    echo \[ Rave Installer \] Installing DMD
    if [ "$ARCH" -eq 1 ]; then
        if pacman -S dmd ; then
            success DMD
        else
            failure DMD
        fi
    else
        if curl -fsS https://dlang.org/install.sh | bash -s dmd ; then
            success DMD
        else
            failure DMD
        fi
    fi
}

i_llvm11
i_dmd

echo \[ Rave Installer \] Done.
