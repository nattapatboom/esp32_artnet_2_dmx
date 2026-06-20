# build_ota.ps1 - Gen firmware.bin for OTA update
# Usage: .\scripts\build_ota.ps1 [-Version "1.3.0"]
# Output: dist\firmware_vX.X.X.bin  (ready to upload via web OTA page)

param(
    [string]$Version = ""
)

$ProjectRoot = Split-Path -Parent $PSScriptRoot
$PioExe = "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe"
$BinSource = "$ProjectRoot\.pio\build\wt32-eth01\firmware.bin"
$DistDir = "$ProjectRoot\dist"

# --- Validate PlatformIO ---
if (-not (Test-Path $PioExe)) {
    Write-Error "pio.exe not found at: $PioExe"
    exit 1
}

# --- Build ---
Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  Art-Net Node -- OTA Firmware Builder" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "[1/3] Cleaning previous build..." -ForegroundColor Yellow

Push-Location $ProjectRoot
& $PioExe run -t clean
$CleanResult = $LASTEXITCODE
if ($CleanResult -ne 0) {
    Pop-Location
    Write-Host ""
    Write-Error "Clean FAILED. Fix errors before generating OTA binary."
    exit 1
}

Write-Host "[1/3] Building firmware..." -ForegroundColor Yellow
& $PioExe run
$BuildResult = $LASTEXITCODE
Pop-Location

if ($BuildResult -ne 0) {
    Write-Host ""
    Write-Error "Build FAILED. Fix errors before generating OTA binary."
    exit 1
}

Write-Host "[1/3] Build SUCCESS" -ForegroundColor Green

# --- Determine output filename ---
if ($Version -eq "") {
    $Timestamp = Get-Date -Format "yyyyMMdd_HHmm"
    $OutName = "firmware_$Timestamp.bin"
} else {
    $OutName = "firmware_v${Version}.bin"
}

$BinDest = "$DistDir\$OutName"

# --- Copy to dist/ ---
Write-Host "[2/3] Copying binary to dist/..." -ForegroundColor Yellow

if (-not (Test-Path $DistDir)) {
    New-Item -ItemType Directory -Path $DistDir | Out-Null
}

Copy-Item -Path $BinSource -Destination $BinDest -Force

if (-not (Test-Path $BinDest)) {
    Write-Error "Copy failed. File not found: $BinDest"
    exit 1
}

$FileSizeBytes = (Get-Item $BinDest).Length
$SizeKB = [math]::Round($FileSizeBytes / 1024, 1)
$SizeStr = "$SizeKB KB"
Write-Host "[2/3] Copied: $OutName ($SizeStr)" -ForegroundColor Green

# --- Summary ---
Write-Host ""
Write-Host "[3/3] Done." -ForegroundColor Yellow
Write-Host ""
Write-Host "  OTA File : $BinDest" -ForegroundColor White
Write-Host "  Size     : $SizeStr" -ForegroundColor White
Write-Host ""
Write-Host "Upload via web UI:" -ForegroundColor Cyan
Write-Host "  http://artnet.local  ->  Update tab  ->  Select .bin  ->  Flash" -ForegroundColor Cyan
Write-Host ""
Write-Host "NOTE: NVS config + LittleFS data (outputs.json, wiz_bulbs.json)" -ForegroundColor DarkGray
Write-Host "      are stored in separate flash partitions and are NOT erased" -ForegroundColor DarkGray
Write-Host "      by OTA updates. All settings will be preserved." -ForegroundColor DarkGray
Write-Host ""
