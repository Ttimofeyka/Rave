$dest = "llvm10.exe"

Write-Output "Downloading LLVM-10 Installer..."

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

Write-Output "Running LLVM-10 Installer..."

Start-Process $dest

# Next - downloading and running DMD

Write-Output "Downloading D-Installer..."

$u = "https://s3.us-west-2.amazonaws.com/downloads.dlang.org/pre-releases/2022/dmd-2.100.0-beta.1.exe"
$d = "dmd_installer.exe"
Invoke-WebRequest -Uri $u -OutFile $d

Write-Output "Running D-Installer..."

Start-Process $d # Run installer

cmd.exe /c "dub build" 
