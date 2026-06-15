# Installing libcunit on Windows

This guide explains how to build and install libcunit (CUnit) for the elevator control system project on Windows.

## Overview

libcunit is a unit testing framework for C. The project includes automated scripts to build it from source for Windows with MinGW/GCC.

### What's Included

- `scripts/build-libcunit.ps1` — PowerShell script (recommended)
- `scripts/build-libcunit.bat` — Batch file wrapper
- `scripts/build-libcunit.sh` — Bash script (for WSL/Git Bash)

---

## Quick Start (Easiest Method)

### Step 1: Open Command Prompt or PowerShell

Navigate to the project directory:
```cmd
cd C:\Users\miros\Code\elevator_guardrail
```

### Step 2: Run the Build Script

**Option A: Using PowerShell (Recommended)**
```powershell
powershell -ExecutionPolicy Bypass -File scripts\build-libcunit.ps1
```

**Option B: Using Batch File**
```cmd
scripts\build-libcunit.bat
```

**Option C: Using PowerShell (with default install location)**
```powershell
.\scripts\build-libcunit.ps1
```

### Step 3: Wait for Build to Complete

The script will:
1. Check for GCC compiler
2. Download libcunit source from GitHub
3. Build the library with your GCC
4. Install to `C:\strawberry\c` (default Strawberry Perl location)
5. Verify installation

### Step 4: Test CUnit Tests

Once installation is complete:
```bash
make test-cunit
```

Or test both test suites:
```bash
make test        # Simple tests (always works)
make test-cunit  # CUnit tests (if libcunit installed)
```

---

## Installation Locations

### Default Install Location
```
C:\strawberry\c\
├── include\CUnit\      (header files)
└── lib\                (libcunit.a, libcunit.dll, etc.)
```

This is where Strawberry Perl GCC expects libraries, so the Makefile will find them automatically.

### Custom Install Location

To install to a different location:

```powershell
# Install to C:\libraries\cunit
.\scripts\build-libcunit.ps1 -InstallPrefix "C:\libraries\cunit"
```

Then update your Makefile:
```makefile
CUNIT_LDFLAGS = -L"C:\libraries\cunit\lib" -lcunit
```

---

## Prerequisites

You need:

1. **GCC Compiler** — Already installed (from Strawberry Perl)
   - Path: `C:\Strawberry\c\bin\gcc.exe`
   - Check: `gcc --version`

2. **PowerShell** or **Batch** (for Windows)
   - PowerShell: Already installed on Windows 10/11
   - Check: `powershell --version`

3. **Internet Connection** — To download libcunit source

---

## Troubleshooting

### "GCC not found"
**Problem:** Script can't find the GCC compiler

**Solution:**
```powershell
# Check if GCC is in PATH
gcc --version

# If not found, add Strawberry Perl to PATH:
$env:PATH += ";C:\Strawberry\c\bin"

# Then run the script again
.\scripts\build-libcunit.ps1
```

### "Download failed"
**Problem:** Script can't download libcunit source

**Solution:** Download manually and extract to temp directory, then run make:
```bash
# Download and extract manually, then:
make test-cunit
```

### "Configure script not found"
**Problem:** Autotools not available

**Solution:** The script falls back to CMake if available, or you can:
1. Install autotools via MSYS2
2. Or use CMake: `cmake --build .`

### "Permission denied" during install
**Problem:** Can't write to install directory

**Solution:** Use a user-writable location:
```powershell
.\scripts\build-libcunit.ps1 -InstallPrefix "C:\Users\$env:USERNAME\cunit"
```

---

## Manual Installation (Advanced)

If the automated script doesn't work, you can build manually:

### Step 1: Download Source
```bash
# Download from GitHub
git clone https://github.com/Farinacci/cunit.git
cd cunit
```

### Step 2: Configure
```bash
./configure --prefix=C:\strawberry\c
```

### Step 3: Build
```bash
make
```

### Step 4: Install
```bash
make install
```

### Step 5: Test
```bash
make test-cunit
```

---

## Verifying Installation

After installation, verify libcunit is accessible:

```bash
# Check for header files
dir "C:\strawberry\c\include\CUnit"

# Check for library files
dir "C:\strawberry\c\lib\*cunit*"

# Run the CUnit tests
make test-cunit
```

Expected output:
```
     CUnit - A unit testing framework for C - Version 2.1-3
     http://cunit.sourceforge.net/

Suite: Elevator Tests
  Test: ...passed
  ...

Run Summary:    Type  Total    Ran Passed Failed Inactive
              suites      2      2    n/a      0        0
               tests     55     55     55      0        0
```

---

## Understanding the Build

### What the Script Does

1. **Check Environment**
   - Verifies GCC is installed
   - Checks PowerShell/Bash availability

2. **Download**
   - Gets libcunit source from GitHub
   - Falls back to SourceForge if needed
   - Extracts to temp directory

3. **Build**
   - Runs `./configure` with options
   - Compiles with `make`
   - Installs to chosen prefix

4. **Verify**
   - Checks if headers and libs exist
   - Reports success/warnings

### Build Time

Expected time on typical Windows machine:
- Download: 30-60 seconds
- Configure: 10-30 seconds
- Build: 30-120 seconds
- Install: 5-10 seconds
- **Total: 2-3 minutes**

---

## Using CUnit Tests

Once libcunit is installed:

### Run All Tests
```bash
make test        # Simple tests (49 tests, always works)
make test-cunit  # CUnit tests (55 tests, requires libcunit)
make             # Both executables
```

### Run Only CUnit Tests
```bash
make test-cunit
./test_elevator_guardrails
```

### Expected Results

```
Total Tests:   55
Tests Passed:  55
Tests Failed:  0
Success Rate:  100.0%
```

---

## CUnit vs Simple Tests

| Feature | Simple Tests | CUnit Tests |
|---------|--------------|------------|
| Count | 49 tests | 55 tests |
| Dependencies | None | libcunit |
| Speed | Fast | Very fast |
| Reports | Basic | Detailed |
| Use | Development | Full validation |

Run both for comprehensive test coverage!

---

## Updating Makefile (If Needed)

If you installed to a non-standard location, update the Makefile:

```makefile
# Find this section:
CUNIT_LDFLAGS = -lm -lcunit

# If compiler can't find libcunit, add:
CUNIT_LDFLAGS = -Lyour/install/path/lib -lm -lcunit

# Also add include path if needed:
CFLAGS = -Wall -Wextra -std=c99 -g -Iinclude -Iyour/install/path/include
```

---

## Getting Help

If you encounter issues:

1. Check the error message from the build script
2. Verify GCC is working: `gcc --version`
3. Check internet connection for downloads
4. See Troubleshooting section above
5. Check `C:\ProgramData\chocolatey\logs\chocolatey.log` if using Chocolatey

---

## Alternative: Using WSL (Windows Subsystem for Linux)

If you're using WSL with Ubuntu/Debian:

```bash
# Install build tools and libcunit
sudo apt update
sudo apt install build-essential libcunit1 libcunit1-dev

# Then build and test normally
make test-cunit
```

---

## Summary

✅ **Automated:** Run one script to download, build, and install libcunit
✅ **Works with Strawberry Perl GCC:** Uses existing compiler
✅ **No Admin Required:** Installs to user-accessible location
✅ **Cross-Platform:** Scripts for Windows, Linux, macOS

Next step: Run `scripts\build-libcunit.ps1` and then `make test-cunit`!
