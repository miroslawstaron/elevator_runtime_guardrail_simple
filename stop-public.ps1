$ErrorActionPreference = 'Stop'
$sessionPath = Join-Path $PSScriptRoot 'build\public\session.json'
if (-not (Test-Path -LiteralPath $sessionPath)) { Write-Host 'No public session is recorded.'; return }
$session = Get-Content -LiteralPath $sessionPath -Raw | ConvertFrom-Json
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
Remove-Item -LiteralPath $sessionPath
Write-Host 'Public tunnel and its C server stopped.'
