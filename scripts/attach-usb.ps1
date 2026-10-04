param(
    [string]$BindOnlyBusId,
    [string]$ForceBindOnlyBusId,
    [ValidateSet("Auto", "Application", "Bootsel")]
    [string]$DeviceMode = "Auto",
    [ValidateRange(0, 60)]
    [int]$WaitSeconds = 15
)

$ErrorActionPreference = "Stop"

$usbipd = Join-Path $env:ProgramFiles "usbipd-win\usbipd.exe"
if (-not (Test-Path $usbipd)) {
    throw "usbipd-win is not installed at $usbipd"
}

if ($BindOnlyBusId -or $ForceBindOnlyBusId) {
    $targetBusId = if ($ForceBindOnlyBusId) { $ForceBindOnlyBusId } else { $BindOnlyBusId }
    $bindArguments = @("bind", "--busid", $targetBusId)
    if ($ForceBindOnlyBusId) {
        $bindArguments += "--force"
    }

    & $usbipd @bindArguments
    if ($LASTEXITCODE -ne 0) {
        throw "usbipd bind failed with exit code $LASTEXITCODE"
    }

    exit 0
}

$serialPattern = "2e8a:000a|2e8a:0003|Pico|RP2|Raspberry|USB Serial|CDC|Arduino|CH340|CP210|FTDI"
$deadline = [DateTime]::UtcNow.AddSeconds($WaitSeconds)
do {
    $devices = & $usbipd list
    if ($LASTEXITCODE -ne 0) {
        throw "usbipd list failed with exit code $LASTEXITCODE"
    }

    $connectedDevices = @($devices | Where-Object { $_ -match '^\s*(\d+-\d+)\s+' })
    $candidateDevices = @(
        switch ($DeviceMode) {
            "Application" { $connectedDevices | Where-Object { $_ -match '\b2e8a:000a\b' } }
            "Bootsel" { $connectedDevices | Where-Object { $_ -match '\b2e8a:0003\b' } }
            default {
                $picoDevices = @($connectedDevices | Where-Object { $_ -match '\b2e8a:000[3a]\b' })
                if ($picoDevices.Count -gt 0) {
                    $picoDevices
                } else {
                    $connectedDevices | Where-Object { $_ -match $serialPattern }
                }
            }
        }
    )

    if ($candidateDevices.Count -gt 0 -or [DateTime]::UtcNow -ge $deadline) {
        break
    }

    Start-Sleep -Milliseconds 250
} while ($true)

$devices | Write-Host

if ($candidateDevices.Count -eq 0) {
    $expected = switch ($DeviceMode) {
        "Application" { "Pico application device 2e8a:000a" }
        "Bootsel" { "RP2 BOOTSEL device 2e8a:0003" }
        default { "likely Pico device" }
    }
    throw "No $expected appeared within $WaitSeconds second(s)."
}

if ($candidateDevices.Count -gt 1) {
    $candidateDevices | ForEach-Object { Write-Host "Candidate: $_" }
    throw "More than one likely Pico/serial device was found; refusing to attach the wrong device."
}

$deviceLine = $candidateDevices[0]
$busId = [regex]::Match($deviceLine, '^\s*(\d+-\d+)\s+').Groups[1].Value
$isAttached = $deviceLine -match '\bAttached\b'
$isShared = $isAttached -or $deviceLine -match '(?<!Not )\bShared\b'
$isBootsel = $DeviceMode -eq "Bootsel" -or $deviceLine -match '\b2e8a:0003\b' -or $deviceLine -match '\bRP2 Boot\b'
Write-Host "Selected USB device: $deviceLine"

function Invoke-PrivilegedBind {
    param(
        [Parameter(Mandatory = $true)]
        [string]$BusId,
        [switch]$Force
    )

    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = [Security.Principal.WindowsPrincipal]::new($identity)
    $isAdministrator = $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)

    if (-not $isAdministrator) {
        $operation = if ($Force) { "recover the device binding" } else { "share the device" }
        Write-Host "Administrator rights are required to $operation; requesting elevation."
        $parameter = if ($Force) { "ForceBindOnlyBusId" } else { "BindOnlyBusId" }
        $arguments = "-NoProfile -ExecutionPolicy Bypass -File `"$PSCommandPath`" -$parameter `"$BusId`""
        $elevated = Start-Process -FilePath "powershell.exe" -Verb RunAs -ArgumentList $arguments -Wait -PassThru
        if ($elevated.ExitCode -ne 0) {
            throw "Elevated usbipd bind failed with exit code $($elevated.ExitCode)"
        }
    } else {
        $bindArguments = @("bind", "--busid", $BusId)
        if ($Force) {
            $bindArguments += "--force"
        }

        & $usbipd @bindArguments
        if ($LASTEXITCODE -ne 0) {
            throw "usbipd bind failed with exit code $LASTEXITCODE"
        }
    }
}

function Invoke-UsbipdAttach {
    param(
        [Parameter(Mandatory = $true)]
        [string]$BusId
    )

    # Windows PowerShell 5 promotes a native program's stderr to a terminating
    # NativeCommandError when ErrorActionPreference is Stop. usbipd writes its
    # normal informational messages to stderr, so temporarily allow them while
    # preserving both the output and the native exit code for the caller.
    $previousErrorActionPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = "Continue"
        $output = @(& $usbipd attach --wsl --busid $BusId 2>&1)
        $exitCode = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previousErrorActionPreference
    }

    return [PSCustomObject]@{
        Output = $output
        ExitCode = $exitCode
    }
}

function Test-WslDevice {
    if ($isBootsel) {
        & wsl.exe --exec lsusb -d 2e8a:0003 2>$null | Out-Null
    } else {
        & wsl.exe --exec test -e /dev/ttyACM0 2>$null | Out-Null
    }
    return $LASTEXITCODE -eq 0
}

function Invoke-UsbipdDetach {
    param(
        [Parameter(Mandatory = $true)]
        [string]$BusId
    )

    $previousErrorActionPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = "Continue"
        $output = @(& $usbipd detach --busid $BusId 2>&1)
        $exitCode = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previousErrorActionPreference
    }
    $output | ForEach-Object { Write-Host $_ }
    return $exitCode
}

if (-not $isShared) {
    Invoke-PrivilegedBind -BusId $busId
}

if (-not $isAttached) {
    $attachExitCode = 1
    $forceRebound = $false
    for ($attachAttempt = 0; $attachAttempt -lt 4; $attachAttempt++) {
        $attachResult = Invoke-UsbipdAttach -BusId $busId
        $attachOutput = @($attachResult.Output)
        $attachExitCode = $attachResult.ExitCode
        $attachOutput | ForEach-Object { Write-Host $_ }
        if ($attachExitCode -eq 0) {
            break
        }

        $attachMessage = $attachOutput -join "`n"
        if ($attachMessage -match "Device busy.*exported") {
            # usbipd's auto-attach can win the race but still report the old
            # Windows state. Accept it if WSL already has the expected device;
            # otherwise clear the stale export and retry the same identity.
            if (Test-WslDevice) {
                Write-Host "The device was already auto-attached to WSL."
                $attachExitCode = 0
                break
            }
            Write-Warning "Clearing a stale USB/IP export for BUSID $busId and retrying."
            [void](Invoke-UsbipdDetach -BusId $busId)
            Start-Sleep -Milliseconds 500
            continue
        }

        if ($attachMessage -match "Device in error state" -and -not $forceRebound) {
            Write-Warning "Windows reports the Pico USB/IP binding in an error state. Rebinding it with usbipd --force and retrying once."
            Invoke-PrivilegedBind -BusId $busId -Force
            $forceRebound = $true
            continue
        }

        break
    }

    if ($attachExitCode -ne 0) {
        throw "usbipd attach failed for BUSID $busId with exit code $attachExitCode"
    }
}

Write-Host "Attached BUSID $busId to WSL."
$deviceFound = $false
for ($attempt = 0; $attempt -lt 20; $attempt++) {
    if ($isBootsel) {
        & wsl.exe --exec lsusb -d 2e8a:0003
    } else {
        & wsl.exe --exec test -e /dev/ttyACM0
    }

    if ($LASTEXITCODE -eq 0) {
        $deviceFound = $true
        break
    }

    Start-Sleep -Milliseconds 250
}

if (-not $deviceFound) {
    if ($isBootsel) {
        throw "The device attached, but the RP2 BOOTSEL device did not appear in WSL."
    }

    throw "The device attached, but WSL did not expose /dev/ttyACM0. Check that the device firmware enables USB CDC serial."
}

if ($isBootsel) {
    Write-Host "RP2 BOOTSEL is ready in WSL. Reading device information..."
    & wsl.exe --user root --exec picotool info -a
    if ($LASTEXITCODE -ne 0) {
        throw "picotool info failed with exit code $LASTEXITCODE"
    }
} else {
    & wsl.exe --exec ls -l /dev/ttyACM0
}
