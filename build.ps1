param([switch]$Test, [switch]$Terminal)
$ErrorActionPreference = 'Stop'
Push-Location $PSScriptRoot
try {
    New-Item -ItemType Directory -Force build | Out-Null
    $compiler = 'gcc'
    $flags = @('-Wall', '-Wextra', '-Werror', '-std=c99', '-O2', '-Iinclude')
    $core = @('src/elevator.c', 'src/guardrails.c')
    & $compiler @flags @core src/simulation.c src/web_server.c -o build/elevator_web.exe -lws2_32
    if ($LASTEXITCODE -ne 0) { throw 'Web server compilation failed.' }
    Write-Host 'Built build/elevator_web.exe. Start with .\run-web.ps1' -ForegroundColor Green
    if ($Terminal) {
        & $compiler @flags @core src/main.c -o build/elevator_guardrails.exe
        if ($LASTEXITCODE -ne 0) { throw 'Terminal compilation failed.' }
    }
    if ($Test) {
        & $compiler @flags @core tests/test_simple.c -o build/test_simple.exe
        if ($LASTEXITCODE -ne 0) { throw 'Original test compilation failed.' }
        & .\build\test_simple.exe
        if ($LASTEXITCODE -ne 0) { throw 'Original tests failed.' }
        & $compiler @flags @core src/simulation.c tests/test_simulation.c -o build/test_simulation.exe
        if ($LASTEXITCODE -ne 0) { throw 'Passenger test compilation failed.' }
        & .\build\test_simulation.exe
        if ($LASTEXITCODE -ne 0) { throw 'Passenger tests failed.' }
        if (Get-Command node -ErrorAction SilentlyContinue) {
            & node tests/test_web.mjs
            if ($LASTEXITCODE -ne 0) { throw 'HTTP integration tests failed.' }
        }
    }
} finally { Pop-Location }
