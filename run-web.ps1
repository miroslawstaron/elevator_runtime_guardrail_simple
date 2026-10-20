<#
****************************************
*
* Elevator Lab -- Local and Network Simulator Launcher
*
* SPDX-License-Identifier: MIT
*
****************************************
#>

param([ValidateRange(1024, 65535)][int]$Port = 8099, [switch]$OpenBrowser, [string]$ListenAddress = '127.0.0.1', [string[]]$AllowedHost = @())
$ErrorActionPreference = 'Stop'
# SETTINGS: Use saved network binding and aliases unless the caller supplied explicit overrides
$networkSettings = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'lab-network.json') -Raw | ConvertFrom-Json
if (-not $PSBoundParameters.ContainsKey('ListenAddress')) { $ListenAddress = $networkSettings.ListenAddress }
if (-not $PSBoundParameters.ContainsKey('AllowedHost')) { $AllowedHost = @($networkSettings.AllowedHost) }
Push-Location $PSScriptRoot
try {
    # LAUNCH 1: Build the C runtime before starting the foreground server
    & .\build.ps1
    if ($OpenBrowser) { Start-Process "http://localhost:$Port" }
    # LAUNCH 2: Pass the chosen port, bind address, asset root and hostname aliases to C
    $serverArguments = @('--port', $Port, '--listen-address', $ListenAddress, '--web-root', "$PSScriptRoot\web")
    foreach ($hostName in $AllowedHost) { $serverArguments += @('--allowed-host', $hostName) }
    & .\build\elevator_web.exe @serverArguments
    if ($LASTEXITCODE -ne 0) { throw "Web server exited with code $LASTEXITCODE." }
} finally { Pop-Location }
