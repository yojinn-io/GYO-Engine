# lag-l2 + K8 L2 session, one Windows player
# (docs/object_fps_pvp/plans/v8/v8-product-hitbox-lag-l2-session.md).
#
#   powershell -ExecutionPolicy Bypass -File windows.ps1 -MacIp <MAC_IP> -Name win1 -Package <folder> [-MovementDiag]
#
# Package is the folder gyo-object_fps_pvp\ from the windows-x64 tarball (it holds
# bin\ and build_metadata.json). The script prints the package's source revision, checks
# that the Gateway answers, starts the Client with its log and trace in a new
# folder, and when the Client exits zips that folder to lag-l2-<Name>.zip next to
# this script. Nothing here needs the network beyond the LAN.
param(
    [Parameter(Mandatory = $true)][string]$MacIp,
    [Parameter(Mandatory = $true)][string]$Name,
    [Parameter(Mandatory = $true)][string]$Package,
    # Also write the Client's movement diagnostics (v8-product-pm-detect-diag).
    [switch]$MovementDiag
)
$ErrorActionPreference = 'Stop'

$metadata = Get-Content -Raw -Path (Join-Path $Package 'build_metadata.json') | ConvertFrom-Json
Write-Host "source_revision=$($metadata.source_revision)"

try {
    $rooms = Invoke-WebRequest -UseBasicParsing -TimeoutSec 3 -Uri "http://${MacIp}:8080/rooms"
    Write-Host "Gateway answers: $($rooms.Content)"
} catch {
    Write-Host "The Gateway at ${MacIp}:8080 does not answer: $($_.Exception.Message)"
    Write-Host 'Check the address, the Mac firewall and that both are on the same network; nothing was started.'
    exit 2
}

$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$out = Join-Path $PSScriptRoot "lag-l2-$Name-$stamp"
New-Item -ItemType Directory -Path $out | Out-Null
$client = Join-Path $Package 'bin\gyo_object_fps_pvp.exe'
Write-Host "Client log and trace: $out"
# Session facts (field-session C1): this machine, the package, the settings.
$machine = Join-Path $out 'machine.txt'
try {
    $os = Get-CimInstance Win32_OperatingSystem
    $cpu = Get-CimInstance Win32_Processor | Select-Object -First 1
    $gpus = (Get-CimInstance Win32_VideoController | ForEach-Object { "$($_.Name) driver=$($_.DriverVersion)" }) -join '; '
    $ram = (Get-CimInstance Win32_ComputerSystem).TotalPhysicalMemory
    @(
        "name=$Name", "start=$(Get-Date -Format o)", "mac_ip=$MacIp",
        "source_revision=$($metadata.source_revision)", "platform=$($metadata.platform)",
        "client_sha256=$((Get-FileHash -Algorithm SHA256 $client).Hash.ToLower())",
        "os=$($os.Caption) $($os.Version)", "cpu=$($cpu.Name)", "gpu=$gpus", "ram_bytes=$ram",
        "movement_diag=$([bool]$MovementDiag)", "artificial_delay=none"
    ) | Set-Content -Path $machine -Encoding utf8
} catch {
    "machine facts unavailable: $($_.Exception.Message)" | Set-Content -Path $machine -Encoding utf8
}
$arguments = @(
    '--gateway', "${MacIp}:8080",
    '--log', ('"' + (Join-Path $out "client-$stamp.log") + '"'),
    '--movement-trace', ('"' + (Join-Path $out "client-$stamp-commands.jsonl") + '"'))
if ($MovementDiag) {
    $arguments += @('--movement-diag', ('"' + (Join-Path $out "client-$stamp-movement-diag.jsonl") + '"'))
}
# The Client's own output (an assert's text goes to standard error; field-session C7).
$process = Start-Process -FilePath $client -PassThru -ArgumentList $arguments `
    -RedirectStandardOutput (Join-Path $out 'client-stdout.txt') -RedirectStandardError (Join-Path $out 'client-stderr.txt')
# Without -Wait, ExitCode stays empty unless the handle is taken now (the
# 2026-11-08 rehearsal wrote "exit_code=").
$null = $process.Handle
# The Client's memory every 10 s while it runs (exploratory data of the session
# declaration; nothing in the Client logs it). Sampling never stops the Client.
$memory = Join-Path $out 'client-memory.csv'
'time,working_set_bytes,private_bytes' | Set-Content -Path $memory -Encoding ascii
while (-not $process.HasExited) {
    try {
        $process.Refresh()
        "$(Get-Date -Format o),$($process.WorkingSet64),$($process.PrivateMemorySize64)" |
            Add-Content -Path $memory -Encoding ascii
    } catch {}
    Start-Sleep -Seconds 10
}
$process.WaitForExit()
Write-Host "Client exited with $($process.ExitCode)"
"end=$(Get-Date -Format o)", "exit_code=$($process.ExitCode)" | Add-Content -Path $machine -Encoding utf8

$zip = Join-Path $PSScriptRoot "lag-l2-$Name.zip"
if (Test-Path $zip) { $zip = Join-Path $PSScriptRoot "lag-l2-$Name-$stamp.zip" }
Compress-Archive -Path (Join-Path $out '*') -DestinationPath $zip
Write-Host "Collected: $zip  (copy it to the Mac: SESSION\clients\$Name\)"
