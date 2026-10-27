param(
    [Parameter(Mandatory=$true)][string]$OutputDirectory,
    [Parameter(Mandatory=$true)][string]$DependencySources
)
$ErrorActionPreference='Stop'
& python "$PSScriptRoot/release.py" --output $OutputDirectory --dependency-sources $DependencySources
exit $LASTEXITCODE
