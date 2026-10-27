$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\Import-Toolchain.ps1"
$root = Split-Path $PSScriptRoot -Parent
& $CMake -S $root -B "$root\build-tests" -G Ninja "-DCMAKE_MAKE_PROGRAM=$Ninja" -DCMAKE_BUILD_TYPE=Release -DCMAKE_TRY_COMPILE_CONFIGURATION=Release -DBOWSPEED_BUILD_PLUGIN=OFF
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $CMake --build "$root\build-tests"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $CTest --test-dir "$root\build-tests" --output-on-failure
exit $LASTEXITCODE
