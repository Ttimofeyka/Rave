function checkLib {
    REQUIRED_PKG=$1
    PKG_OK=$(dpkg-query -W --showformat='${Status}\n' $REQUIRED_PKG|grep "install ok installed")
    echo Checking for $REQUIRED_PKG: $PKG_OK
    if [ "" = "$PKG_OK" ]; then
        echo "No $REQUIRED_PKG. Installing $REQUIRED_PKG."
        sudo apt-get --yes install $REQUIRED_PKG
    fi
}   

if [command -v termux-setup-storage]; then
    clear
    pkg install ldc
    pkg install wget
    pkg install binutils
    pkg install make
    pkg install cmake
    echo "You need install llvm-10 for Termux yourself."
else
    clear
    if [ -f "/etc/arch-release" ]; then
        sudo pacman -S dlang
        sudo pacman -S dub
        sudo pacman -S binutils
        echo "Unfortunately, llvm-10 is not available for your platform, so you will have to build and install it yourself."
        echo "The script will download the source code for you."
        sudo wget https://github.com/llvm/llvm-project/releases/download/llvmorg-10.0.0/llvm-10.0.0.src.tar.xz
    else
        checkLib "llvm-10"
        checkLib "lldb-10"
        checkLib "llvm-10-dev"
        checkLib "llvm-10-runtime"
        checkLib "binutils"
        sudo curl https://dlang.org/install.sh | bash -s
        checkLib "lld"
        dub build
        cd rt
        bash build_all.sh
        cd ../
        clear
        echo "Done!"
    fi
fi
