@echo off
rem Stay in this cmd session:
rem   call scripts\activate.bat
rem Leave with: deactivate

set "ROOT=%~dp0.."
for %%I in ("%ROOT%") do set "ROOT=%%~fI"

if defined SERE_ACTIVE (
  echo Already in %SERE_PROJECT_NAME%.
  exit /b 0
)

set "SERE_OLD_PATH=%PATH%"
set "SERE_OLD_PROMPT=%PROMPT%"
set "SERE_ACTIVE=1"
set "SERE_PROJECT_ROOT=%ROOT%"
for %%I in ("%ROOT%") do set "SERE_PROJECT_NAME=%%~nxI"
set "SERE_VENV_BIN=%ROOT%\venv\bin"
if exist "%ROOT%\venv\stdlib\prelude.sere" set "SERE_STDLIB=%ROOT%\venv\stdlib"
if exist "%ROOT%\venv\bin\sere.exe" set "PATH=%ROOT%\venv\bin;%PATH%"
if exist "%ROOT%\venv\sere.cfg" (
  for /f "tokens=1,* delims==" %%A in ('findstr /b /c:"home" "%ROOT%\venv\sere.cfg"') do (
    set "SERE_HOME=%%~B"
  )
)
if defined SERE_HOME (
  set "SERE_HOME=%SERE_HOME: =%"
  set "PATH=%SERE_HOME%;%PATH%"
)
cd /d "%ROOT%"
prompt (sere:%SERE_PROJECT_NAME%) $P$G
doskey deactivate=set "PATH=%SERE_OLD_PATH%" $T prompt %SERE_OLD_PROMPT% $T set "SERE_ACTIVE=" $T echo Sere project deactivated.
echo Sere project: %SERE_PROJECT_NAME%
echo   commands  sere build ^| sere run ^| sere clean ^| deactivate
echo   note      use "call scripts\activate.bat" so PATH stays in this window
