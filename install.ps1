$webclient = New-Object System.Net.WebClient
$file = "llvm_installer.exe"

if ((gwmi win32_operatingsystem | select osarchitecture).osarchitecture -eq "64-bit")
{
    $webclient.DownloadFile("https://github.com/llvm/llvm-project/releases/download/llvmorg-11.0.0/LLVM-11.0.0-win64.exe",$file)
}
else {
    $webclient.DownloadFile("https://github.com/llvm/llvm-project/releases/download/llvmorg-11.0.0/LLVM-11.0.0-win32.exe",$file)
}

echo "Please install LLVM directly on disk C (C:/LLVM or C:/Program Files/LLVM)"

./llvm_installer.exe

$file = "dlang_installer.exe"

$webclient.DownloadFile("https://s3.us-west-2.amazonaws.com/downloads.dlang.org/releases/2022/dmd-2.100.0.exe",$file)

./dlang_installer.exe

$webclient = New-Object System.Net.WebClient
$file = "llvm_mingw_zip.zip"
$webclient.DownloadFile("https://github.com/mstorsjo/llvm-mingw/releases/download/20201020/llvm-mingw-20201020-msvcrt-x86_64.zip",$file)

Expand-Archive -Path $file -DestinationPath "./"

$Folder = 'C:/LLVM'
if (Test-Path -Path $Folder) {
    Copy-Item -Path "./llvm-mingw/x86_64-w64-mingw32" -Destination "C:/LLVM/x86_64-w64-mingw32" -Recurse -Force
    Copy-Item -Path "./llvm-mingw/i686-w64-mingw32" -Destination "C:/LLVM/i686-w64-mingw32" -Recurse -Force
    Copy-Item -Path "./llvm-mingw/aarch64-w64-mingw32" -Destination "C:/LLVM/aarch64-w64-mingw32" -Recurse -Force
    Copy-Item -Path "./llvm-mingw/armv7-w64-mingw32" -Destination "C:/LLVM/armv7-w64-mingw32" -Recurse -Force
    Copy-Item -Path "./llvm-mingw/bin" -Destination "C:/LLVM/bin" -Recurse -Force
    Copy-Item -Path "./llvm-mingw/lib" -Destination "C:/LLVM/xlib" -Recurse -Force
} else {
    $Folder = 'C:/Program Files/LLVM'
    if (Test-Path -Path $Folder) {
        Copy-Item -Path "./llvm-mingw/x86_64-w64-mingw32" -Destination "C:/Program Files/LLVM/x86_64-w64-mingw32" -Recurse -Force
        Copy-Item -Path "./llvm-mingw/i686-w64-mingw32" -Destination "C:/Program Files/LLVM/i686-w64-mingw32" -Recurse -Force
        Copy-Item -Path "./llvm-mingw/aarch64-w64-mingw32" -Destination "C:/Program Files/LLVM/aarch64-w64-mingw32" -Recurse -Force
        Copy-Item -Path "./llvm-mingw/armv7-w64-mingw32" -Destination "C:/Program Files/LLVM/armv7-w64-mingw32" -Recurse -Force
        Copy-Item -Path "./llvm-mingw/bin" -Destination "C:/Program Files/LLVM/bin" -Recurse -Force
        Copy-Item -Path "./llvm-mingw/lib" -Destination "C:/Program Files/LLVM/lib" -Recurse -Force
    }
    else {
        echo "Undefined LLVM path (not C:/LLVM or C:/Program Files/LLVM)"
    }
}

Remove-Item -Path "./llvm-mingw_zip.zip" -Force
Remove-Item -Path "./llvm-mingw" -Force -Recurse
Remove-Item -Path "./llvm_installer.exe" -Force
Remove-Item -Path "./dlang_installer.exe" -Force

Remove-Item -Path "./options.json" -Force
New-Item "./options.json"
Set-Content "./options.json" '{
    "compiler": "clang"
}

'

echo "Done! Write 'dub build' in folder with Rave."

Start-Sleep -Seconds 3