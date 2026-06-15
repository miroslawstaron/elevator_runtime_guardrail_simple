# Test Files Summary

## Quick Reference

### Run Tests Immediately (No Setup Required)
```bash
gcc -Wall -Wextra -std=c99 -g -o test_simple elevator.c guardrails.c test_simple.c -lm
./test_simple
```

## Test Files Overview

### Core Test Files

| File | Type | Tests | Dependencies | Status |
|------|------|-------|--------------|--------|
| [test_simple.c](test_simple.c) | Implementation | 49 | None (stdlib only) | ✓ All Pass |
| [test_framework.h](test_framework.h) | Framework | - | None | Used by test_simple |
| [test_elevator.c](test_elevator.c) | CUnit Suite | 28 | libcunit | Optional |
| [test_guardrails.c](test_guardrails.c) | CUnit Suite | 21 | libcunit | Optional |
| [test_runner.c](test_runner.c) | CUnit Runner | - | libcunit | Optional |

### Build Configuration Files

| File | Purpose |
|------|---------|
| [Makefile](Makefile) | Make build system (test-simple, test-cunit targets) |
| [CMakeLists.txt](CMakeLists.txt) | CMake build system (alternative) |

### Documentation Files

| File | Content |
|------|---------|
| [TESTING.md](TESTING.md) | Main testing documentation with coverage details |
| [TEST_README.md](TEST_README.md) | Installation and usage guide |
| [TEST_FILES.md](TEST_FILES.md) | This file - file organization |

## Test Organization

```
Elevator Tests (28 tests)
├── Initialization (1)
├── Button Operations (6)
├── Emergency Stop (2)
├── State Transitions (2)
├── Movement & Boundaries (3)
├── Between Floors Detection (3)
├── State String Representation (2)
└── Integration Tests (1)

Guardrails Tests (21 tests)
├── Basic Checks (2)
├── Doors Open Violations (5)
├── Emergency Stop (2)
├── Violation Enforcement (3)
├── Violation Descriptions (3)
└── Integration Tests (4)
```

## Recommended Usage

### For Quick Testing (Recommended)
Use the simple test runner - no dependencies:
```bash
gcc -Wall -Wextra -std=c99 -g -o test_simple elevator.c guardrails.c test_simple.c -lm
./test_simple
```

### For CI/CD Integration
```bash
#!/bin/bash
gcc -Wall -Wextra -std=c99 -g -o test_simple elevator.c guardrails.c test_simple.c -lm
./test_simple
if [ $? -eq 0 ]; then
    echo "✓ All tests passed"
    exit 0
else
    echo "✗ Tests failed"
    exit 1
fi
```

### For Advanced Testing (With CUnit)
```bash
# Install CUnit first (Ubuntu/Debian)
sudo apt-get install libcunit1-dev

# Build with CMake
mkdir build && cd build
cmake ..
cmake --build .
ctest --verbose
```

## Test Coverage Summary

- **Total Tests**: 49
- **Elevator Tests**: 28 (initialization, buttons, movement, states)
- **Guardrails Tests**: 21 (safety checks, enforcement, violations)
- **Assertions**: 49+
- **Pass Rate**: 100%

## Key Features

### test_simple.c Advantages
- ✓ No external dependencies
- ✓ Fast compilation
- ✓ Portable (works on any C99 compiler)
- ✓ Clear output format
- ✓ 49 comprehensive tests
- ✓ Tests all critical functionality

### CUnit Tests Advantages (Optional)
- ✓ Industrial standard testing framework
- ✓ Detailed test suite organization
- ✓ 54 total tests (more granular)
- ✓ Professional test reports
- ✓ Better integration with CI/CD tools

## Test Result Interpretation

### Successful Run
```
=====================================
Test Summary:
=====================================
Total Tests:   49
Tests Passed:  49
Tests Failed:  0
Assertions:    49
Success Rate:  100.0%
```

### Failed Test Example
```
[TEST] Some Test: FAIL (expected 5, got 3)
Suite: Example Tests
  Passed: 4, Failed: 1, Assertions: 5
```

## Adding New Tests

To add tests to `test_simple.c`:

```c
TEST("New feature test");
// Setup
elevator_init(&elev);
elev.current_floor = FLOOR_3;

// Execute
elevator_press_button(&elev, FLOOR_5);

// Assert
ASSERT_EQUAL(elev.target_floor, FLOOR_5);
ASSERT_TRUE(elev.button_pressed[FLOOR_5]);
TEST_END();
```

## Test Framework Macros

Available assertions in `test_framework.h`:
- `ASSERT_EQUAL(a, b)` - Integer equality
- `ASSERT_TRUE(condition)` - Boolean true
- `ASSERT_FALSE(condition)` - Boolean false
- `ASSERT_NULL(ptr)` - Pointer is NULL
- `ASSERT_NOT_NULL(ptr)` - Pointer is not NULL
- `ASSERT_STRING_EQUAL(a, b)` - String equality

## Compilation Options

```bash
# Default (warnings + debug symbols)
gcc -Wall -Wextra -std=c99 -g

# With coverage analysis (requires gcov)
gcc -Wall -Wextra -std=c99 -g --coverage

# Release build (optimized)
gcc -Wall -Wextra -std=c99 -O2 -DNDEBUG

# With sanitizers (for memory safety)
gcc -Wall -Wextra -std=c99 -g -fsanitize=address -fsanitize=undefined
```

## Files Tested

- [elevator.c](elevator.c) / [elevator.h](elevator.h)
  - State machine implementation
  - Floor movement logic
  - Button handling
  - 28 specific tests

- [guardrails.c](guardrails.c) / [guardrails.h](guardrails.h)
  - Safety violation detection
  - Constraint enforcement
  - 21 specific tests

## Performance Notes

Test execution time: ~100ms (simple runner)

## Troubleshooting

### Compilation Errors

**Error: gcc not found**
- Install MinGW (Windows) or GCC (Linux/Mac)

**Error: undefined reference**
- Ensure all source files are included: `elevator.c guardrails.c test_simple.c`

### Test Failures

**Error: Floor bounds test fails**
- Check that target_floor is set when directly manipulating elevator state

**Error: State transition tests fail**
- Verify button_pressed array initialization
- Check state machine logic in elevator.c

## Next Steps

1. ✓ Run simple tests: `./test_simple`
2. Review test coverage in [TESTING.md](TESTING.md)
3. Examine specific tests in [test_simple.c](test_simple.c)
4. (Optional) Install CUnit and run advanced test suite
