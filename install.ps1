$dest = "llvm10.exe"

Write-Output "Downloading LLVM-10..."

if ((gwmi win32_operatingsystem | select osarchitecture).osarchitecture -eq "64-bit")
{
    $url = "https://github.com/llvm/llvm-project/releases/download/llvmorg-10.0.0/LLVM-10.0.0-win64.exe"
    Invoke-WebRequest -Uri $url -OutFile $dest
}
else
{
    $url = "https://github.com/llvm/llvm-project/releases/download/llvmorg-10.0.0/LLVM-10.0.0-win32.exe"
    Invoke-WebRequest -Uri $url -OutFile $dest
}

Start-Process $dest