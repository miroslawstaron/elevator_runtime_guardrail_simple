param([ValidateRange(1024, 65535)][int]$Port = 8099, [switch]$OpenBrowser)
$ErrorActionPreference = 'Stop'
Push-Location $PSScriptRoot
try {
    & .\build.ps1
    if ($OpenBrowser) { Start-Process "http://localhost:$Port" }
    & .\build\elevator_web.exe --port $Port --web-root "$PSScriptRoot\web"
    if ($LASTEXITCODE -ne 0) { throw "Web server exited with code $LASTEXITCODE." }
} finally { Pop-Location }
