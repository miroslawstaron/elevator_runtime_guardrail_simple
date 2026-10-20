<#
****************************************
*
* Elevator Lab -- Windows Build and Verification
*
* SPDX-License-Identifier: MIT
*
****************************************
#>

param([switch]$Test, [switch]$Terminal)
$ErrorActionPreference = 'Stop'
# BUILD 1: Resolve relative build inputs from the project root and restore the caller directory afterward
Push-Location $PSScriptRoot
try {
    New-Item -ItemType Directory -Force build | Out-Null
    # DOCUMENTATION: Reject missing or stale published diagrams when Node is available
    if (Get-Command node -ErrorAction SilentlyContinue) {
        & node tools/check-evolution.mjs
        if ($LASTEXITCODE -ne 0) { throw 'Evolution documentation is incomplete or stale. Run node tools/render-diagrams.mjs.' }
    }
    # BUILD 2: Compile the authored C modules with strict warnings and link embedded SQLite and Windows sockets
    $compiler = 'gcc'
    $flags = @('-Wall', '-Wextra', '-Werror', '-std=c99', '-O2', '-Iinclude', '-Ivendor/sqlite')
    $core = @('src/elevator.c', 'src/guardrails.c')
    $sqliteObject = & .\tools\build-sqlite.ps1
    & $compiler @flags @core src/simulation.c src/telemetry.c src/web_server.c $sqliteObject -o build/elevator_web.exe -lws2_32
    if ($LASTEXITCODE -ne 0) { throw 'Web server compilation failed.' }
    Write-Host 'Built build/elevator_web.exe. Start with .\run-web.ps1' -ForegroundColor Green
    # OPTIONAL TARGET: Preserve the original terminal executable alongside the web simulator
    if ($Terminal) {
        & $compiler @flags @core src/main.c -o build/elevator_guardrails.exe
        if ($LASTEXITCODE -ne 0) { throw 'Terminal compilation failed.' }
    }
    # VERIFICATION: Run original safety checks, passenger dispatch, persistence and available HTTP integration checks
    if ($Test) {
        & $compiler @flags @core tests/test_simple.c -o build/test_simple.exe
        if ($LASTEXITCODE -ne 0) { throw 'Original test compilation failed.' }
        & .\build\test_simple.exe
        if ($LASTEXITCODE -ne 0) { throw 'Original tests failed.' }
        & $compiler @flags @core src/simulation.c tests/test_simulation.c -o build/test_simulation.exe
        if ($LASTEXITCODE -ne 0) { throw 'Passenger test compilation failed.' }
        & .\build\test_simulation.exe
        if ($LASTEXITCODE -ne 0) { throw 'Passenger tests failed.' }
        & $compiler @flags @core src/simulation.c src/telemetry.c tests/test_telemetry.c $sqliteObject -o build/test_telemetry.exe
        if ($LASTEXITCODE -ne 0) { throw 'Telemetry test compilation failed.' }
        & .\build\test_telemetry.exe
        if ($LASTEXITCODE -ne 0) { throw 'Telemetry tests failed.' }
        if (Get-Command node -ErrorAction SilentlyContinue) {
            & node tests/test_web.mjs
            if ($LASTEXITCODE -ne 0) { throw 'HTTP integration tests failed.' }
            & node tests/test_telemetry_web.mjs
            if ($LASTEXITCODE -ne 0) { throw 'Telemetry HTTP integration tests failed.' }
        }
    }
} finally { Pop-Location }
