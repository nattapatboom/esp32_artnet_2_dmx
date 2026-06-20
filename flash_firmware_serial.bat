@echo off
setlocal EnableExtensions EnableDelayedExpansion

title WT32-ETH01 Serial Firmware Flash

set "PIO_PY=%USERPROFILE%\.platformio\penv\Scripts\python.exe"
set "ESPTOOL=%USERPROFILE%\.platformio\packages\tool-esptoolpy\esptool.py"
set "DEFAULT_PORT=COM12"
set "DEFAULT_BAUD=460800"
set "DEFAULT_BIN="
set "DEVICE_FILE=test_device_ip.txt"

if exist "%DEVICE_FILE%" (
  for /f "usebackq tokens=1,* delims==" %%A in ("%DEVICE_FILE%") do (
    set "CFG_KEY=%%A"
    set "CFG_VAL=%%B"
    if defined CFG_VAL (
      if /I "!CFG_KEY!"=="PORT" set "DEFAULT_PORT=!CFG_VAL!"
      if /I "!CFG_KEY!"=="COM" set "DEFAULT_PORT=!CFG_VAL!"
      if /I "!CFG_KEY!"=="SERIAL_PORT" set "DEFAULT_PORT=!CFG_VAL!"
    )
  )
)

if exist "dist" (
  for /f "delims=" %%F in ('dir /b /a:-d /o:-d "dist\firmware_*.bin" 2^>nul') do (
    if not defined DEFAULT_BIN set "DEFAULT_BIN=dist\%%F"
  )
)

echo.
echo WT32-ETH01 Serial Firmware Flash
echo --------------------------------
echo This flashes an application firmware .bin to address 0x10000.
echo Use this with firmware files from the dist folder.
echo.

if not exist "%PIO_PY%" (
  echo ERROR: PlatformIO Python was not found:
  echo   %PIO_PY%
  echo.
  echo Install/open PlatformIO once, then run this script again.
  pause
  exit /b 1
)

if not exist "%ESPTOOL%" (
  echo ERROR: PlatformIO esptool.py was not found:
  echo   %ESPTOOL%
  echo.
  echo Run a PlatformIO build once, then run this script again.
  pause
  exit /b 1
)

set "BIN_FILE=%~1"
if not defined BIN_FILE (
  if defined DEFAULT_BIN (
    echo Latest firmware found:
    echo   %DEFAULT_BIN%
    echo.
    set /p "BIN_FILE=Firmware bin path [Enter = latest]: "
    if not defined BIN_FILE set "BIN_FILE=%DEFAULT_BIN%"
  ) else (
    set /p "BIN_FILE=Firmware bin path: "
  )
)

set "BIN_FILE=%BIN_FILE:"=%"
if not exist "%BIN_FILE%" (
  echo.
  echo ERROR: Firmware file not found:
  echo   %BIN_FILE%
  pause
  exit /b 1
)

set /p "FLASH_PORT=Serial port [Enter = %DEFAULT_PORT%]: "
if not defined FLASH_PORT set "FLASH_PORT=%DEFAULT_PORT%"

set /p "FLASH_BAUD=Baud rate [Enter = %DEFAULT_BAUD%]: "
if not defined FLASH_BAUD set "FLASH_BAUD=%DEFAULT_BAUD%"

echo.
echo Firmware : %BIN_FILE%
echo Port     : %FLASH_PORT%
echo Baud     : %FLASH_BAUD%
echo Address  : 0x10000
echo.
echo Put the WT32-ETH01 into serial bootloader mode if needed:
echo   Hold GPIO0/BOOT, press RESET, release RESET, then release GPIO0/BOOT.
echo.
pause

"%PIO_PY%" "%ESPTOOL%" --chip esp32 --port "%FLASH_PORT%" --baud "%FLASH_BAUD%" --before default_reset --after hard_reset write_flash -z 0x10000 "%BIN_FILE%"
set "ERR=%ERRORLEVEL%"

echo.
if "%ERR%"=="0" (
  echo Flash complete.
) else (
  echo Flash failed with exit code %ERR%.
  echo Try baud 115200 if 460800 is unstable.
)
echo.
pause
exit /b %ERR%
