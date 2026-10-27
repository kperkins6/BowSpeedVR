param(
    [Parameter(Mandatory=$true)][string]$CommonLib,
    [Parameter(Mandatory=$true)][string]$Vcpkg,
    [Parameter(Mandatory=$true)][string]$Dependencies,
    [int]$Jobs=8
)
$ErrorActionPreference='Stop'
. "$PSScriptRoot\Import-Toolchain.ps1"
$root=Split-Path $PSScriptRoot -Parent
$pin='39f9d07a6ffabea8fb559eee87ab7d27cd463e8a'
$actual=& git -C $CommonLib rev-parse HEAD
if ($LASTEXITCODE -ne 0 -or $actual -ne $pin) {throw "CommonLib must be pinned to $pin"}
$openvr=& git -C "$CommonLib\extern\openvr" rev-parse HEAD
if ($LASTEXITCODE -ne 0 -or $openvr -ne '60eb187801956ad277f1cae6680e3a410ee0873b') {throw 'OpenVR submodule must match the pinned CommonLib checkout'}
$Dependencies=[IO.Path]::GetFullPath($Dependencies)
New-Item -ItemType Directory -Force "$Dependencies\downloads","$Dependencies\registries" | Out-Null
$env:VCPKG_ROOT=[IO.Path]::GetFullPath($Vcpkg)
$env:VCPKG_DOWNLOADS="$Dependencies\downloads"
$env:X_VCPKG_REGISTRIES_CACHE="$Dependencies\registries"
$env:VCPKG_BINARY_SOURCES='clear'
$env:GIT_CONFIG_COUNT='1'
$env:GIT_CONFIG_KEY_0='http.sslBackend'
$env:GIT_CONFIG_VALUE_0='openssl'
# For restricted Windows environments, optionally set X_VCPKG_ASSET_SOURCES
# to the HTTPS+SHA512 Python adapter in scripts/asset-download.py.
& $CMake -S $root -B "$root\build-release" -G Ninja "-DCMAKE_MAKE_PROGRAM=$Ninja" -DCMAKE_BUILD_TYPE=Release -DCMAKE_TRY_COMPILE_CONFIGURATION=Release -DBOWSPEED_BUILD_PLUGIN=ON "-DCOMMONLIB_SOURCE_DIR=$CommonLib" "-DCMAKE_TOOLCHAIN_FILE=$Vcpkg\scripts\buildsystems\vcpkg.cmake" -DVCPKG_TARGET_TRIPLET=x64-windows-static-md "-DVCPKG_INSTALLED_DIR=$Dependencies\installed"
if ($LASTEXITCODE -ne 0) {exit $LASTEXITCODE}
& $CMake --build "$root\build-release" --parallel $Jobs
if ($LASTEXITCODE -ne 0) {exit $LASTEXITCODE}
& $CTest --test-dir "$root\build-release" --output-on-failure
exit $LASTEXITCODE
