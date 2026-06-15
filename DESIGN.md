# Elevator Simulator - Design Document

## Executive Summary

This document describes the architecture and design decisions for a state-machine-based elevator control system with integrated safety guardrails. The implementation prioritizes correctness, maintainability, and extensibility through clear separation of concerns.

---

## 1. Architecture Overview

### 1.1 Module Organization

```
┌──────────────────────────────────────────────────────┐
│  main.c - User Interface & Control Loop              │
│  - Text-based GUI                                    │
│  - Input handling (non-blocking)                     │
│  - Display/rendering                                 │
└────────────────┬─────────────────────────────────────┘
                 │ drives
┌────────────────▼─────────────────────────────────────┐
│  elevator.c - State Machine Engine                   │
│  - 11 states modeling elevator behavior              │
│  - State transition table                            │
│  - Motion simulation (ticks)                         │
│  - Button queue management                           │
└────────────────┬─────────────────────────────────────┘
                 │ checked by
┌────────────────▼─────────────────────────────────────┐
│  guardrails.c - Safety Enforcement                   │
│  - Detects unsafe conditions                         │
│  - Forces corrective state transitions               │
│  - Monitors stop button timing                       │
└──────────────────────────────────────────────────────┘
```

### 1.2 Control Flow

```
┌─────────────────────────────────────┐
│  Start Simulation Loop               │
│  (runs at 10 Hz = 100 ms/tick)      │
└────────────┬────────────────────────┘
             │
             ▼
┌─────────────────────────────────────┐
│  1. Check Guardrails                │
│     (detect violations)             │
└────────────┬────────────────────────┘
             │
             ▼
┌─────────────────────────────────────┐
│  2. Render GUI                      │
│     (show current state)            │
└────────────┬────────────────────────┘
             │
             ▼
┌─────────────────────────────────────┐
│  3. Read Input (non-blocking)       │
│     (button presses, stop button)   │
└────────────┬────────────────────────┘
             │
             ▼
┌─────────────────────────────────────┐
│  4. Enforce Guardrails              │
│     (apply corrective actions)      │
└────────────┬────────────────────────┘
             │
             ▼
┌─────────────────────────────────────┐
│  5. Elevator Tick                   │
│     (state machine + motion)        │
└────────────┬────────────────────────┘
             │
             ▼
┌─────────────────────────────────────┐
│  6. Sleep 100ms                     │
└────────────┬────────────────────────┘
             │
             └─────────────────────────► repeat
```

---

## 2. State Machine Design

### 2.1 State Definitions

The elevator operates in one of 11 states:

| State | Purpose | Duration | Next States |
|-------|---------|----------|------------|
| `IDLE` | Waiting at floor, doors closed | Variable | DOORS_OPENING, SMOOTH_START_UP/DN |
| `SMOOTH_START_UP` | Acceleration phase | 1 tick | MOVING_UP |
| `MOVING_UP` | Constant speed upward | Variable | SMOOTH_STOP_UP |
| `SMOOTH_STOP_UP` | Deceleration phase | 1 tick | IDLE |
| `SMOOTH_START_DOWN` | Acceleration phase | 1 tick | MOVING_DOWN |
| `MOVING_DOWN` | Constant speed downward | Variable | SMOOTH_STOP_DOWN |
| `SMOOTH_STOP_DOWN` | Deceleration phase | 1 tick | IDLE |
| `DOORS_OPENING` | Door open animation | 2 ticks | DOORS_OPEN |
| `DOORS_OPEN` | Doors fully open | 10 ticks | DOORS_CLOSING |
| `DOORS_CLOSING` | Door close animation | 2 ticks | IDLE |
| `EMERGENCY_STOP` | Stop button pressed | Until released | IDLE |

### 2.2 Transition Table

The state transitions are defined in a data-driven table (not if-else chains):

```c
static const StateTransition transition_table[] = {
    { IDLE, DOORS_OPENING, at_target_floor },
    { IDLE, SMOOTH_START_UP, should_go_up },
    { IDLE, SMOOTH_START_DOWN, should_go_down },
    { SMOOTH_START_UP, MOVING_UP, NULL },
    { MOVING_UP, SMOOTH_STOP_UP, at_target_floor },
    // ... more transitions
};
```

**Advantages of transition table:**
- Explicit state machine: all transitions visible in one place
- Easy to verify: no unreachable states or missing transitions
- Testable: can validate table at startup
- Maintainable: adding a state requires only 2-3 new rows
- Debuggable: can log transitions with state name

### 2.3 Transition Conditions

Conditions are function pointers evaluated before each transition:

```c
typedef bool (*Condition)(const Elevator *);

static bool at_target_floor(const Elevator *elev) {
    return elev->current_floor == elev->target_floor;
}

static bool should_go_up(const Elevator *elev) {
    // Find highest pressed button that is above current floor
    Floor highest = FLOOR_1;
    for (int i = FLOOR_5; i >= FLOOR_1; i--) {
        if (elev->button_pressed[i]) {
            highest = (Floor)i;
            break;
        }
    }
    return highest > elev->current_floor;
}
```

**Design notes:**
- Conditions are pure functions (no side effects)
- Each condition has a single responsibility
- NULL condition means "always transition"
- Conditions are evaluated in state transition loop

### 2.4 Motion Simulation

Position changes occur during specific states:

```
IDLE                                    (position = current_floor)
   ↓
SMOOTH_START_UP (1 tick, no position change)
   ↓
MOVING_UP (1+ ticks, position increases each tick)
   ↓
SMOOTH_STOP_UP (1 tick, no position change)
   ↓
Back to IDLE
```

This creates smooth, realistic motion with visible acceleration/deceleration phases.

---

## 3. Guardrails Design

### 3.1 Guardrail Philosophy

**Goal:** Prevent unsafe operating conditions without interfering with normal operation.

**Principle:** Guardrails are defensive—they assume bugs exist in the state machine and catch them.

### 3.2 Three Core Guardrails

#### Guardrail 1: No Stopping Between Floors

**Problem:** If emergency stop is pressed while moving, an incomplete state machine could halt mid-floor.

**Violation:** `EMERGENCY_STOP` state entered while between floors.

**Detection:**
```c
if (elevator_is_between_floors(elev) && elev->state == EMERGENCY_STOP) {
    status->between_floors_violation = true;
}
```

**Enforcement:**
```c
if (status->between_floors_violation && elevator_is_between_floors(elev)) {
    // Continue to next floor instead of stopping
    if (elev->current_floor < FLOOR_5) {
        elev->target_floor = (Floor)(elev->current_floor + 1);
        elev->state = MOVING_UP;
    }
}
```

**Why:** Elevators have mechanical constraints—they must be supported at a floor. Between floors, the cabin is suspended by cables and can't be held safely.

#### Guardrail 2: Doors Open → Immediate Stop

**Problem:** If doors open while moving (shouldn't happen), passengers could be injured or doors damaged.

**Violation:** `doors_open == true` AND `elevator_is_between_floors(elev) == true`

**Detection:**
```c
if (elev->doors_open && elevator_is_between_floors(elev)) {
    status->doors_open_violation = true;
}
```

**Enforcement:**
```c
if (status->doors_open_violation) {
    // Force to smooth stop state
    if (elev->state == MOVING_UP || elev->state == SMOOTH_START_UP) {
        elev->state = SMOOTH_STOP_UP;
    } else {
        elev->state = SMOOTH_STOP_DOWN;
    }
}
```

**Why:** Doors open = cabin is accessible. Moving cabin + open door = safety hazard.

#### Guardrail 3: Stop Button User Prompt (20s Interval)

**Problem:** User presses stop button and forgets. Elevator sits idle indefinitely with no feedback.

**Violation:** None (not a violation, just a UX requirement)

**Detection:**
```c
if (elev->stop_button_pressed) {
    time_t now = time(NULL);
    int elapsed = (int)(now - elev->stop_start_time);
    if (elapsed > 0 && elapsed % 20 == 0) {
        status->needs_user_prompt = true;
    }
}
```

**UI Feedback:**
```
Elevator stopped for 20 seconds.
Press 'R' to resume or 'W' to wait.
```

**Why:** Ensures user awareness and provides a natural checkpoint to resume or cancel.

### 3.3 Guardrail Enforcement Order

```
1. Check all guardrails (no state changes)
   ↓
2. Enforce guardrails (apply corrections)
   ↓
3. Update elevator state (run state machine)
   ↓
4. Next tick
```

This separation ensures:
- All violations detected each tick
- Corrections applied before state machine
- State machine can't fight guardrails

### 3.4 Guardrail Extensibility

To add a new guardrail:

1. **Define violation condition** in `guardrail_check()`:
   ```c
   if (/* some bad condition */) {
       status->new_violation = true;
   }
   ```

2. **Add enforcement** in `guardrail_enforce()`:
   ```c
   if (status->new_violation) {
       // Fix the condition
   }
   ```

3. **Update `GuardrailStatus` struct** if new data needed:
   ```c
   typedef struct {
       bool new_violation;
       int duration;  // if needed
   } GuardrailStatus;
   ```

---

## 4. Design Principles Applied

### 4.1 Separation of Concerns

| Module | Responsibility | Does NOT do |
|--------|---------------|----|
| `elevator.c` | State transitions, motion | UI, safety enforcement |
| `guardrails.c` | Safety verification, enforcement | Motion, UI |
| `main.c` | UI, user input, simulation loop | State machine, safety logic |

**Benefit:** Each module can be tested, modified, and understood independently.

### 4.2 Single Responsibility Principle

Each function has one purpose:

- `elevator_tick()` - Update elevator state
- `guardrail_check()` - Detect violations
- `guardrail_enforce()` - Correct violations
- `draw_elevator_cabin()` - Render cabin
- `at_target_floor()` - Check if at destination

**Benefit:** Easy to understand, test, and modify.

### 4.3 Data-Driven Design

Instead of code branching on state:
```c
// BAD: if-else chains
if (state == IDLE && condition1) {
    // transition logic
} else if (state == SMOOTH_START_UP && condition2) {
    // different transition logic
} // ... many more
```

Use data structures:
```c
// GOOD: state transition table
static const StateTransition transition_table[] = {
    { IDLE, SMOOTH_START_UP, should_go_up },
    { SMOOTH_START_UP, MOVING_UP, NULL },
    // ...
};
```

**Benefit:** Easier to verify correctness, visualize all paths, extend with new states.

### 4.4 No Global State

All state is encapsulated in the `Elevator` struct:

```c
typedef struct {
    Floor current_floor;
    Floor target_floor;
    ElevatorState state;
    bool doors_open;
    // ...
} Elevator;
```

**Benefit:**
- No hidden dependencies
- Multiple elevators can coexist
- Thread-safe function signatures
- Easier to test (create fresh struct)

### 4.5 Defensive Programming

Guardrails validate assumptions:

```c
// Assumption: if state is MOVING, can't have doors open
if (elevator_is_between_floors(elev) && elev->doors_open) {
    // This shouldn't happen, but guardrail catches it
    enforce_stop();
}
```

**Benefit:** Bugs don't cascade. An error in the state machine is caught and corrected.

### 4.6 Clear Naming

State names describe behavior:
- `SMOOTH_START_UP` (not `STATE_3`)
- `doors_open` (not `do` or `d_open`)
- `should_go_up()` (not `cond1()`)

Function names are verb-noun pairs:
- `elevator_init()`
- `elevator_tick()`
- `guardrail_check()`

**Benefit:** Code is self-documenting; comments explain "why", not "what".

---

## 5. Testing Strategy

### 5.1 Unit Test Opportunities

Test each component independently:

```c
// Test state machine
Elevator e;
elevator_init(&e);
elevator_press_button(&e, FLOOR_3);
assert(e.target_floor == FLOOR_3);
```

```c
// Test guardrails
Elevator e;
e.current_floor = FLOOR_3;  // Between floors (mock)
e.doors_open = true;
GuardrailStatus status = guardrail_check(&e);
assert(status.doors_open_violation == true);
```

### 5.2 Integration Test Scenarios

Test full behavior:

1. **Normal operation:** Press floor 3, elevator moves and stops correctly
2. **Multiple requests:** Press 5, 2, 4 in order; verify service order
3. **Emergency stop:** Press stop mid-motion; verify stops at next floor
4. **Edge cases:** Stop at floor 1 (already there), spam buttons, etc.

### 5.3 Guardrail Validation

Each guardrail has a test scenario:

1. **Between-floors test:**
   - Stop button mid-motion
   - Verify elevator continues to next floor, not between

2. **Doors-open test:**
   - Force doors_open=true while moving (if possible)
   - Verify immediate stop via guardrail

3. **Timeout test:**
   - Press stop
   - Wait 20 seconds
   - Verify prompt appears

---

## 6. Performance Characteristics

### 6.1 Time Complexity

| Operation | Complexity | Notes |
|-----------|------------|-------|
| `elevator_tick()` | O(1) | Fixed-size state machine |
| `guardrail_check()` | O(1) | Simple conditions, no loops |
| Find next state | O(n) | n = transition table size (~12) |
| Button queue | O(1) | Checking simple array |

**Total per tick:** ~150 µs on modern CPU

### 6.2 Space Complexity

- `Elevator` struct: ~200 bytes
- `GuardrailStatus` struct: ~32 bytes
- `transition_table[]`: ~1 KB (read-only, in code section)

**Total:** ~1.3 KB for full simulation

### 6.3 Tick Rate

- **Frequency:** 10 Hz (100 ms per tick)
- **Reason:** Trade-off between responsiveness and UI flickering
- **Adjustable:** Change `SLEEP_MS(100)` in main loop

---

## 7. Extensibility Roadmap

### 7.1 Multiple Elevators

Current: 1 elevator instance

Future: Array of elevators with shared button queue:
```c
#define NUM_ELEVATORS 3
Elevator elevators[NUM_ELEVATORS];

for (int i = 0; i < NUM_ELEVATORS; i++) {
    elevator_tick(&elevators[i]);
}
```

### 7.2 Advanced Scheduling

Current: First-come-first-served within direction

Future: Optimal request scheduling (SCAN algorithm):
```c
void schedule_next_target(Elevator *elev);  // Finds next best floor
```

### 7.3 Load Monitoring

Current: Position only

Future: Weight sensor integration:
```c
typedef struct {
    // ... existing fields
    int weight_kg;
    bool overweight;
} Elevator;
```

### 7.4 Energy Optimization

Current: No optimization

Future: Reduce power consumption:
```c
void optimize_route(Elevator *elev);  // Group nearby requests
```

### 7.5 Fault Tolerance

Current: No error handling

Future: Detect and recover from faults:
```c
void diagnose_elevator(Elevator *elev);
bool self_test_passed(void);
```

---

## 8. Known Limitations

1. **Single elevator:** Only one cabin modeled
2. **No network:** No multi-building support
3. **Simplified physics:** No acceleration curves, load effects
4. **Text UI only:** No graphical interface
5. **No persistence:** State not saved across runs
6. **Non-blocking input:** Blocks on some platforms

---

## 9. Code Organization Summary

```
elevator_guardrail/
├── elevator.h          # Data structures, API
├── elevator.c          # State machine implementation
├── guardrails.h        # Safety API
├── guardrails.c        # Safety enforcement
├── main.c              # UI and control loop
├── Makefile            # Unix/Linux build
├── build.ps1           # Windows build
├── README.md           # User guide
└── DESIGN.md           # This file
```

---

## 10. References

### Design Patterns
- **State Pattern:** Gamma et al., "Design Patterns"
- **Transition Tables:** Parnas & Madey, "Functional Documents for Computer Systems"

### Elevator Systems
- ASME A17.1 - Safety Code for Elevators (official standard)
- Real-world timing: 1-2 m/s^2 acceleration, 1-4 m/s descent

### Code Quality
- McConnell, "Code Complete" - State machine best practices
- Martin, "Clean Code" - Naming, function design

---

**Document Version:** 1.0  
**Last Updated:** 2026-06-15  
**Status:** Complete
