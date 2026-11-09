# Configure and build the PC port (32-bit, clang, Ninja) from a plain PowerShell window.
#   .\port\build.ps1            build
#   .\port\build.ps1 -Clean     delete port/build first
param([switch]$Clean, [string]$Target = "")

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$build = Join-Path $PSScriptRoot "build"
if ($Clean -and (Test-Path $build)) { Remove-Item -Recurse -Force $build }

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vswhere -products * -latest -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw "Visual Studio 2022 Build Tools with the C++ workload not found" }
$vcvars = Join-Path $vs "VC\Auxiliary\Build\vcvarsall.bat"

$llvm = "C:\Program Files\LLVM\bin"
$cmake = (Get-Command cmake -ErrorAction SilentlyContinue).Source
if (-not $cmake) { $cmake = "C:\Program Files\CMake\bin\cmake.exe" }
$ninja = (Get-Command ninja -ErrorAction SilentlyContinue).Source
if (-not $ninja) { $ninja = Join-Path $env:LOCALAPPDATA "Microsoft\WinGet\Links\ninja.exe" }

$tgt = if ($Target) { "--target $Target" } else { "" }
$cmd = "`"$vcvars`" x86 >nul && `"$cmake`" -S `"$PSScriptRoot`" -B `"$build`" -G Ninja " +
       "`"-DCMAKE_MAKE_PROGRAM=$ninja`" `"-DCMAKE_C_COMPILER=$llvm\clang.exe`" `"-DCMAKE_CXX_COMPILER=$llvm\clang++.exe`" " +
       "&& `"$cmake`" --build `"$build`" $tgt -- -k 0"
cmd /c "set `"PATH=%PATH%;${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer`" && $cmd"
exit $LASTEXITCODE
