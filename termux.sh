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