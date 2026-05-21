# Runs Ghidra headless on DarkSoulsII.exe and executes the m2_recon post-script.
#
# Resolves all paths to 8.3 short form before invoking Ghidra's analyzeHeadless.bat,
# which has a parser bug when paths contain "(x86)" (the parens close the for-in-do
# block prematurely).
#
# Idempotent: first run imports + auto-analyzes; later runs reuse the project.

[CmdletBinding()]
param(
    [switch]$Reanalyze
)

$ErrorActionPreference = 'Stop'

$JdkHome    = 'C:\Program Files\Eclipse Adoptium\jdk-21.0.11.10-hotspot'
$GhidraDir  = "$env:USERPROFILE\tools\ghidra\ghidra_12.1_PUBLIC"
$ProjDir    = "$env:USERPROFILE\ds2sc-ghidra"
$ProjName   = 'ds2sc'
$Ds2Exe     = 'C:\Users\h\ds2-game\DarkSoulsII.exe'   # junction to <Steam install>\Game\
$RepoRoot   = Split-Path -Parent $PSScriptRoot
$ScriptDir  = Join-Path $RepoRoot 'ghidra-scripts'
$OutDir     = Join-Path $RepoRoot 'recon'

if (-not (Test-Path $JdkHome))                         { throw "JDK 21 missing: $JdkHome" }
if (-not (Test-Path "$GhidraDir\support\analyzeHeadless.bat")) { throw "Ghidra missing: $GhidraDir" }
if (-not (Test-Path $Ds2Exe))                          { throw "DS2 exe missing: $Ds2Exe" }
if (-not (Test-Path $ScriptDir))                       { throw "Script dir missing: $ScriptDir" }

if (-not (Test-Path $ProjDir)) { New-Item -ItemType Directory -Path $ProjDir | Out-Null }
if (-not (Test-Path $OutDir))  { New-Item -ItemType Directory -Path $OutDir  | Out-Null }

# Resolve to 8.3 short paths to dodge Ghidra's "(x86)" parser bug.
$fso = New-Object -ComObject Scripting.FileSystemObject
function ShortFile([string]$p) { $fso.GetFile($p).ShortPath }
function ShortDir([string]$p)  { $fso.GetFolder($p).ShortPath }

$ScriptDirS = ShortDir  $ScriptDir
$OutDirS    = ShortDir  $OutDir
$ProjDirS   = ShortDir  $ProjDir
$Analyze    = ShortFile (Join-Path $GhidraDir 'support\analyzeHeadless.bat')

# We import the DS2 exe through a paren-free junction so Ghidra stores it
# under its real name (and dodges the analyzeHeadless.bat (x86) parser bug).
$Ds2BaseS   = Split-Path -Leaf $Ds2Exe

$env:JAVA_HOME = $JdkHome

$projExists = Test-Path (Join-Path $ProjDir "$ProjName.gpr")
if ($Reanalyze -or -not $projExists) {
    if ($projExists) {
        Write-Host "Re-importing into existing project (delete $ProjDir manually if you want a fresh project)." -ForegroundColor Yellow
    } else {
        Write-Host "First-time import + analysis. This will take a while (~28MB binary; expect 15-60 min)." -ForegroundColor Cyan
    }
    & cmd /c "`"$Analyze`" `"$ProjDirS`" $ProjName -import `"$Ds2Exe`" -overwrite -scriptPath `"$ScriptDirS`" -postScript m2_recon.py `"$OutDirS`""
} else {
    Write-Host "Project exists; re-running post-script only." -ForegroundColor Cyan
    & cmd /c "`"$Analyze`" `"$ProjDirS`" $ProjName -process $Ds2BaseS -scriptPath `"$ScriptDirS`" -postScript m2_recon.py `"$OutDirS`" -noanalysis"
}

if ($LASTEXITCODE -ne 0) { throw "Ghidra exited $LASTEXITCODE" }
Write-Host "Done. Recon files in $OutDir" -ForegroundColor Green
Get-ChildItem $OutDir | Format-Table Name, Length -AutoSize
