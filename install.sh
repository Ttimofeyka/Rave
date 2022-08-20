if [ -f "/etc/arch-release" ]; then
    pacman -S dmd
    echo Please, install LLVM-10.
else
    sudo apt install llvm-10 lldb-10 llvm-10-dev libllvm10 llvm-10-runtime dmd
fi

echo Done.