@echo off
rem Puts the Sere compiler on PATH in this cmd session.
rem   sere-path.cmd
rem   sere-path.cmd -Persistent
rem   sere-path.cmd -Remove

set "HERE=%~dp0"
if "%HERE:~-1%"=="\" set "HERE=%HERE:~0,-1%"

set "SERE_BIN="
if exist "%HERE%\sere.exe" set "SERE_BIN=%HERE%"
if not defined SERE_BIN if exist "%HERE%\sere" set "SERE_BIN=%HERE%"
if not defined SERE_BIN if exist "%HERE%\..\bin\sere.exe" for %%I in ("%HERE%\..\bin") do set "SERE_BIN=%%~fI"
if not defined SERE_BIN if exist "%HERE%\..\venv\bin\sere.exe" for %%I in ("%HERE%\..\venv\bin") do set "SERE_BIN=%%~fI"
if not defined SERE_BIN if exist "%HERE%\..\build\windows-clang-cl-relwithdebinfo\bin\sere.exe" (
  for %%I in ("%HERE%\..\build\windows-clang-cl-relwithdebinfo\bin") do set "SERE_BIN=%%~fI"
)

if not defined SERE_BIN (
  echo sere.exe not found. Build the compiler or run this from the folder that contains it.
  exit /b 1
)

if /I "%~1"=="-Remove" goto :remove

set "PATH=%SERE_BIN%;%PATH%"
echo This session PATH starts with:
echo   %SERE_BIN%
if exist "%SERE_BIN%\sere.exe" (echo sere -^> %SERE_BIN%\sere.exe) else (echo sere -^> %SERE_BIN%\sere)
echo Try:  sere --help

if /I "%~1"=="-Persistent" (
  powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%HERE%\sere-path.ps1" -Persistent
)
exit /b 0

:remove
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%HERE%\sere-path.ps1" -Remove %*
exit /b %ERRORLEVEL%
