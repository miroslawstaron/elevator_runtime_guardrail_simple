# Build script for Elevator Simulator on Windows
# Usage: .\build.ps1

$CC = "gcc"
$CFLAGS = "-Wall -Wextra -std=c99 -O2"
$TARGET = "elevator_sim.exe"
$SOURCES = @("main.c", "elevator.c", "guardrails.c")
$HEADERS = @("elevator.h", "guardrails.h")

Write-Host "Building Elevator Simulator..." -ForegroundColor Green

# Compile all sources
$COMPILE_CMD = "$CC $CFLAGS -o $TARGET $($SOURCES -join ' ')"
Write-Host "Command: $COMPILE_CMD"
Invoke-Expression $COMPILE_CMD

if ($LASTEXITCODE -eq 0) {
    Write-Host "✓ Build successful!" -ForegroundColor Green
    Write-Host "  Run with: .\$TARGET" -ForegroundColor Cyan
} else {
    Write-Host "✗ Build failed!" -ForegroundColor Red
    exit 1
}
