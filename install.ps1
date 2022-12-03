$webclient = New-Object System.Net.WebClient
$file = "$pwd\llvm_installer.exe"

if ((gwmi win32_operatingsystem | select osarchitecture).osarchitecture -eq "64-bit")
{
$webclient.DownloadFile("https://github.com/llvm/llvm-project/releases/download/llvmorg-10.0.0/LLVM-10.0.0-win64.exe",$file)
}
else {
$webclient.DownloadFile("https://github.com/llvm/llvm-project/releases/download/llvmorg-10.0.0/LLVM-10.0.0-win32.exe",$file)
}

.\$pwd\llvm_installer.exe

$file = "$pwd\dlang_installer.exe"

$webclient.DownloadFile("https://s3.us-west-2.amazonaws.com/downloads.dlang.org/releases/2022/dmd-2.100.0.exe",$file)

.\$pwd\dlang_installer.exe

echo "Done! Write 'dub build' in folder with Rave."

Start-Sleep -Seconds 3