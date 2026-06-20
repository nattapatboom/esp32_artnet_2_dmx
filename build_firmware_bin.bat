@echo off
setlocal

cd /d "%~dp0"

set "PIO=%USERPROFILE%\.platformio\penv\Scripts\pio.exe"
if not exist "%PIO%" (
  echo PlatformIO was not found at:
  echo   %PIO%
  echo.
  echo Please install PlatformIO or edit this BAT file to point to pio.exe.
  pause
  exit /b 1
)

set "PYTHON=%USERPROFILE%\.platformio\penv\Scripts\python.exe"
if exist "%PYTHON%" (
  echo Compiling Web UI assets - web/index.html to include/web_pages.h...
  "%PYTHON%" tools/build_web.py
  if errorlevel 1 (
    echo.
    echo Web UI compilation failed.
    pause
    exit /b 1
  )
) else (
  echo Warning: Python interpreter not found in PlatformIO penv. Skipping Web UI asset compilation.
)

echo Cleaning previous build...
"%PIO%" run -t clean
if errorlevel 1 (
  echo.
  echo Clean failed.
  pause
  exit /b 1
)

echo Building WT32-ETH01 firmware...
"%PIO%" run
if errorlevel 1 (
  echo.
  echo Build failed.
  pause
  exit /b 1
)

if not exist "dist" mkdir "dist"

for /f %%a in ('powershell -NoProfile -Command "Get-Date -Format yyyyMMdd_HHmmss"') do set "STAMP=%%a"

set "OUT=dist\firmware_%STAMP%.bin"
copy /Y ".pio\build\wt32-eth01\firmware.bin" "%OUT%" >nul
if errorlevel 1 (
  echo.
  echo Build succeeded, but copying firmware.bin failed.
  pause
  exit /b 1
)

echo.
echo Build complete.
echo Firmware BIN:
echo   %OUT%
echo.
pause
