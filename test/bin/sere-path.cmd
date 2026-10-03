@echo off
set "HERE=%~dp0"
if "%HERE:~-1%"=="\" set "HERE=%HERE:~0,-1%"
set "SERE_BIN="
if exist "%HERE%\sere.exe" set "SERE_BIN=%HERE%"
if not defined SERE_BIN if exist "%HERE%\..\venv\bin\sere.exe" for %%I in ("%HERE%\..\venv\bin") do set "SERE_BIN=%%~fI"
if not defined SERE_BIN if exist "%HERE%\..\bin\sere.exe" for %%I in ("%HERE%\..\bin") do set "SERE_BIN=%%~fI"
if not defined SERE_BIN (
  echo sere.exe not found. Run this from a Sere project or compiler bin folder.
  exit /b 1
)
if /I "%~1"=="-Remove" (
  powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%HERE%\sere-path.ps1" -Remove %*
  exit /b %ERRORLEVEL%
)
set "PATH=%SERE_BIN%;%PATH%"
echo This session PATH starts with:
echo   %SERE_BIN%
if /I "%~1"=="-Persistent" powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%HERE%\sere-path.ps1" -Persistent
exit /b 0
