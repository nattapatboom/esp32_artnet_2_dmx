@echo off
setlocal EnableExtensions EnableDelayedExpansion

title WT32-ETH01 OTA Firmware Flash by IP

set "DEFAULT_IP=192.168.1.93"
set "DEFAULT_BIN="
set "DEVICE_FILE=test_device_ip.txt"

if exist "%DEVICE_FILE%" (
  for /f "usebackq tokens=1,* delims==" %%A in ("%DEVICE_FILE%") do (
    set "CFG_KEY=%%A"
    set "CFG_VAL=%%B"
    if not defined CFG_VAL (
      if "!CFG_KEY:~0,1!" NEQ "#" if not defined FILE_LEGACY_IP set "FILE_LEGACY_IP=!CFG_KEY!"
    ) else (
      if /I "!CFG_KEY!"=="IP" set "DEFAULT_IP=!CFG_VAL!"
      if /I "!CFG_KEY!"=="DEVICE_IP" set "DEFAULT_IP=!CFG_VAL!"
    )
  )
  if defined FILE_LEGACY_IP set "DEFAULT_IP=!FILE_LEGACY_IP!"
)

if exist "dist" (
  for /f "delims=" %%F in ('dir /b /a:-d /o:-d "dist\firmware_*.bin" 2^>nul') do (
    if not defined DEFAULT_BIN set "DEFAULT_BIN=dist\%%F"
  )
)

echo.
echo WT32-ETH01 OTA Firmware Flash by IP
echo -----------------------------------
echo This uploads an application firmware .bin to the controller web OTA endpoint.
echo Endpoint: http://DEVICE_IP/update
echo.

where curl.exe >nul 2>nul
if errorlevel 1 (
  echo ERROR: curl.exe was not found.
  echo Windows 10/11 normally includes curl. If missing, install curl or use the web Update page.
  echo.
  pause
  exit /b 1
)

set "DEVICE_IP=%~1"
if not defined DEVICE_IP (
  set /p "DEVICE_IP=Controller IP [Enter = %DEFAULT_IP%]: "
  if not defined DEVICE_IP set "DEVICE_IP=%DEFAULT_IP%"
)

set "DEVICE_IP=%DEVICE_IP:"=%"
set "DEVICE_IP=%DEVICE_IP:http://=%"
set "DEVICE_IP=%DEVICE_IP:https://=%"
for /f "tokens=1 delims=/" %%A in ("%DEVICE_IP%") do set "DEVICE_IP=%%A"

set "BIN_FILE=%~2"
if not defined BIN_FILE (
  if defined DEFAULT_BIN (
    echo.
    echo Latest firmware found:
    echo   %DEFAULT_BIN%
    echo.
    set /p "BIN_FILE=Firmware bin path [Enter = latest]: "
    if not defined BIN_FILE set "BIN_FILE=%DEFAULT_BIN%"
  ) else (
    echo.
    set /p "BIN_FILE=Firmware bin path: "
  )
)

set "BIN_FILE=%BIN_FILE:"=%"
if not exist "%BIN_FILE%" (
  echo.
  echo ERROR: Firmware file not found:
  echo   %BIN_FILE%
  echo.
  pause
  exit /b 1
)

echo.
echo Controller : http://%DEVICE_IP%/
echo Firmware   : %BIN_FILE%
echo.
echo Make sure the controller web page is reachable before continuing.
echo The controller will reboot automatically after a successful upload.
echo.
pause

echo.
echo Uploading firmware...
curl.exe --fail --show-error --progress-bar ^
  -F "update=@%BIN_FILE%" ^
  "http://%DEVICE_IP%/update"
set "ERR=%ERRORLEVEL%"

echo.
if "%ERR%"=="0" (
  echo.
  echo OTA upload accepted. Controller should be rebooting now.
  echo Wait about 10-15 seconds, then open:
  echo   http://%DEVICE_IP%/
) else (
  echo.
  echo OTA upload failed with exit code %ERR%.
  echo Check the IP address, network connection, and that the selected .bin is a firmware image.
)
echo.
pause
exit /b %ERR%
