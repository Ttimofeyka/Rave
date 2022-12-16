#!/usr/bin/sh

# Rave Dependencies Installer
# original by Ttimofeyka
# modified ver by RedLeader167 (JustKenux)
# https://github.com/Ttimofeyka/Rave


ARCH=0
TERMUX=0
if [ -f "/etc/arch-release" ]; then
    ARCH=1
    echo \[ Rave Installer \] Arch Linux detected, switching to ARCH mode.
fi
if [ command -v termux-setup-storage ]; then
    TERMUX=1
    echo \[ Rave Installer \] Termux detected, switching to TERMUX mode.
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
        if sudo pacman -S llvm-11-libs ; then
            success LLVM-11
        else
            failure LLVM-11
        fi
    else
        if [ "$TERMUX" -eq 1 ]; then
            pkg i wget
            cpu = uname -m
            if [ cpu -eq "aarch64" ]; then
            wget https://github.com/amuramatsu/termux-packages/releases/download/libllvm11_11.1.0-4/libllvm11_11.1.0-4_aarch64.deb
                if dpkg -i libllvm11_11.1.0-4/libllvm11_11.1.0-4_aarch64.deb ; then
                    success LLVM-11
                else
                    failure LLVM-11
                fi
                wget https://github.com/ldc-developers/ldc/releases/download/v1.30.0/ldc2-1.30.0-linux-aarch64.tar.xz
                mkdir _ldc
                tar -xf ldc2-1.30.0-linux-aarch64.tar.xz -C _ldc/
                echo export PATH=$PATH:~/_ldc/bin > ldc_init.sh
                echo Write the \"bash command ldc_init.sh\" for the end of the installation.
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
