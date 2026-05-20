@echo off
REM Runs Ghidra headless on DarkSoulsII.exe and executes the M2-recon post-script.
REM Idempotent: re-imports only if the project doesn't already contain the program.

setlocal

set "JAVA_HOME=C:\Program Files\Eclipse Adoptium\jdk-21.0.11.10-hotspot"
set "GHIDRA_DIR=%USERPROFILE%\tools\ghidra\ghidra_12.1_PUBLIC"
set "PROJ_DIR=%USERPROFILE%\ds2sc-ghidra"
set "PROJ_NAME=ds2sc"
set "DS2_EXE=C:\Program Files (x86)\Steam\steamapps\common\Dark Souls II Scholar of the First Sin\Game\DarkSoulsII.exe"
set "SCRIPT_DIR=%~dp0..\ghidra-scripts"
set "OUT_DIR=%~dp0..\recon"

if not exist "%GHIDRA_DIR%\support\analyzeHeadless.bat" (
    echo ERROR: Ghidra not found at %GHIDRA_DIR%
    echo Extract ghidra_12.1_PUBLIC_20260513.zip into %USERPROFILE%\tools\ghidra first.
    exit /b 1
)

if not exist "%PROJ_DIR%" mkdir "%PROJ_DIR%"
if not exist "%OUT_DIR%" mkdir "%OUT_DIR%"

REM First run imports + auto-analyzes. Subsequent runs only re-execute the post-script.
if exist "%PROJ_DIR%\%PROJ_NAME%.gpr" (
    echo Project exists; re-running post-script only.
    "%GHIDRA_DIR%\support\analyzeHeadless.bat" "%PROJ_DIR%" "%PROJ_NAME%" ^
        -process DarkSoulsII.exe ^
        -scriptPath "%SCRIPT_DIR%" ^
        -postScript m2_recon.py "%OUT_DIR%" ^
        -noanalysis
) else (
    echo First-time import + analysis. This will take a while ^(DarkSoulsII.exe is ~28MB^).
    "%GHIDRA_DIR%\support\analyzeHeadless.bat" "%PROJ_DIR%" "%PROJ_NAME%" ^
        -import "%DS2_EXE%" ^
        -scriptPath "%SCRIPT_DIR%" ^
        -postScript m2_recon.py "%OUT_DIR%"
)

endlocal
