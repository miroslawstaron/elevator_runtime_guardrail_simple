# Elevator Guardrails Unit Tests

Comprehensive unit test suite for the elevator control system and guardrails using CUnit.

## Prerequisites

### Installation

**Ubuntu/Debian:**
```bash
sudo apt-get install libcunit1 libcunit1-dev
```

**macOS (Homebrew):**
```bash
brew install cunit
```

**Windows (with MSYS2/MinGW):**
```bash
pacman -S mingw-w64-x86_64-cunit
```

**Or build from source:**
```bash
wget https://sourceforge.net/projects/cunit/files/CUnit/2.1-3/CUnit-2.1-3.tar.bz2
tar -xjf CUnit-2.1-3.tar.bz2
cd CUnit-2.1-3
./configure
make
sudo make install
```

## Build Instructions

### Using Make

```bash
# Build everything (main program + tests)
make all

# Run tests only
make test

# Run main program
make run

# Clean build artifacts
make clean

# Show help
make help
```

### Using CMake

```bash
# Create build directory
mkdir build
cd build

# Configure and build
cmake ..
cmake --build .

# Run tests
ctest

# Or run directly
./test_elevator_guardrails
```

## Test Coverage

### Elevator Tests (28 test cases)

**Initialization Tests:**
- Elevator initializes at floor 1 with IDLE state

**Button Press Tests:**
- Valid floor button presses
- Button presses from IDLE state
- Multiple button presses
- Floor boundary (0 and 9) presses

**Emergency Stop Tests:**
- Press emergency stop button
- Release emergency stop button

**State Transition Tests:**
- IDLE → SMOOTH_START_UP → MOVING_UP
- IDLE → SMOOTH_START_DOWN → MOVING_DOWN
- Smooth motion transitions

**Movement Tests:**
- Moving up advances floor
- Moving down decreases floor
- Floor boundary enforcement

**Door Operation Tests:**
- Door opening animation
- Button clearing when doors open
- Between-floors detection

**Integration Tests:**
- Complete journey upward
- Complete journey downward
- Multiple stops sequence

### Guardrails Tests (26 test cases)

**Basic Guardrail Checks:**
- No violations in normal operation
- Doors open at floor is safe

**Doors Open Violation Tests:**
- Doors open while MOVING_UP
- Doors open while MOVING_DOWN
- Doors open during smooth motion phases

**Emergency Stop Tracking:**
- Stop button press detection
- Seconds elapsed tracking

**Between Floors Violation Tests:**
- Emergency stop between floors detection

**Enforcement Tests:**
- Force stop when doors open while moving
- Continue to nearest floor when stopped between floors
- Move down when stopped in upper half
- Move up when stopped in lower half

**Violation Description Tests:**
- Generate appropriate error messages
- Handle multiple violations

**Edge Cases:**
- Doors not a violation if not moving
- Doors safe when fully closed

## Test Results

When you run `make test`, you should see output like:

```
=====================================
Test Summary:
=====================================
Tests run:     54
Tests passed:  54
Tests failed:   0
Assertions:     54
```

## Test Statistics

- **Total Test Cases:** 54
- **Elevator Tests:** 28
- **Guardrails Tests:** 26
- **Coverage Areas:** Initialization, State Transitions, Movement, Door Operations, Emergency Stop, Safety Guardrails

## Test Organization

### test_elevator.c
Tests for the state machine and elevator movement logic:
- Initialization and configuration
- Button handling and queuing
- State transitions and timing
- Position tracking and movement
- Door operations
- Emergency stop functionality

### test_guardrails.c
Tests for safety guardrails and constraint enforcement:
- Violation detection (doors open while moving)
- Emergency stop tracking
- Between-floors violation handling
- Guardrail enforcement actions
- Error message generation

### test_runner.c
Main test harness that:
- Initializes CUnit test registry
- Registers all test suites
- Runs all tests in verbose mode
- Reports test statistics

## Running Individual Tests

To run just the elevator tests or guardrails tests, you can modify test_runner.c to comment out one of the `add_*_tests()` calls.

Alternatively, use CUnit's test selection features by modifying the runner with a filter.

## Extending Tests

To add new tests:

1. **Add test function** to test_elevator.c or test_guardrails.c:
```c
static void test_new_feature(void) {
    elevator_init(test_elev);
    // Set up conditions
    elevator_some_operation(test_elev);
    // Assert expected behavior
    CU_ASSERT_EQUAL(test_elev->state, EXPECTED_STATE);
}
```

2. **Register test** in the suite setup function:
```c
CU_add_test(pSuite, "New feature test", test_new_feature);
```

3. **Rebuild and run:**
```bash
make clean
make test
```

## Debugging Failed Tests

To debug a specific test:

1. Compile with debug symbols (already done with `-g` flag):
```bash
gcc -Wall -Wextra -std=c99 -g -o debug_test test_elevator.c elevator.c test_runner.c -lcunit
```

2. Run under debugger:
```bash
gdb ./debug_test
(gdb) run
(gdb) where  # at breakpoint
(gdb) print test_elev->state  # inspect variables
```

## Notes

- All tests use isolated Elevator instances
- Setup/teardown functions ensure test isolation
- Tests can be run in any order
- No external dependencies besides CUnit
- Tests verify both correct behavior and edge cases
