<#
****************************************
*
* Elevator Lab -- Recorded Public Session Shutdown
*
* SPDX-License-Identifier: MIT
*
****************************************
#>

$ErrorActionPreference = 'Stop'
$sessionPath = Join-Path $PSScriptRoot 'build\public\session.json'
if (-not (Test-Path -LiteralPath $sessionPath)) { Write-Host 'No public session is recorded.'; return }
# SHUTDOWN 1: Read the recorded process identities rather than selecting processes by name
$session = Get-Content -LiteralPath $sessionPath -Raw | ConvertFrom-Json
# SHUTDOWN 2: Verify executable and creation time so recycled process IDs cannot stop unrelated work
foreach ($component in @('Server', 'Tunnel')) {
    $processId = $session."${component}ProcessId"
    $expectedExecutable = $session."${component}Executable"
    $expectedStart = ([datetime]$session."${component}StartTimeUtc").ToUniversalTime()
    $process = Get-Process -Id $processId -ErrorAction SilentlyContinue
    if (-not $process) { continue }
    if ($process.Path -ne $expectedExecutable -or $process.StartTime.ToUniversalTime() -ne $expectedStart) {
        throw "Process $processId no longer matches the recorded $component. No unrelated process will be stopped."
    }
    Stop-Process -Id $processId
}
# SHUTDOWN 3: Remove only the session record after the recorded components have been handled
Remove-Item -LiteralPath $sessionPath
Write-Host 'Public tunnel and its C server stopped.'
