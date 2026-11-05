# lag-l2 + K8 L2 session, one Windows player
# (docs/object_fps_pvp/plans/v8/v8-product-hitbox-lag-l2-session.md).
#
#   powershell -ExecutionPolicy Bypass -File windows.ps1 -MacIp <MAC_IP> -Name win1 -Package <folder>
#
# Package is the folder gyo-object_fps_pvp\ from the windows-x64 tarball (it holds
# bin\ and build_metadata.json). The script prints the package's source revision, checks
# that the Gateway answers, starts the Client with its log and trace in a new
# folder, and when the Client exits zips that folder to lag-l2-<Name>.zip next to
# this script. Nothing here needs the network beyond the LAN.
param(
    [Parameter(Mandatory = $true)][string]$MacIp,
    [Parameter(Mandatory = $true)][string]$Name,
    [Parameter(Mandatory = $true)][string]$Package
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
$process = Start-Process -FilePath $client -PassThru -Wait -ArgumentList @(
    '--gateway', "${MacIp}:8080",
    '--log', ('"' + (Join-Path $out "client-$stamp.log") + '"'),
    '--movement-trace', ('"' + (Join-Path $out "client-$stamp-commands.jsonl") + '"'))
Write-Host "Client exited with $($process.ExitCode)"

$zip = Join-Path $PSScriptRoot "lag-l2-$Name.zip"
if (Test-Path $zip) { $zip = Join-Path $PSScriptRoot "lag-l2-$Name-$stamp.zip" }
Compress-Archive -Path (Join-Path $out '*') -DestinationPath $zip
Write-Host "Collected: $zip  (copy it to the Mac: SESSION\clients\$Name\)"
