param(
    [string]$BuildDir = "$(Split-Path -Parent $PSScriptRoot)\..\build"
)

$ErrorActionPreference = "Stop"

$resolvedBuildDir = (Resolve-Path $BuildDir).Path
$stageDir = Join-Path $resolvedBuildDir "stage"
$qtExe = Get-Command windeployqt -ErrorAction Stop
$bridgeExe = Join-Path (Resolve-Path (Join-Path $resolvedBuildDir "..\..\..\target\release")).Path "lithedb-bridge.exe"

if (Test-Path $stageDir) {
    Remove-Item -Recurse -Force $stageDir
}

cmake --install $resolvedBuildDir --prefix $stageDir --config Release

$appExe = Join-Path $stageDir "bin\LitheDB.exe"
if (-not (Test-Path $appExe)) {
    $appExe = Join-Path $stageDir "LitheDB.exe"
}

if (-not (Test-Path $appExe)) {
    throw "Qt executable not found in staged bundle."
}

if (-not (Test-Path $bridgeExe)) {
    throw "Release bridge binary not found: $bridgeExe"
}

& $qtExe.Source --release --compiler-runtime --no-translations $appExe
