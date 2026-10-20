# Elevator Simulator with State Machine & Guardrails

A comprehensive C implementation of an elevator control system featuring a state machine architecture with guardrails for safe operation.

## New: Elevator Lab web interface

Operate the elevator from a responsive Metriczone dashboard using the supplied logo and a deep teal measurement identity. Add named passengers and groups on different floors, watch shared trips and intermediate stops, and use cabin, door, stop, and resume controls. **The entire backend remains C**, including the HTTP server, clock, passenger dispatch, original state machine, and original guardrails. The frontend is dependency-free HTML/CSS/JavaScript.

```powershell
.\run-web.ps1 -Port 8090 -OpenBrowser
.\build.ps1 -Test -Terminal
```

For direct network access, open <http://lazythought.cse.chalmers.se:8099/>. The Windows launch scripts read `lab-network.json`, which saves the all-interface binding and allowed DNS hostname. Run `tools/enable-lab-firewall.ps1` in an administrator PowerShell to allow TCP 8099 on all network profiles, matching the 5G and SDV dashboards.

To share the C simulator over the internet, run `.\run-public.ps1`. It checks local port **8099**, starts the C backend and an HTTPS tunnel in the background, and prints the public URL. Keep this computer awake and online. Stop with `.\stop-public.ps1`; restarting assigns a new public URL. Use `.\tools\restart-public-server.ps1` to restart only the C server with the saved network settings while keeping the current tunnel URL. See [public access setup](docs/WEB_INTERFACE.md#share-over-the-internet) and [deployment diagrams](docs/evolution/03-public-access.md).

On Linux/macOS: `make web`, then `./elevator_web --port 8090`. Open <http://localhost:8090>. Read [web setup, API, and testing](docs/WEB_INTERFACE.md), [before diagrams](docs/evolution/01-before.md), [after diagrams](docs/evolution/02-after.md), or the [rendered diagram gallery](docs/evolution/diagrams.html).

## Overview

Use **Usage scenarios** to run a 300-person busy morning or afternoon, occasional travellers, VIP traffic, or custom timed arrival streams. Configure counts, timing, routes and priorities; use up to 20× playback and retain each experiment's telemetry. In **Telemetry & experiments**, **Analyze telemetry & suggest improvements** produces an evidence-based steering report for the selected run. See [scenario instructions and API examples](docs/SCENARIOS.md) and [stage 10 architecture and sequences](docs/evolution/10-traffic-scenarios-and-analysis.md).

The C backend now includes [embedded telemetry and passenger feedback](docs/TELEMETRY.md). It records elevator samples at 10 Hz, durable events, passenger journey timings, and textual feedback with optional ratings in `data/elevator.sqlite3`. The web **Telemetry & experiments** section provides named runs, historical queries, filters, pagination, JSON/CSV exports, and feedback forms for live or archived passengers. History survives resets and restarts; no separate database service is needed. See the [telemetry architecture and sequences](docs/evolution/04-telemetry.md).

Open **System evolution** in the web sidebar to trace every implementation stage through architecture and sequence diagrams, enlarge/zoom drawings, and download SVG or editable Mermaid sources. The ordered [stage registry](docs/evolution/stages.json) drives both the web timeline and offline gallery. Each future meaningful change must append a new documented stage, regenerate the assets, and pass `node tools/check-evolution.mjs`; the standing workflow is recorded in [AGENTS.md](AGENTS.md).

This project demonstrates solid software engineering principles by separating concerns into independent modules:

### Terminal baseline architecture

This original terminal layout is preserved for reference. Current architecture and interaction sequences are available in the web **System evolution** timeline and [diagram gallery](docs/evolution/diagrams.html).

```
┌─────────────────────────────────────────────┐
│           main.c (GUI & Control)            │
├─────────────────────────────────────────────┤
│  elevator.c (State Machine Implementation)  │
│  guardrails.c (Safety Enforcement)          │
├─────────────────────────────────────────────┤
│      Simulation Engine (100ms ticks)        │
└─────────────────────────────────────────────┘
```

## Building

### On Linux/macOS:
```bash
make              # Build the simulator
make run          # Build and run
make clean        # Clean build artifacts
```

### On Windows (PowerShell):
```powershell
.\build.ps1 -Terminal                   # Build web and original terminal simulators
.\build\elevator_guardrails.exe         # Run original terminal UI
.\run-web.ps1 -Port 8090                # Run web interface
```

### Manual compilation:
```bash
gcc -Wall -Wextra -std=c99 -O2 -Iinclude -o elevator_sim src/main.c src/elevator.c src/guardrails.c
```

## Module Descriptions

### `elevator.h / elevator.c` - Core Elevator Logic
**Responsibility:** Elevator state machine and motion simulation

**Key Components:**
- **State Machine:** 11 states modeling elevator behavior
  - `IDLE`: Waiting at a floor with doors closed
  - `SMOOTH_START_{UP|DOWN}`: Acceleration phase (smooth motion)
  - `MOVING_{UP|DOWN}`: Constant velocity between floors
  - `SMOOTH_STOP_{UP|DOWN}`: Deceleration phase
  - `DOORS_{OPENING|OPEN|CLOSING}`: Door operation states
  - `EMERGENCY_STOP`: Stop button pressed

- **Transition Table:** Data-driven state transitions with condition functions
  - Allows easy visualization of all valid state transitions
  - Conditions are function pointers for flexibility
  - No complex nested if-else chains

- **Features:**
  - Buttons can be pressed while moving
  - Elevator services buttons in order
  - Smooth acceleration/deceleration with separate animation ticks
  - Door operation integrated into state machine

**Design Decisions:**
- Separate smooth start/stop states ensure realistic motion
- Transition table makes the state machine explicit and testable
- No global state—all state encapsulated in `Elevator` struct

### `guardrails.h / guardrails.c` - Safety Enforcement
**Responsibility:** Preventing unsafe operating conditions

**Guardrails Implemented:**

1. **No stopping between floors**
   - If stop is triggered mid-motion, elevator continues to next floor
   - Prevents elevator from becoming stuck between levels

2. **Doors open → immediate halt**
   - Checks every tick if doors are open while moving
   - Forces state transition to `SMOOTH_STOP` immediately
   - Safety: prevents injury from closing doors on passengers

3. **Stop button prompt every 20 seconds**
   - Tracks time elapsed since stop was pressed
   - Prompts user every 20 seconds if still stopped
   - User can resume ('R') or wait ('W')

4. **Floor bounds validation**
   - Detects out-of-range `current_floor` / `target_floor` values
   - Clamps values to safe limits (`FLOOR_0` to `FLOOR_9`)
   - Prevents invalid state propagation after unexpected corruption

**Design Decisions:**
- Guardrails run after state machine update
- Status structure allows caller to decide action
- Non-invasive: guardrails don't create side effects, only report/enforce

### `main.c` - User Interface
**Responsibility:** Text-based GUI and user interaction

**Features:**
- Real-time visual representation of elevator cabin
- Floor indicator (F1-F5)
- Button status display (● = pressed, ○ = not pressed)
- Live status panel (current floor, state, doors, stop button)
- Colored warnings for guardrail violations

**Controls:**
```
[1-5]  - Request floor
[S]    - Emergency stop
[R]    - Resume after stop
[W]    - Acknowledge prompt (wait)
[H]    - Help screen
[Q]    - Quit
```

**Design Decisions:**
- Non-blocking input: responds to user while simulation continues
- Clear visual layout: cabins on left, status on right
- Cross-platform: uses `#ifdef _WIN32` for compatibility

## State Machine Transition Diagram

```
                    ┌─────────────┐
                    │    IDLE     │
                    └──────┬──────┘
                           │
                ┌──────────┼──────────┐
                │          │          │
                ▼          ▼          ▼
          SMOOTH_    DOORS_     SMOOTH_
          START_UP   OPENING    START_DN
            │          │          │
            │          │          │
            ▼          ▼          ▼
          MOVING_    DOORS_     MOVING_
            UP        OPEN        DOWN
            │          │          │
            └──────┬───┘          │
                   │              │
                   ▼              ▼
             SMOOTH_STOP_   SMOOTH_STOP_
               UP ◄─────────→ DOWN
                   │
                   ▼
                 IDLE
```

## Guardrail Examples

### Example 1: Preventing stops between floors
```
User presses [S] while elevator is moving.
→ Elevator continues to next floor instead of stopping mid-motion.
→ Prevents mechanical failure (no energy dissipation between floors).
```

### Example 2: Doors open safety
```
Doors open while MOVING_UP occurs (shouldn't happen, but guardrail catches it).
→ Elevator transitions to SMOOTH_STOP_UP immediately.
→ Prevents injury from door closure during motion.
```

### Example 3: Stop button timing
```
User presses [S] at time T=0.
T=19s: Elevator remains stopped. No prompt.
T=20s: Prompt appears: "Press R to resume or W to wait"
T=40s: Prompt appears again.
User presses [R]: Stop button released, elevator resumes.
```

## Code Design Principles Applied

1. **Separation of Concerns**
   - `elevator.c`: Pure simulation logic
   - `guardrails.c`: Safety logic
   - `main.c`: User interaction only

2. **Data-Driven Design**
   - Transition table makes state machine explicit
   - Easy to add new states or transitions
   - Condition functions encapsulate logic

3. **No Global State**
   - All state in `Elevator` struct
   - Supports multiple elevators (easy extension)
   - Thread-safe function signatures

4. **Defensive Programming**
   - Guardrails run independently
   - Type-safe state enums
   - Bounds checking on floors (1-5)

5. **Clear Naming**
   - State names describe behavior (e.g., `SMOOTH_START_UP` not `STATE_3`)
   - Function names are verb-noun pairs
   - Comments explain "why", not "what"

6. **Testability**
   - Pure functions with no side effects
   - Guardrails independent of elevator logic
   - Easy to add unit tests

## Extensibility

### Adding a new state:
1. Add to `ElevatorState` enum in `elevator.h`
2. Add transitions to `transition_table[]` in `elevator.c`
3. Update `elevator_state_to_string()` for display

### Adding a new guardrail:
1. Add check to `guardrail_check()` in `guardrails.c`
2. Update `GuardrailStatus` struct if new data needed
3. Add description in `guardrail_violation_description()`

### Adding multiple elevators:
1. Create array of `Elevator` structs
2. Call `elevator_tick()` for each
3. Coordinate button requests between elevators

## Testing Scenarios

1. **Basic movement:**
   - Press [3], observe smooth acceleration, movement, deceleration
   
2. **Multiple requests:**
   - Press [5], then [2], then [4]
   - Elevator should service in order (1→2→3→4→5 on first pass)
   
3. **Emergency stop:**
   - Press [4], then immediately press [S] while moving
   - Elevator should stop at next floor (not between)
   
4. **Stop button timeout:**
   - Press [S]
   - Wait 20 seconds and confirm prompt appears
   - Press [W] to wait or [R] to resume

5. **Edge cases:**
   - Press [1] when already at floor 1 (doors should open)
   - Spam buttons (should queue correctly)
   - Stop then immediately resume (should work smoothly)

## Performance

- **Tick rate:** 10 Hz (100ms per simulation step)
- **State transitions:** O(n) where n = number of transitions (~12)
- **Guardrail checks:** O(1) per check
- **Memory:** ~200 bytes per elevator instance

## Future Enhancements

- [ ] Multiple elevators with separate queues
- [ ] Priority requests (VIP/emergency floors)
- [ ] Load simulation (weight affects speed)
- [ ] Persistence (save/load state)
- [ ] Networking (distributed control)
- [ ] Energy optimization (group requests efficiently)

## References

State machine design pattern from:
- Gamma et al., "Design Patterns: Elements of Reusable Object-Oriented Software"
- McConnell, "Code Complete" (Chapter on state machines)

---

**Author:** Elevator Simulation System
**Version:** 1.0
**License:** Educational use
