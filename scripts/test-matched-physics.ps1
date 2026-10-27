$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\Import-Toolchain.ps1"
$root = Split-Path $PSScriptRoot -Parent
$out = Join-Path $root 'build-matched-physics'
New-Item -ItemType Directory -Path $out -Force | Out-Null
Push-Location $out
try {
    & cl.exe /nologo /std:c++latest /EHsc /W4 /WX /O2 "/I$root\include" "$root\src\MatchedPhysics.cpp" "$root\tests\MatchedPhysicsTests.cpp" /Fe:matched-physics-tests.exe
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & (Join-Path $out 'matched-physics-tests.exe')
    exit $LASTEXITCODE
} finally { Pop-Location }
