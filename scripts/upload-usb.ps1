param(
    [Parameter(Mandatory = $true)]
    [string]$FirmwarePath
)

$ErrorActionPreference = "Stop"
$attachScript = Join-Path $PSScriptRoot "attach-usb.ps1"

& wsl.exe --user root --exec test -r $FirmwarePath
if ($LASTEXITCODE -ne 0) {
    throw "The firmware image is not readable inside WSL: $FirmwarePath"
}

# Attach whichever Pico identity is present. If the application is running,
# ask its reset interface for BOOTSEL and attach the re-enumerated device.
& $attachScript -DeviceMode Auto -WaitSeconds 15
if ($LASTEXITCODE -ne 0) {
    throw "Pico USB attachment failed with exit code $LASTEXITCODE"
}

& wsl.exe --user root --exec lsusb -d 2e8a:0003
if ($LASTEXITCODE -ne 0) {
    Write-Host "Pico is in application mode; requesting a BOOTSEL reboot..."
    & wsl.exe --user root --exec picotool reboot --usb -f
    if ($LASTEXITCODE -ne 0) {
        # USB/IP drops the WSL device during re-enumeration, so picotool can
        # report that it did not rediscover BOOTSEL even though reset worked.
        Write-Host "The reset interface detached; checking Windows for BOOTSEL."
    }

    $bootselAttached = $false
    try {
        & $attachScript -DeviceMode Bootsel -WaitSeconds 15
        $bootselAttached = $true
    } catch {
        if ($_.Exception.Message -notmatch "^No RP2 BOOTSEL device") {
            throw
        }
        Write-Warning "The vendor reset did not expose BOOTSEL; trying the USB CDC 1200-baud reset once."
    }

    if (-not $bootselAttached) {
        & $attachScript -DeviceMode Application -WaitSeconds 5
        & wsl.exe --user root --exec stty -F /dev/ttyACM0 1200
        if ($LASTEXITCODE -ne 0) {
            throw "Both automatic BOOTSEL reset methods failed. Hold BOOTSEL while resetting or reconnecting the Pico, then rerun make upload."
        }

        try {
            & $attachScript -DeviceMode Bootsel -WaitSeconds 15
            $bootselAttached = $true
        } catch {
            if ($_.Exception.Message -notmatch "^No RP2 BOOTSEL device") {
                throw
            }
            throw "The Pico accepted neither automatic BOOTSEL reset method. Hold BOOTSEL while resetting or reconnecting the Pico, then rerun make upload."
        }
    }

    & wsl.exe --user root --exec lsusb -d 2e8a:0003
    if ($LASTEXITCODE -ne 0 -or -not $bootselAttached) {
        throw "BOOTSEL was detected by Windows but was not attached to WSL."
    }
}

Write-Host "Uploading $FirmwarePath with picotool..."
& wsl.exe --user root --exec picotool load -v -x $FirmwarePath
if ($LASTEXITCODE -ne 0) {
    throw "picotool upload failed with exit code $LASTEXITCODE"
}

Write-Host "Upload completed. Waiting for application USB enumeration..."
Start-Sleep -Seconds 2

& $attachScript -DeviceMode Application -WaitSeconds 15
if ($LASTEXITCODE -ne 0) {
    throw "Firmware was uploaded, but application USB reattachment failed with exit code $LASTEXITCODE"
}
