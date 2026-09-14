param([string]$SdkRoot = $env:AE_SDK_ROOT)
$ErrorActionPreference = 'Stop'
if (-not $SdkRoot) { throw 'Set AE_SDK_ROOT to the SDK folder containing Examples.' }
$root = Split-Path $PSScriptRoot -Parent
$sdk = (Resolve-Path $SdkRoot).Path
if (-not (Test-Path "$sdk/Examples/Headers/AE_Effect.h")) { throw 'Invalid AE_SDK_ROOT' }
# Run from an x64 Visual Studio 2022 developer shell (cl.exe and rc.exe on PATH).
$build = Join-Path $root 'build/windows-x64'
$dist = Join-Path $root 'dist'
New-Item -ItemType Directory -Force $build,$dist | Out-Null
$inc = @('Headers','Headers/SP','Headers/Win','Util','Resources') | ForEach-Object { "/I$sdk/Examples/$_" }
function Check-Exit { if ($LASTEXITCODE -ne 0) { throw "Build command failed: $LASTEXITCODE" } }
Push-Location $build
try {
    & cl.exe /nologo /TP /P /DMSWindows /DWIN32 /DNOMINMAX @inc "/Fi$build/Grain32.rr" "$root/resources/Grain32PiPL.r"
    Check-Exit
    & "$sdk/Examples/Resources/PiPLtool.exe" "$build/Grain32.rr" "$build/Grain32.rrc"
    Check-Exit
    & cl.exe /nologo /TP /P /DMSWindows @inc "/Fi$build/Grain32.rc" "$build/Grain32.rrc"
    Check-Exit
    & rc.exe /nologo "/fo$build/Grain32.res" "$build/Grain32.rc"
    Check-Exit
    & cl.exe /nologo /std:c++17 /EHsc /O2 /MT /LD /DMSWindows /DWIN32 /DNOMINMAX @inc "$root/src/Grain32.cpp" "$root/src/Grain32_UI.cpp" "$build/Grain32.res" /link "/OUT:$dist/Grain32.aex" /MACHINE:X64
    Check-Exit
    & cl.exe /nologo /std:c++17 /EHsc /O2 /MT "$root/tests/grain_core_tests.cpp" "/Fe:$build/grain_core_tests.exe"
    Check-Exit
    & "$build/grain_core_tests.exe"
    Check-Exit
    & dumpbin.exe /exports "$dist/Grain32.aex"
    Check-Exit
} finally { Pop-Location }
