@echo off
setlocal

cd /d "%~dp0"

set "PYTHON=%USERPROFILE%\.platformio\penv\Scripts\python.exe"
if exist "%PYTHON%" (
  "%PYTHON%" tools/build_manual.py
) else (
  where python >nul 2>nul
  if errorlevel 1 (
    echo Error: Python was not found in PlatformIO or system PATH.
    echo Please install Python or PlatformIO to run this script.
    pause
    exit /b 1
  ) else (
    python tools/build_manual.py
  )
)

if errorlevel 1 (
  echo.
  echo Compilation failed.
  pause
  exit /b 1
)

echo.
pause
