# Build helper for ds2sc. Run from the repo root.
#
# Usage:
#   .\scripts\build.ps1              # Release build (default)
#   .\scripts\build.ps1 -Config Debug
#   .\scripts\build.ps1 -Clean       # nuke build/ first

[CmdletBinding()]
param(
    [ValidateSet('Debug','Release','RelWithDebInfo')]
    [string]$Config = 'Release',
    [switch]$Clean
)

$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$build = Join-Path $root 'build'

if ($Clean -and (Test-Path $build)) {
    Write-Host "Cleaning $build" -ForegroundColor Yellow
    Remove-Item $build -Recurse -Force
}

# Locate cmake. Prefer PATH, fall back to VS-bundled CMake.
$cmake = (Get-Command cmake -ErrorAction SilentlyContinue).Source
if (-not $cmake) {
    $vsCmake = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
    if (Test-Path $vsCmake) { $cmake = $vsCmake }
}
if (-not $cmake) {
    Write-Error "cmake not found. Install VS Build Tools 2022 with the C++ CMake tools component."
}

Write-Host "cmake: $cmake"

if (-not (Test-Path (Join-Path $build 'CMakeCache.txt'))) {
    & $cmake -B $build -S $root -G "Visual Studio 17 2022" -A x64
    if ($LASTEXITCODE -ne 0) { throw "cmake configure failed" }
}

& $cmake --build $build --config $Config
if ($LASTEXITCODE -ne 0) { throw "cmake build failed" }

Write-Host ""
Write-Host "Build complete." -ForegroundColor Green
Write-Host "Outputs staged to game install:" -ForegroundColor Green
$gameDir = "C:\Program Files (x86)\Steam\steamapps\common\Dark Souls II Scholar of the First Sin"
Write-Host "  $gameDir\ds2sc_launcher.exe"
Write-Host "  $gameDir\SeamlessCoop\ds2sc.dll"
Write-Host "  $gameDir\SeamlessCoop\ds2sc_settings.ini"
