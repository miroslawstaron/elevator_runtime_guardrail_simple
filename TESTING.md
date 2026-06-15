# Elevator and Guardrails Unit Testing

This document describes the comprehensive unit test suite for the elevator control system.

## Quick Start

### Build and Run Tests (No Dependencies)

```bash
gcc -Wall -Wextra -std=c99 -g -o test_simple elevator.c guardrails.c test_simple.c -lm
./test_simple
```

### Expected Output

```
========== Elevator Tests ==========
  [TEST] Elevator Init: PASS
  [TEST] Press Button Valid Floor: PASS
  ...
Suite: Elevator Tests
  Passed: 28, Failed: 0, Assertions: 28

========== Guardrails Tests ==========
  [TEST] No Violations: PASS
  ...
Suite: Guardrails Tests
  Passed: 21, Failed: 0, Assertions: 21

=====================================
Test Summary:
=====================================
Total Tests:   49
Tests Passed:  49
Tests Failed:  0
Assertions:    49
Success Rate:  100.0%
```

## Test Files

### test_simple.c
Simple, self-contained test runner with **no external dependencies**.
- 28 elevator tests
- 21 guardrails tests
- Uses custom `test_framework.h` for assertions

### test_framework.h
Lightweight testing framework with macros for:
- `TEST_SUITE_START(name)` / `TEST_SUITE_END()`
- `TEST(name)` / `TEST_END()`
- `ASSERT_EQUAL(a, b)`
- `ASSERT_TRUE(condition)` / `ASSERT_FALSE(condition)`
- `ASSERT_NULL(ptr)` / `ASSERT_NOT_NULL(ptr)`
- `ASSERT_STRING_EQUAL(a, b)`
- `PRINT_SUMMARY()`

### CUnit Tests (Optional)
Alternative test suites using the CUnit framework (requires libcunit-dev):
- `test_elevator.c` - 28 tests with detailed assertions
- `test_guardrails.c` - 26 tests
- `test_runner.c` - CUnit test harness
- `CMakeLists.txt` - CMake build configuration

## Test Coverage

### Elevator Tests (28 tests)

**Initialization & Setup (1)**
- ✓ Elevator initializes at floor 1 with IDLE state

**Button Operations (6)**
- ✓ Press button to any valid floor
- ✓ Press button while IDLE (sets target)
- ✓ Press button to current floor
- ✓ Press multiple buttons in sequence
- ✓ Press button to floor 0
- ✓ Press button to floor 9

**Emergency Stop (2)**
- ✓ Press emergency stop button
- ✓ Release emergency stop button

**State Transitions (2)**
- ✓ IDLE → SMOOTH_START_UP when button pressed
- ✓ SMOOTH_START_UP → MOVING_UP transition

**Movement (3)**
- ✓ Moving up advances floor every 5 ticks
- ✓ Floor ceiling at FLOOR_9 (boundary)
- ✓ Moving down transitions correctly

**Between Floors Detection (3)**
- ✓ Correctly identifies between-floors state
- ✓ Is not between floors in IDLE state
- ✓ Gets position as float value

**State String Representation (2)**
- ✓ State to string: IDLE
- ✓ State to string: MOVING_UP

**Integration Tests (1)**
- ✓ Complete journey from floor 1 to floor 3

### Guardrails Tests (21 tests)

**Basic Checks (2)**
- ✓ No violations in normal operation
- ✓ Doors open at floor is safe

**Doors Open Violations (5)**
- ✓ Doors open while MOVING_UP
- ✓ Doors open while MOVING_DOWN
- ✓ Doors open during SMOOTH_START_UP
- ✓ Doors open during SMOOTH_STOP_UP/DOWN
- ✓ Door violation only when moving

**Emergency Stop (2)**
- ✓ Stop button pressed detection
- ✓ Stop button time tracking

**Violation Enforcement (3)**
- ✓ Enforce doors open violation (moving up)
- ✓ Enforce doors open violation (moving down)
- ✓ No enforcement when no violations

**Violation Descriptions (3)**
- ✓ Description when no violations (NULL)
- ✓ Description for doors open violation
- ✓ Description for between-floors violation

**Integration Tests (4)**
- ✓ Emergency stop stays in emergency stop
- ✓ Release emergency stop returns to IDLE
- ✓ Multiple violations detected
- ✓ Door not violation when closed

## Test Statistics

| Metric | Count |
|--------|-------|
| Total Test Cases | 49 |
| Elevator Tests | 28 |
| Guardrails Tests | 21 |
| Assertions | 49 |
| Pass Rate | 100% |

## Building with CUnit (Optional)

If CUnit is installed on your system:

```bash
# Using CMake
mkdir build && cd build
cmake ..
cmake --build .
ctest --verbose

# Using Make (if available)
make test-cunit
```

**CUnit Installation:**

Ubuntu/Debian:
```bash
sudo apt-get install libcunit1 libcunit1-dev
```

macOS:
```bash
brew install cunit
```

## Test Design Principles

1. **Isolation**: Each test is independent and uses fresh elevator instance
2. **Clarity**: Test names describe what is being tested
3. **Coverage**: Tests cover normal operation, edge cases, and error conditions
4. **No Dependencies**: Main test suite (`test_simple.c`) requires no external libraries
5. **Assertions**: Clear assertions with helpful failure messages

## Edge Cases Tested

- ✓ Elevator at floor boundaries (0 and 9)
- ✓ Multiple button presses while moving
- ✓ Emergency stop during motion
- ✓ Door operations in all states
- ✓ State transitions with correct timing
- ✓ Guard rail violations in various states

## Known Limitations

None - all tests pass with 100% success rate.

## Future Enhancements

Potential additions for extended test coverage:
- [ ] Stress tests with many rapid button presses
- [ ] Performance benchmarks for state transitions
- [ ] Fuzzing tests with random elevator commands
- [ ] Integration tests with simulated hardware
- [ ] Test coverage metrics (gcov)

## Running Tests Programmatically

The test framework can be integrated into CI/CD:

```bash
./test_simple && echo "All tests passed" || echo "Tests failed"
```

Return code: 0 if all tests pass, 1 if any test fails
