# Pi-Gaze: put the first-boot files on a freshly flashed Raspberry Pi OS Lite (64-bit) card.
# Flash the card first (Raspberry Pi Imager, NO customisation), then run:
#   powershell -ExecutionPolicy Bypass -File software\deploy\prepare-sdcard.ps1 [-Boot E:\]
param([string]$Boot)
$ErrorActionPreference = "Stop"

if (-not $Boot) {
    $v = Get-Volume | Where-Object FileSystemLabel -eq "bootfs" | Select-Object -First 1
    if (-not $v -or -not $v.DriveLetter) { throw "No 'bootfs' partition found. Is the flashed card inserted?" }
    $Boot = "$($v.DriveLetter):\"
}
if (-not (Test-Path (Join-Path $Boot "config.txt"))) { throw "$Boot is not a Raspberry Pi boot partition (no config.txt)" }
$software = Split-Path -Parent $PSScriptRoot

# cloud-init: user 'pigaze' (SSH key only), hostname, first-boot installer
Copy-Item (Join-Path $PSScriptRoot "sdcard\user-data") (Join-Path $Boot "user-data") -Force
# network: keep a Pi-Gaze network-config the user may already have filled in
$net = Join-Path $Boot "network-config"
if (-not ((Test-Path $net) -and (Select-String -Path $net -Pattern "Pi-Gaze network" -Quiet))) {
    Copy-Item (Join-Path $PSScriptRoot "sdcard\network-config") $net -Force
}

# sources for the first-boot build (overwrites an older copy; build folders skipped)
robocopy $software (Join-Path $Boot "pigaze") /E /XD build-win build /NFL /NDL /NJH /NJS /NP | Out-Null
if ($LASTEXITCODE -ge 8) { throw "copying the sources failed (robocopy $LASTEXITCODE)" }

# UART0 on GPIO14/15 for the CH9329 from the very first boot
$cfg = Join-Path $Boot "config.txt"
if (-not (Select-String -Path $cfg -Pattern "^dtparam=uart0=on" -Quiet)) {
    [IO.File]::AppendAllText($cfg, "`n# Pi-Gaze: CH9329 on GPIO14 (TX) / GPIO15 (RX)`ndtparam=uart0=on`n")   # LF only
}

# no kernel/login console on the serial port: nothing but our frames may reach the CH9329
$cmdline = Join-Path $Boot "cmdline.txt"
$line = [IO.File]::ReadAllText($cmdline)
if ($line -match "console=serial0,115200 ") {
    [IO.File]::WriteAllText($cmdline, ($line -replace "console=serial0,115200 ", ""))   # stays one line
}

Write-Host "Pi-Gaze files are on $Boot"
Write-Host "Wi-Fi: open ${net} and replace the network name and password (or use an Ethernet cable)."
