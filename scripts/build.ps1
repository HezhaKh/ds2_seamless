# Build a repo-local package. Requires Windows x64 and VS 2022 C++ tools.
# .\scripts\build.ps1 [-Config Debug] [-Clean]
# .\scripts\build.ps1 -StageToGame -GameDir 'D:\Steam\...\Game'
[CmdletBinding()]
param(
    [ValidateSet('Debug','Release','RelWithDebInfo')]
    [string]$Config = 'Release',
    [switch]$Clean,
    [switch]$StageToGame,
    [string]$GameDir = ''
)

$ErrorActionPreference = 'Stop'
if ($env:OS -ne 'Windows_NT') { throw 'ds2sc requires Windows x64.' }
if (-not [Environment]::Is64BitOperatingSystem -or $env:PROCESSOR_ARCHITECTURE -eq 'ARM64' -or $env:PROCESSOR_ARCHITEW6432 -eq 'ARM64') {
    throw 'ds2sc requires Windows x64 (not Win32 or ARM64).'
}
if ($StageToGame) {
    if (-not $GameDir -or -not (Test-Path -LiteralPath (Join-Path $GameDir 'DarkSoulsII.exe') -PathType Leaf)) {
        throw '-StageToGame requires -GameDir containing DarkSoulsII.exe.'
    }
    $GameDir = (Resolve-Path -LiteralPath $GameDir).Path
} elseif ($GameDir) {
    throw 'Use -StageToGame with -GameDir to enable game staging.'
}

$root = Split-Path -Parent $PSScriptRoot
$build = Join-Path $root 'build'
if ($Clean -and (Test-Path -LiteralPath $build)) {
    Write-Host "Cleaning: $build"
    Remove-Item -LiteralPath $build -Recurse -Force
}

$cmakeCommand = Get-Command cmake -ErrorAction SilentlyContinue
$cmake = if ($cmakeCommand) { $cmakeCommand.Source } else { $null }
if (-not $cmake) {
    $vsCmake = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
    if (Test-Path -LiteralPath $vsCmake) { $cmake = $vsCmake }
}
if (-not $cmake) { throw 'CMake not found. Install CMake 3.20+ or VS 2022 C++ CMake tools.' }

Write-Host "CMake: $cmake"
# Always configure: cached opt-in staging must not persist on a default invocation.
$stage = if ($StageToGame) { 'ON' } else { 'OFF' }
& $cmake -S $root -B $build -G 'Visual Studio 17 2022' -A x64 "-DDS2SC_STAGE_TO_GAME=$stage" "-DDS2SC_GAME_DIR=$GameDir"
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed (exit $LASTEXITCODE)." }
& $cmake --build $build --config $Config
if ($LASTEXITCODE -ne 0) { throw "CMake build failed (exit $LASTEXITCODE)." }

$package = Join-Path $build "package\$Config"
Write-Host "Build complete. Package: $package"
Write-Host '  ds2sc_launcher.exe'
Write-Host '  SeamlessCoop\ds2sc.dll'
Write-Host '  SeamlessCoop\ds2sc_settings.ini'
if ($StageToGame) { Write-Host "Also staged to game: $GameDir" }
