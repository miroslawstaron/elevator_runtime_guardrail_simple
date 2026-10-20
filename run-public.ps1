param([ValidateRange(1024, 65535)][int]$Port = 8099, [switch]$OpenBrowser)
$ErrorActionPreference = 'Stop'
$publicDirectory = Join-Path $PSScriptRoot 'build\public'
$toolsDirectory = Join-Path $PSScriptRoot 'build\tools'
New-Item -ItemType Directory -Force $publicDirectory, $toolsDirectory | Out-Null
$sessionPath = Join-Path $publicDirectory 'session.json'
if (Test-Path -LiteralPath $sessionPath) {
    throw 'A public-session record exists. Run .\stop-public.ps1 before starting another public session.'
}

# Test an actual exclusive bind, including Windows reserved/occupied ports.
$portProbe = [System.Net.Sockets.TcpListener]::new([System.Net.IPAddress]::Loopback, $Port)
$portProbe.Server.ExclusiveAddressUse = $true
try { $portProbe.Start() } finally { $portProbe.Stop() }

$executable = Join-Path $publicDirectory 'elevator_web.exe'
Push-Location $PSScriptRoot
try {
    & gcc -Wall -Wextra -Werror -std=c99 -O2 -Iinclude src/elevator.c src/guardrails.c src/simulation.c src/web_server.c -o $executable -lws2_32
    if ($LASTEXITCODE -ne 0) { throw 'C web server compilation failed.' }
} finally { Pop-Location }

# Official pinned Windows x64 release. Verify the GitHub release SHA-256 before execution.
$tunnelExecutable = Join-Path $toolsDirectory 'cloudflared.exe'
$tunnelVersion = '2026.10.2'
$expectedHash = '12c7c887f8aaab779aef1dfa9b60673530a5adb88ecc55dd6df8f56e0b70f199'
if (-not (Test-Path -LiteralPath $tunnelExecutable)) {
    Invoke-WebRequest -Uri "https://github.com/cloudflare/cloudflared/releases/download/$tunnelVersion/cloudflared-windows-amd64.exe" -OutFile $tunnelExecutable
}
if ((Get-FileHash -LiteralPath $tunnelExecutable -Algorithm SHA256).Hash.ToLowerInvariant() -ne $expectedHash) {
    throw "Checksum mismatch for $tunnelExecutable. Use the official $tunnelVersion release."
}

$tunnelProcess = $null
$serverProcess = $null
try {
    $tunnelLog = Join-Path $publicDirectory 'tunnel.log'
    $tunnelProcess = Start-Process -FilePath $tunnelExecutable -ArgumentList @('tunnel', '--no-autoupdate', '--protocol', 'http2', '--url', "http://127.0.0.1:$Port") -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $publicDirectory 'tunnel.stdout.log') -RedirectStandardError $tunnelLog
    $publicUrl = $null
    for ($attempt = 0; $attempt -lt 100; $attempt++) {
        $tunnelProcess.Refresh()
        if ($tunnelProcess.HasExited) { throw "Tunnel exited. See $tunnelLog" }
        if (Test-Path -LiteralPath $tunnelLog) {
            $logText = Get-Content -LiteralPath $tunnelLog -Raw
            if ($logText -match 'https://[a-z0-9-]+\.trycloudflare\.com') { $publicUrl = $Matches[0]; break }
        }
        Start-Sleep -Milliseconds 300
    }
    if (-not $publicUrl) { throw "No public address assigned. See $tunnelLog" }
    # cloudflared preserves Host/Origin. Only this exact HTTPS origin is accepted by C.
    $serverProcess = Start-Process -FilePath $executable -ArgumentList @('--port', $Port, '--web-root', ('"' + (Join-Path $PSScriptRoot 'web') + '"'), '--public-origin', $publicUrl) -WindowStyle Hidden -WorkingDirectory $PSScriptRoot -PassThru -RedirectStandardOutput (Join-Path $publicDirectory 'server.log') -RedirectStandardError (Join-Path $publicDirectory 'server.stderr.log')
    $ready = $false
    for ($attempt = 0; $attempt -lt 30; $attempt++) {
        $serverProcess.Refresh()
        if ($serverProcess.HasExited) { throw 'C server exited. See build/public/server.stderr.log.' }
        try { $null = Invoke-RestMethod -Uri "http://127.0.0.1:$Port/api/state" -TimeoutSec 2; $ready = $true; break } catch { Start-Sleep -Milliseconds 200 }
    }
    if (-not $ready) { throw 'C server did not become ready.' }
    $publicReady = $false
    $lastPublicError = ''
    for ($attempt = 0; $attempt -lt 15; $attempt++) {
        try {
            $null = Invoke-RestMethod -Uri "$publicUrl/api/state" -TimeoutSec 3
            $publicReady = $true; break
        } catch {
            $lastPublicError = $_.Exception.Message + ' ' + $_.ErrorDetails.Message
            # Some local DNS servers negatively cache newly created tunnel names.
            # Verify against a public DNS resolver without changing machine DNS settings.
            try {
                $publicName = ([uri]$publicUrl).DnsSafeHost
                $dnsAnswer = Invoke-RestMethod -Uri "https://dns.google/resolve?name=$publicName&type=A" -TimeoutSec 3
                $edgeAddress = ($dnsAnswer.Answer | Where-Object { $_.type -eq 1 } | Select-Object -First 1).data
                if ($edgeAddress) {
                    $result = & curl.exe --silent --show-error --fail --max-time 4 --resolve "${publicName}:443:$edgeAddress" "$publicUrl/api/state" 2>$null
                    if ($LASTEXITCODE -eq 0 -and ($result | ConvertFrom-Json).state) {
                        $publicReady = $true
                        Write-Host 'Public HTTPS endpoint verified through public DNS; local DNS may need time to resolve the new hostname.'
                        break
                    }
                }
            } catch { $lastPublicError = $_.Exception.Message }
            Start-Sleep -Milliseconds 400
        }
    }
    if (-not $publicReady) { throw "Public endpoint did not become reachable: $publicUrl. $lastPublicError" }
    [ordered]@{
        PublicUrl = $publicUrl
        LocalUrl = "http://localhost:$Port"
        Port = $Port
        ServerProcessId = $serverProcess.Id
        ServerStartTimeUtc = $serverProcess.StartTime.ToUniversalTime().ToString('o')
        ServerExecutable = $executable
        TunnelProcessId = $tunnelProcess.Id
        TunnelStartTimeUtc = $tunnelProcess.StartTime.ToUniversalTime().ToString('o')
        TunnelExecutable = $tunnelExecutable
    } | ConvertTo-Json | Set-Content -LiteralPath $sessionPath
    Write-Host "Public simulator: $publicUrl" -ForegroundColor Green
    Write-Host "Local C server: http://localhost:$Port"
    Write-Host 'Running in the background. Stop with .\stop-public.ps1. Keep this computer awake and online.'
    if ($OpenBrowser) { Start-Process $publicUrl }
} catch {
    if ($serverProcess -and -not $serverProcess.HasExited) { Stop-Process -Id $serverProcess.Id }
    if ($tunnelProcess -and -not $tunnelProcess.HasExited) { Stop-Process -Id $tunnelProcess.Id }
    throw
}
