param([string]$MatchedInclude = '')
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\Import-Toolchain.ps1"
$root = Split-Path $PSScriptRoot -Parent
if (!$MatchedInclude) { $MatchedInclude = Join-Path $root 'include' }
$MatchedInclude = (Resolve-Path -LiteralPath $MatchedInclude).Path
$output = Join-Path $root 'build-tests-physics-catalog'
New-Item -ItemType Directory -Path $output -Force | Out-Null
Push-Location $output
try {
    & cl.exe /nologo /std:c++20 /EHsc /W4 /WX "/I$root\include" "/I$MatchedInclude" "/I$root\tests" "$root\tests\PhysicsCatalogTests.cpp" "$root\src\Config.cpp" "$root\src\PhysicsCatalog.cpp" "/Fe:$output\PhysicsCatalogTests.exe"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & "$output\PhysicsCatalogTests.exe" "$root\config\BowSpeedVR-pairing.ini"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & python "$root\scripts\generate-physics-catalog.py" --check
    exit $LASTEXITCODE
} finally {
    Pop-Location
}
