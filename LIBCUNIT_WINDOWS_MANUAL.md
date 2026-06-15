# Manual libcunit Installation for Windows

Since automated scripts have encoding issues on some Windows systems, this guide provides step-by-step manual installation instructions.

## Prerequisites

✅ **Already have:**
- GCC from Strawberry Perl: `C:\Strawberry\c\bin\gcc.exe`
- GNU Make (included with Strawberry Perl)
- PowerShell or Command Prompt

## Step-by-Step Installation

### Step 1: Download libcunit Source

1. Open your web browser
2. Go to: https://github.com/Farinacci/cunit/archive/master.zip
3. Click to download `master.zip`
4. Extract to: `C:\temp\cunit` (create temp folder if needed)

**Result:** You should have:
```
C:\temp\cunit\
├── Makefile
├── Makefile.in
├── configure
├── CUnit/
└── ... (other files)
```

### Step 2: Open Command Prompt/PowerShell

```
Windows Key → Type "PowerShell" → Press Enter
```

Or use Command Prompt:
```
Windows Key → Type "cmd" → Press Enter
```

### Step 3: Navigate to Source Directory

```bash
cd C:\temp\cunit
```

### Step 4: Run Configure

```bash
./configure --prefix=C:\strawberry\c --enable-shared --enable-static
```

**Expected output:**
```
checking for a BSD-compatible install... /usr/bin/install -c
checking whether build environment is sane... yes
...
config.status: creating Makefile
config.status: creating doc/Makefile
```

### Step 5: Compile

```bash
make
```

**Expected output:**
```
Compiling... 
Linking libcunit...
[many lines of output]
```

**Time:** 30-60 seconds

### Step 6: Install

```bash
make install
```

**Expected output:**
```
Installing libcunit...
Copying library files...
Copying header files...
```

### Step 7: Verify Installation

Check that files were installed:

```bash
# Check headers
ls C:\strawberry\c\include\CUnit

# Check library
ls C:\strawberry\c\lib\*cunit*
```

**Expected output:**
```
Mode                 Name
----                 ----
-a---          [timestamp]  CUnit.h
-a---          [timestamp]  CUnitTypes.h
-a---          [timestamp]  TestDB.h
-a---          [timestamp]  TestRun.h
```

### Step 8: Test CUnit

Go back to project directory:

```bash
cd C:\Users\miros\Code\elevator_guardrail
```

Run the CUnit tests:

```bash
make test-cunit
```

**Expected output:**
```
Linking CUnit test executable...
Running CUnit tests...

     CUnit - A unit testing framework for C - Version 2.1-3
     http://cunit.sourceforge.net/

Suite: Elevator Tests
  Test: Initialization ...passed
  ...
Run Summary:    Type  Total    Ran Passed Failed Inactive
              suites      2      2    n/a      0        0
               tests     55     55     55      0        0
```

---

## If Something Goes Wrong

### Error: "configure: command not found"

The source may not have extracted properly. Make sure `configure` file exists:

```bash
ls C:\temp\cunit\configure
```

If not found, re-extract the ZIP file.

### Error: "make: command not found"

GNU Make is not in PATH. Add Strawberry Perl to PATH:

```powershell
$env:PATH += ";C:\Strawberry\c\bin;C:\Strawberry\perl\bin"
make
```

### Error: "gcc: command not found"

GCC is not in PATH. Add Strawberry Perl:

```powershell
$env:PATH += ";C:\Strawberry\c\bin"
gcc --version
```

### Lib files not installed

Try `make install` again with:

```bash
make install
```

If that fails, manually copy the lib files:

```bash
# Find where libs were built
ls build/

# Copy them manually
cp build/libcunit.a C:\strawberry\c\lib\
```

---

## Verification Checklist

After installation, verify:

- [ ] `C:\strawberry\c\include\CUnit\` folder exists
- [ ] `C:\strawberry\c\lib\libcunit.a` file exists (or .dll)
- [ ] `make test-cunit` runs without linker errors
- [ ] All 55 CUnit tests pass

If all checked, installation is complete! ✅

---

## Alternative: Using Pre-built Binaries

If the above doesn't work, you can try pre-built binaries:

1. Download pre-built CUnit for Windows from:
   - MSYS2 repository
   - MinGW repository
   - Or compile on WSL and copy files

2. Install files to:
   ```
   C:\strawberry\c\include\CUnit\
   C:\strawberry\c\lib\
   ```

3. Then run: `make test-cunit`

---

## Getting Help

If you still have issues:

1. Check that GCC works:
   ```bash
   gcc --version
   gcc -v
   ```

2. Check Strawberry Perl is installed:
   ```bash
   perl --version
   ```

3. Check configure script exists:
   ```bash
   ls C:\temp\cunit\configure
   ```

4. Run configure with verbose output:
   ```bash
   ./configure --prefix=C:\strawberry\c --verbose
   ```

---

## What Happens Next

Once libcunit is installed:

```bash
# Run all tests (simple + CUnit)
make test

# Or just CUnit tests
make test-cunit

# Or just simple tests (always works)
make test-simple
```

**Result:** All 104 assertions should pass (49 simple + 55 CUnit)

---

## Time Required

- Download: 30-60 seconds
- Extract: 10 seconds
- Configure: 10-20 seconds
- Build: 30-60 seconds
- Install: 5-10 seconds
- **Total: 2-3 minutes**

---

## Summary

✅ Easy to follow steps  
✅ Error recovery instructions  
✅ Verification checks  
✅ Takes 2-3 minutes  
✅ No special tools needed  

You're all set! Run the tests and enjoy 100% test pass rate. 🎉
