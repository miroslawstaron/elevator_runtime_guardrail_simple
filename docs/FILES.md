# Project Files - Complete Reference

## Source Code Files

### `elevator.h` (123 lines)
**Purpose:** Header file defining elevator state machine types and public API.

**Key Types:**
- `ElevatorState` enum: 11 states (IDLE, MOVING_UP, EMERGENCY_STOP, etc.)
- `Floor` enum: Floors 1-5
- `Elevator` struct: Complete state representation

**Public Functions:**
- `elevator_init()` - Initialize elevator at floor 1
- `elevator_press_button()` - Register floor request
- `elevator_press_stop()` / `elevator_release_stop()` - Emergency control
- `elevator_tick()` - Main simulation step
- `elevator_state_to_string()` - Debug/display function
- `elevator_is_between_floors()` - Check if in motion
- `elevator_get_position()` - Get current floor
- `elevator_open_doors()` / `elevator_close_doors()` - Manual door control

**Design Notes:**
- No global state
- All functions take Elevator pointer
- State information complete and self-contained

---

### `elevator.c` (250 lines)
**Purpose:** Core state machine implementation with transition logic.

**Key Sections:**

1. **State Transition Table** (lines 10-25)
   - Data-driven definition of all valid transitions
   - Condition functions as pointers
   - Easy to visualize and modify entire state machine

2. **Initialization** (lines 47-56)
   - Sets up fresh elevator at floor 1
   - Clears all button presses
   - Initial state: IDLE

3. **Button Management** (lines 58-83)
   - `elevator_press_button()` - Queues a floor request
   - Validates floor range (1-5)
   - Sets target floor if idle

4. **Condition Functions** (lines 95-140)
   - `at_target_floor()` - Check if reached destination
   - `should_go_up()` - Find highest pending button above current
   - `should_go_down()` - Find lowest pending button below current
   - `doors_fully_open()` / `doors_fully_closed()` - Door timing checks
   - `has_pending_buttons()` - Any buttons pressed?

5. **State Transition Logic** (lines 165-180)
   - `find_next_state()` - Main state machine evaluation
   - Evaluates all transitions in table
   - Applies condition functions
   - Stays in state if no valid transition

6. **Elevator Tick** (lines 182-230)
   - Updates elevator based on current state
   - Position changes in MOVING_UP/MOVING_DOWN states
   - Door state management
   - Button clearing when served

**Design Patterns:**
- Transition table for explicit state definition
- Condition functions for decoupled logic
- Pure functions (no side effects except state mutation)

---

### `guardrails.h` (25 lines)
**Purpose:** Header file defining safety guardrail API.

**Key Types:**
- `GuardrailStatus` struct: Reports all current violations and conditions
  - `doors_open_violation` - Doors open while moving
  - `between_floors_violation` - Stop pressed mid-motion
  - `stop_button_pressed` - Current stop state
  - `seconds_stopped` - Duration of stop
  - `needs_user_prompt` - 20-second interval reached

**Public Functions:**
- `guardrail_check()` - Detect all violations (read-only)
- `guardrail_enforce()` - Apply corrective actions
- `guardrail_violation_description()` - Get human-readable message

**Design Philosophy:**
- Guardrails are defensive (assume bugs exist)
- Check and enforce are separate functions
- Status structure for decoupling

---

### `guardrails.c` (100 lines)
**Purpose:** Safety enforcement implementation.

**Key Sections:**

1. **Guardrail Checks** (lines 5-40)
   - **Guardrail 1:** Doors open detection
   - **Guardrail 2:** Between-floors stop detection
   - **Guardrail 3:** Stop button timing (20-second intervals)
   - No side effects, returns complete status

2. **Guardrail Enforcement** (lines 42-68)
   - **Enforce 1:** Immediate stop if doors open while moving
   - **Enforce 2:** Force continuation to next floor if stop mid-motion
   - Both applied after check phase

3. **Message Generation** (lines 70-90)
   - Human-readable violation descriptions
   - Stop button timing message
   - Used for UI display

**Enforcement Order:**
1. Run `guardrail_check()` (read-only)
2. Render UI (shows warnings)
3. Read input (user actions)
4. Run `guardrail_enforce()` (corrects violations)
5. Run `elevator_tick()` (state machine)

---

### `main.c` (280 lines)
**Purpose:** Text-based user interface and simulation control loop.

**Key Sections:**

1. **Cross-Platform Support** (lines 8-18)
   - Windows: Uses `_kbhit()`, `_getch()`, `cls`
   - Linux/macOS: Uses `clear`
   - Unified via `#ifdef _WIN32`

2. **Display Functions** (lines 30-130)
   - `draw_elevator_cabin()` - ASCII art of elevator shaft with positions
   - `draw_status()` - Status panel (floor, state, buttons, stop)
   - `draw_warnings()` - Guardrail violation warnings
   - `draw_help()` - Help screen with controls

3. **Input Handling** (lines 20-30)
   - `get_nonblocking_input()` - Non-blocking keyboard read
   - Supports buttons 1-5, S (stop), R (resume), W (wait), H (help), Q (quit)

4. **Main Simulation Loop** (lines 155-215)
   - Initialization
   - 100ms tick loop (10 Hz)
   - Check → Render → Input → Enforce → Tick → Sleep
   - Clean shutdown on 'Q'

**UI Features:**
- Real-time cabin visualization
- Floor indicator with button status
- Live state display
- Warning messages
- Colored output (if terminal supports)

---

## Documentation Files

### `README.md` (400+ lines)
**Audience:** Users, developers new to project

**Covers:**
- Project overview and architecture diagram
- How to build (Linux, macOS, Windows)
- Module descriptions with design rationales
- State machine diagram
- Guardrail examples
- Code design principles
- Testing scenarios
- Performance metrics
- Future enhancements
- References

**Key Sections:**
- Module responsibilities
- State machine overview
- Guardrail examples (3 scenarios)
- Design principles (7 principles explained)
- Extensibility roadmap

---

### `DESIGN.md` (600+ lines)
**Audience:** Developers implementing new features, architects

**Covers:**
- Architecture overview (detailed)
- Control flow diagram
- State machine detailed design
- Transition table explanation
- Transition conditions
- Motion simulation
- Guardrails deep dive (philosophy, enforcement order, extensibility)
- Design principles with examples
- Testing strategy
- Performance analysis
- Extensibility roadmap
- Known limitations
- References

**Key Features:**
- State definitions with tables
- Guardrail enforcement sequences
- Extensibility patterns
- Performance characteristics (time/space complexity)
- Roadmap for future features

---

### `QUICK_START.md` (200 lines)
**Audience:** New users wanting to run immediately

**Covers:**
- Compilation (3 methods: make, PowerShell, manual)
- Running the simulator
- Interactive controls
- Common scenarios (4 examples)
- Understanding the display
- State indicators
- Guardrail examples (3 scenarios)
- Tips & tricks
- Troubleshooting
- Learning path

**Purpose:** Get from 0 to running simulation in 2 minutes.

---

### `STATE_MACHINE.md` (400+ lines)
**Audience:** Developers understanding or modifying state logic

**Covers:**
- Complete state definitions (11 states)
- Each state with purpose, duration, transitions, invariants
- State transition matrix (complete reference)
- Transition pseudocode
- Tick sequence diagram
- Detailed example trace (floor 1→3)
- Condition function specifications
- State machine properties (deterministic, complete, reachable, safe)
- How to add new states
- References to state machine theory

**Key Tables:**
- State transition matrix (11×11)
- Condition functions with examples
- Transition specifications per state

---

### `FILES.md` (this file)
**Purpose:** Complete file reference and navigation guide.

---

## Build Configuration Files

### `Makefile` (25 lines)
**Purpose:** Unix/Linux/macOS build automation.

**Targets:**
- `all` (default) - Build executable
- `run` - Build and run
- `clean` - Remove build artifacts
- `rebuild` - Clean and build

**Usage:**
```bash
make              # Build
make run          # Run
make clean        # Clean
make rebuild      # Fresh build
```

---

### `build.ps1` (15 lines)
**Purpose:** Windows PowerShell build script.

**Features:**
- Compiles with gcc
- Shows compilation command
- Reports success/failure
- No external dependencies

**Usage:**
```powershell
.\build.ps1       # Build
.\elevator_sim.exe # Run
```

---

## File Dependencies

```
main.c
├─ elevator.h
│  ├─ time.h (stdlib)
│  └─ stdbool.h (stdlib)
├─ guardrails.h
│  └─ elevator.h
└─ [platform headers]
   ├─ windows.h (Windows)
   ├─ conio.h (Windows)
   └─ termios.h (Unix)

elevator.c
├─ elevator.h
├─ string.h (stdlib)

guardrails.c
├─ guardrails.h
├─ elevator.h
├─ time.h (stdlib)
├─ stdio.h (stdlib)
└─ string.h (stdlib)
```

---

## Code Statistics

| File | Lines | Type | Purpose |
|------|-------|------|---------|
| elevator.h | 123 | Header | Type definitions, API |
| elevator.c | 250 | Implementation | State machine |
| guardrails.h | 25 | Header | Safety API |
| guardrails.c | 100 | Implementation | Safety enforcement |
| main.c | 280 | Implementation | UI and control |
| **Total Code** | **778** | | |
| README.md | 400+ | Documentation | Overview |
| DESIGN.md | 600+ | Documentation | Architecture |
| QUICK_START.md | 200 | Documentation | Getting started |
| STATE_MACHINE.md | 400+ | Documentation | State reference |
| FILES.md | 200+ | Documentation | This file |
| **Total Docs** | **1800+** | | |

**Total Project:** ~2600 lines (inclusive of docs and code)

---

## Directory Structure

```
elevator_guardrail/
│
├─ Source Code (5 files)
│  ├─ elevator.h
│  ├─ elevator.c
│  ├─ guardrails.h
│  ├─ guardrails.c
│  └─ main.c
│
├─ Build Configuration (2 files)
│  ├─ Makefile
│  └─ build.ps1
│
├─ Documentation (5 files)
│  ├─ README.md
│  ├─ DESIGN.md
│  ├─ QUICK_START.md
│  ├─ STATE_MACHINE.md
│  └─ FILES.md
│
└─ Compiled Output (when built)
   └─ elevator_sim[.exe]
```

---

## Reading Guide

### For Users
1. **QUICK_START.md** - Get it running in 2 minutes
2. **README.md** - Understand features and controls
3. Try the simulator!

### For Developers
1. **README.md** - Module overview
2. **DESIGN.md** - Architecture and patterns
3. **STATE_MACHINE.md** - State definitions
4. **elevator.h / elevator.c** - State machine code
5. **guardrails.c** - Safety logic
6. **main.c** - UI implementation

### For Modifiers
1. **DESIGN.md** Section 7 - Extensibility patterns
2. **STATE_MACHINE.md** - "Adding a New State" section
3. Study existing code as template
4. Run tests after changes

### For Debuggers
1. **DESIGN.md** Section 3 - Guardrails
2. **STATE_MACHINE.md** - Complete trace example
3. Add `printf()` statements in state transitions
4. Watch `state` and `current_floor` variables

---

## How to Use This Reference

**I want to...** → Read this file:

- Run the simulator → QUICK_START.md
- Understand how it works → README.md
- Modify/extend the code → DESIGN.md
- Understand state machine → STATE_MACHINE.md
- Find specific function → FILES.md (this file)
- Add a new feature → DESIGN.md Section 7
- Fix a bug → DESIGN.md Section 5 (testing), then trace through STATE_MACHINE.md

---

## File Edit Checklist

When modifying files:

| File | Requires Header Change? | Requires Test? | Requires Doc Update? |
|------|------------------------|-----------------|----------------------|
| elevator.c | Maybe (elevator.h) | Yes | Yes (STATE_MACHINE.md) |
| guardrails.c | Maybe (guardrails.h) | Yes | Yes (DESIGN.md) |
| main.c | No | Yes | Yes (QUICK_START.md) |
| elevator.h | N/A | Yes | Yes (README.md) |
| guardrails.h | N/A | Yes | Yes (README.md) |

---

**Document Version:** 1.0  
**Created:** 2026-06-15  
**Last Updated:** 2026-06-15
