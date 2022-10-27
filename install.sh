#!/usr/bin/sh

# Rave Dependencies Installer
# original by Ttimofeyka
# modified ver by RedLeader167 (JustKenux)
# https://github.com/Ttimofeyka/Rave


ARCH=0
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

i_llvm10 ()
{
    echo \[ Rave Installer \] Installing LLVM-10
    if [ "$ARCH" -eq 1 ]; then
        echo \[ Rave Installer \] LLVM-10 is unavailable in Arch Linux repos. Build it by yourself.
    else
        if sudo apt install llvm-10 lldb-10 llvm-10-dev libllvm10 llvm-10-runtime ; then
            success LLVM-10
        else
            failure LLVM-10
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

i_llvm10
i_dmd

echo \[ Rave Installer \] Done.
