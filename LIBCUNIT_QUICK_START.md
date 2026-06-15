# libcunit Quick Install Guide

## TL;DR (30 seconds)

```powershell
# Open PowerShell and run:
cd C:\Users\miros\Code\elevator_guardrail
powershell -ExecutionPolicy Bypass -File scripts\build-libcunit.ps1

# Then test:
make test-cunit
```

---

## What This Does

Downloads, builds, and installs libcunit testing library for CUnit tests.

**Before:** ❌ 55 CUnit tests can't run (libcunit missing)
**After:** ✅ 55 CUnit tests pass

---

## Step-by-Step

### 1️⃣ Open Command Prompt/PowerShell
```
Windows Key → type "PowerShell" → Press Enter
```

### 2️⃣ Go to Project Directory
```powershell
cd C:\Users\miros\Code\elevator_guardrail
```

### 3️⃣ Run the Build Script
```powershell
powershell -ExecutionPolicy Bypass -File scripts\build-libcunit.ps1
```

Wait 2-3 minutes for build to complete...

### 4️⃣ Verify Installation
```bash
make test-cunit
```

Should show:
```
Run Summary:    Type  Total    Ran Passed Failed Inactive
              suites      2      2    n/a      0        0
               tests     55     55     55      0        0
```

---

## If It Fails

### "GCC not found"
```powershell
# Add Strawberry Perl to PATH:
$env:PATH += ";C:\Strawberry\c\bin"
# Then run the script again
```

### "Download failed"
Check internet connection, then try again.

---

## What Gets Installed

```
C:\strawberry\c\
├── include\CUnit\
└── lib\libcunit.a
```

The Makefile automatically finds it there.

---

## After Installation

Run any of these:
```bash
make test        # 49 simple tests
make test-cunit  # 55 CUnit tests
make             # Both
```

---

## Full Guide

See: [docs/LIBCUNIT_INSTALL.md](docs/LIBCUNIT_INSTALL.md)

---

## Alternative (Manual Build)

```bash
git clone https://github.com/Farinacci/cunit.git
cd cunit
./configure --prefix=C:\strawberry\c
make
make install
cd ..
make test-cunit
```

---

Done! CUnit tests now enabled. 🎉
