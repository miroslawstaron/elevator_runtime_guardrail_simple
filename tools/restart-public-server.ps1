<#
****************************************
*
* Elevator Lab -- C Server Restart with Tunnel Preservation
*
* SPDX-License-Identifier: MIT
*
****************************************
#>

param([string[]]$AllowedHost = @())
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$sessionPath = Join-Path $projectRoot 'build\public\session.json'
$session = Get-Content -LiteralPath $sessionPath -Raw | ConvertFrom-Json
# RESTART 1: Read current network policy without creating a new public tunnel
$networkSettings = Get-Content -LiteralPath (Join-Path $projectRoot 'lab-network.json') -Raw | ConvertFrom-Json
if (-not $PSBoundParameters.ContainsKey('AllowedHost')) { $AllowedHost = @($networkSettings.AllowedHost) }
# VALIDATION: Reject unsafe alias text before constructing process arguments
foreach ($hostName in $AllowedHost) {
    if ($hostName -notmatch '^[a-zA-Z0-9.-]{1,253}$') { throw "Invalid hostname: $hostName" }
}
$listenAddress = $networkSettings.ListenAddress
$listenIp = $null
if (-not [Net.IPAddress]::TryParse($listenAddress, [ref]$listenIp) -or $listenIp.AddressFamily -ne [Net.Sockets.AddressFamily]::InterNetwork) {
    throw 'Network listen address must be an IPv4 address.'
}
# RESTART 2: Carry forward the port, asset root, public origin and configured persistent database
$serverArguments = @('--port', $session.Port, '--listen-address', $listenAddress,
    '--web-root', ('"' + (Join-Path $projectRoot 'web') + '"'), '--public-origin', $session.PublicUrl)
if ($session.DatabasePath) { $serverArguments += @('--database', ('"' + $session.DatabasePath + '"')) }
foreach ($hostName in $AllowedHost) { $serverArguments += @('--allowed-host', $hostName) }

# Keep the existing tunnel and public URL; touch only the recorded C server.
$tunnel = Get-Process -Id $session.TunnelProcessId -ErrorAction Stop
if ($tunnel.Path -ne $session.TunnelExecutable -or
    $tunnel.StartTime.ToUniversalTime() -ne ([datetime]$session.TunnelStartTimeUtc).ToUniversalTime()) {
    throw 'The recorded tunnel identity does not match. No process was changed.'
}
# RESTART 3: Match the old server identity before stopping it; preserve the verified tunnel
$oldServer = Get-Process -Id $session.ServerProcessId -ErrorAction SilentlyContinue
if ($oldServer) {
    if ($oldServer.Path -ne $session.ServerExecutable -or
        $oldServer.StartTime.ToUniversalTime() -ne ([datetime]$session.ServerStartTimeUtc).ToUniversalTime()) {
        throw 'The recorded server identity does not match. No process was changed.'
    }
    Stop-Process -InputObject $oldServer
    $oldServer.WaitForExit()
}
# RESTART 4: Launch the replacement C process hidden and record its new identity immediately
$server = Start-Process -FilePath $session.ServerExecutable -ArgumentList $serverArguments -WindowStyle Hidden -WorkingDirectory $projectRoot -PassThru -RedirectStandardOutput (Join-Path $projectRoot 'build\public\server.log') -RedirectStandardError (Join-Path $projectRoot 'build\public\server.stderr.log')
$session.ServerProcessId = $server.Id
$session.ServerStartTimeUtc = $server.StartTime.ToUniversalTime().ToString('o')
$session | Add-Member -NotePropertyName AllowedHost -NotePropertyValue $AllowedHost -Force
$session | Add-Member -NotePropertyName ListenAddress -NotePropertyValue $listenAddress -Force
$session | Add-Member -NotePropertyName HostnameUrl -NotePropertyValue "http://$($AllowedHost[0]):$($session.Port)" -Force
$session | ConvertTo-Json | Set-Content -LiteralPath $sessionPath
# READINESS: Check loopback and every explicit Host alias against the restarted server
$ready = $false
for ($attempt = 0; $attempt -lt 30; $attempt++) {
    $server.Refresh()
    if ($server.HasExited) { throw 'Server exited. See build/public/server.stderr.log.' }
    try {
        $null = Invoke-RestMethod -Uri "http://127.0.0.1:$($session.Port)/api/state" -NoProxy -TimeoutSec 2
        foreach ($hostName in $AllowedHost) {
            $null = Invoke-RestMethod -Uri "http://127.0.0.1:$($session.Port)/api/state" -Headers @{ Host = "${hostName}:$($session.Port)" } -NoProxy -TimeoutSec 2
        }
        $ready = $true
        break
    } catch { Start-Sleep -Milliseconds 200 }
}
if (-not $ready) { throw 'Server started, but HTTP verification failed. See build/public/server.stderr.log.' }
foreach ($hostName in $AllowedHost) { Write-Output "Network simulator: http://${hostName}:$($session.Port)/" }
Write-Output "Public simulator: $($session.PublicUrl)"
