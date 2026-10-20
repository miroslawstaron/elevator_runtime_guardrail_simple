<#
****************************************
*
* Elevator Lab -- Embedded SQLite Build Helper
*
* SPDX-License-Identifier: MIT
*
****************************************
#>

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
# BUILD INPUTS: Compile the vendored source into a reusable object inside the build directory
$sourcePath = Join-Path $projectRoot 'vendor\sqlite\sqlite3.c'
$objectPath = Join-Path $projectRoot 'build\sqlite3.o'
New-Item -ItemType Directory -Force (Join-Path $projectRoot 'build') | Out-Null
# CACHE: Rebuild only when the object is absent or the SQLite source is newer
if (-not (Test-Path -LiteralPath $objectPath) -or (Get-Item -LiteralPath $sourcePath).LastWriteTimeUtc -gt (Get-Item -LiteralPath $objectPath).LastWriteTimeUtc) {
    # SQLITE OPTIONS: Keep extension loading disabled and use the single-threaded runtime configuration
    & gcc -std=c99 -O2 -DSQLITE_OMIT_LOAD_EXTENSION -DSQLITE_THREADSAFE=0 -c $sourcePath -o $objectPath | Out-Host
    if ($LASTEXITCODE -ne 0) { throw 'Embedded SQLite compilation failed.' }
}
# OUTPUT CONTRACT: Emit only the object path so callers can use it directly as a linker input
Write-Output $objectPath
