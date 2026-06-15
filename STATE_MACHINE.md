# State Machine - Complete Reference

## Overview

The elevator simulator uses a **Mealy state machine** with explicit transitions defined in a data-driven table. Each state represents a distinct mode of operation, and transitions occur based on conditions evaluated every simulation tick.

---

## State Definitions

### IDLE
**Purpose:** Elevator at rest at a floor, doors closed, waiting for requests.

**Entry conditions:** 
- State initialization
- Completed door opening sequence
- Door closing completed
- Stop button released

**Duration:** Variable (until button pressed or pending request detected)

**Actions on entry:**
- Tick counter resets
- No motion

**Transitions:**
| Condition | Next State | Notes |
|-----------|-----------|-------|
| `at_target_floor() && has_pending_buttons()` | DOORS_OPENING | Doors open if at requested floor |
| `should_go_up() && has_pending_buttons()` | SMOOTH_START_UP | Start upward motion |
| `should_go_down() && has_pending_buttons()` | SMOOTH_START_DOWN | Start downward motion |
| No conditions met | IDLE | Stay in place |

---

### SMOOTH_START_UP
**Purpose:** Smooth acceleration phase when starting upward motion.

**Duration:** 1 tick (100ms in real-time)

**Physical Model:** Simulates acceleration phase for passenger comfort and mechanical smoothness.

**Actions per tick:**
- No position change yet
- Increment internal tick counter
- Prepare for constant-speed motion

**Transitions:**
| Condition | Next State | Notes |
|-----------|-----------|-------|
| Tick counter >= 1 | MOVING_UP | Always after 1 tick (NULL condition) |

**Code Example:**
```c
{ SMOOTH_START_UP, MOVING_UP, NULL },  // Always transitions after 1 tick
```

---

### MOVING_UP
**Purpose:** Constant-speed upward motion between floors.

**Duration:** 1+ ticks (until current_floor == target_floor)

**Actions per tick:**
- `current_floor++` (move one floor per tick)
- Check if reached target

**Transitions:**
| Condition | Next State | Notes |
|-----------|-----------|-------|
| `current_floor == target_floor` | SMOOTH_STOP_UP | Start deceleration |
| Otherwise | MOVING_UP | Continue upward |

**Invariant:** Only moves from floor N to floor N+1 per tick (enforced by if statement in tick).

---

### SMOOTH_STOP_UP
**Purpose:** Smooth deceleration phase when stopping from upward motion.

**Duration:** 1 tick (100ms)

**Actions per tick:**
- No position change
- Prepare to stop

**Transitions:**
| Condition | Next State | Notes |
|-----------|-----------|-------|
| Tick counter >= 1 | IDLE | Always after 1 tick |

---

### SMOOTH_START_DOWN
**Purpose:** Smooth acceleration phase when starting downward motion.

**Duration:** 1 tick

**Actions per tick:**
- No position change
- Prepare for constant-speed downward motion

**Transitions:**
| Condition | Next State | Notes |
|-----------|-----------|-------|
| Tick counter >= 1 | MOVING_DOWN | Always after 1 tick |

---

### MOVING_DOWN
**Purpose:** Constant-speed downward motion between floors.

**Duration:** 1+ ticks

**Actions per tick:**
- `current_floor--` (move one floor per tick)
- Check if reached target

**Transitions:**
| Condition | Next State | Notes |
|-----------|-----------|-------|
| `current_floor == target_floor` | SMOOTH_STOP_DOWN | Start deceleration |
| Otherwise | MOVING_DOWN | Continue downward |

---

### SMOOTH_STOP_DOWN
**Purpose:** Smooth deceleration phase when stopping from downward motion.

**Duration:** 1 tick

**Transitions:**
| Condition | Next State | Notes |
|-----------|-----------|-------|
| Tick counter >= 1 | IDLE | Always after 1 tick |

---

### DOORS_OPENING
**Purpose:** Animation phase while doors slide open.

**Duration:** 2 ticks

**Physical Model:** Represents door mechanism opening (typical elevator: 2-3 seconds).

**Actions per tick:**
- Set `doors_open = true`
- Increment tick counter

**Transitions:**
| Condition | Next State | Notes |
|-----------|-----------|-------|
| Tick counter >= 2 | DOORS_OPEN | Animation complete |

---

### DOORS_OPEN
**Purpose:** Doors fully open at a floor, allowing passengers to board/exit.

**Duration:** 10 ticks (1 second in simulation, ~2-4 seconds in real elevators)

**Actions per tick:**
- Keep `doors_open = true`
- Clear button for this floor: `button_pressed[current_floor] = false`

**Transitions:**
| Condition | Next State | Notes |
|-----------|-----------|-------|
| No condition (always) | DOORS_CLOSING | After 10 ticks, always close |

**Safety Note:** Guardrails ensure motion cannot occur while in this state.

---

### DOORS_CLOSING
**Purpose:** Animation phase while doors slide closed.

**Duration:** 2 ticks

**Actions per tick:**
- Set `doors_open = false`
- Increment tick counter

**Transitions:**
| Condition | Next State | Notes |
|-----------|-----------|-------|
| Tick counter >= 2 | IDLE | Animation complete |

---

### EMERGENCY_STOP
**Purpose:** Elevator stopped by user pressing emergency stop button.

**Entry condition:** User presses 'S' key

**Exit condition:** User presses 'R' key to release stop

**Actions per tick:**
- Do nothing (hold current position)
- Maintain state

**Transitions:**
| Condition | Next State | Notes |
|-----------|-----------|-------|
| (special handling) | EMERGENCY_STOP | Only exited by release_stop() |

**Guardrail Protection:** 
- If in EMERGENCY_STOP while between floors, guardrail forces continuation to next floor before true stop.

---

## Complete State Transition Matrix

```
         IDLE  S_ST_UP  MOV_UP  S_STP_UP  S_ST_DN  MOV_DN  S_STP_DN  D_OPEN  D_OPNG  D_CLSG  EMERG
IDLE      -      [B]     [B]      -         [B]     [B]      -       [A]     -       -       -
S_ST_UP   -       -      [*]      -         -       -        -        -       -       -       -
MOV_UP    -       -       -      [A]        -       -        -        -       -       -       -
S_STP_UP [*]      -       -       -         -       -        -        -       -       -       -
S_ST_DN   -       -       -       -         -      [*]      -        -       -       -       -
MOV_DN    -       -       -       -         -       -      [A]       -       -       -       -
S_STP_DN [*]      -       -       -         -       -       -        -       -       -       -
D_OPEN    -       -       -       -         -       -       -        -       -      [*]      -
D_OPNG    -       -       -       -         -       -       -       [*]      -       -       -
D_CLSG   [*]      -       -       -         -       -       -        -       -       -       -
EMERG     -       -       -       -         -       -       -        -       -       -      [E]

Legend:
[*] = Unconditional transition (NULL condition, always next tick)
[A] = at_target_floor() condition
[B] = Button condition (should_go_up/should_go_down)
[E] = Emergency stop release required
 -  = No valid transition
```

---

## Transition Pseudocode

```
FUNCTION find_next_state(elevator):
    IF state == EMERGENCY_STOP:
        RETURN EMERGENCY_STOP  // Stay until released
    
    FOR EACH transition IN transition_table:
        IF transition.current == state:
            IF transition.condition == NULL:
                RETURN transition.next  // No condition, always go
            ELSE IF transition.condition(elevator):
                RETURN transition.next  // Condition true, transition
    
    RETURN state  // No valid transition, stay in current state
```

---

## Tick Sequence

Every simulation tick (100ms):

```
┌─────────────────────────────────────┐
│ 1. Check guardrails                │
│    (detect violations)             │
└────────────┬────────────────────────┘
             ↓
┌─────────────────────────────────────┐
│ 2. Render UI                        │
│    (show current state to user)     │
└────────────┬────────────────────────┘
             ↓
┌─────────────────────────────────────┐
│ 3. Read input                       │
│    (process button presses)         │
└────────────┬────────────────────────┘
             ↓
┌─────────────────────────────────────┐
│ 4. Enforce guardrails               │
│    (force corrective transitions)   │
└────────────┬────────────────────────┘
             ↓
┌─────────────────────────────────────┐
│ 5. Elevator tick:                   │
│    a) Find next state               │
│    b) If state changed, reset tick  │
│    c) Update position based on state│
│    d) Increment tick counter        │
└────────────┬────────────────────────┘
             ↓
┌─────────────────────────────────────┐
│ 6. Sleep 100ms                      │
└────────────┬────────────────────────┘
             ↓
          REPEAT
```

---

## Example Trace: Going from Floor 1 to Floor 3

```
T=0ms:    State=IDLE, Floor=1, Button=3 pressed
          Condition: should_go_up() = TRUE
          Transition: IDLE → SMOOTH_START_UP

T=100ms:  State=SMOOTH_START_UP, Floor=1
          Tick counter reaches 1
          Transition: SMOOTH_START_UP → MOVING_UP

T=200ms:  State=MOVING_UP, Floor=1
          Action: Floor++ → Floor=2
          Condition: at_target_floor() = FALSE (2 ≠ 3)
          No transition

T=300ms:  State=MOVING_UP, Floor=2
          Action: Floor++ → Floor=3
          Condition: at_target_floor() = TRUE (3 = 3)
          Transition: MOVING_UP → SMOOTH_STOP_UP

T=400ms:  State=SMOOTH_STOP_UP, Floor=3
          Tick counter reaches 1
          Transition: SMOOTH_STOP_UP → IDLE

T=500ms:  State=IDLE, Floor=3
          Condition: at_target_floor() = TRUE
          Transition: IDLE → DOORS_OPENING

T=600ms:  State=DOORS_OPENING, Floor=3
          Tick counter=1
          No transition yet

T=700ms:  State=DOORS_OPENING, Floor=3
          Tick counter=2 (reaches threshold)
          Transition: DOORS_OPENING → DOORS_OPEN

T=800ms:  State=DOORS_OPEN, Floor=3
          Action: Clear button_pressed[3]
          (wait 10 ticks with doors open)
          ...

T=1800ms: State=DOORS_OPEN, Floor=3
          Tick counter reaches 10
          Transition: DOORS_OPEN → DOORS_CLOSING

T=1900ms: State=DOORS_CLOSING, Floor=3
          Tick counter=1
          ...

T=2000ms: State=DOORS_CLOSING, Floor=3
          Tick counter=2
          Transition: DOORS_CLOSING → IDLE

T=2100ms: State=IDLE, Floor=3
          No pending buttons
          Stay in IDLE

Total time: ~2.1 seconds from request to doors closed
```

---

## Condition Functions

### `at_target_floor()`
```c
bool at_target_floor(const Elevator *elev) {
    return elev->current_floor == elev->target_floor;
}
```
Used by: MOVING_UP → SMOOTH_STOP_UP, MOVING_DOWN → SMOOTH_STOP_DOWN, IDLE → DOORS_OPENING

### `should_go_up()`
```c
bool should_go_up(const Elevator *elev) {
    if (!has_pending_buttons(elev)) return false;
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
Used by: IDLE → SMOOTH_START_UP

Finds highest floor button pressed above current position.

### `should_go_down()`
```c
bool should_go_down(const Elevator *elev) {
    if (!has_pending_buttons(elev)) return false;
    Floor lowest = FLOOR_5;
    for (int i = FLOOR_1; i <= FLOOR_5; i++) {
        if (elev->button_pressed[i]) {
            lowest = (Floor)i;
            break;
        }
    }
    return lowest < elev->current_floor;
}
```
Used by: IDLE → SMOOTH_START_DOWN

Finds lowest floor button pressed below current position.

### `doors_fully_open()` / `doors_fully_closed()`
```c
bool doors_fully_open(const Elevator *elev) {
    return elev->ticks >= DOOR_ANIM_TICKS;  // 2 ticks
}

bool doors_fully_closed(const Elevator *elev) {
    return elev->ticks >= DOOR_ANIM_TICKS;  // 2 ticks
}
```
Used by: DOORS_OPENING → DOORS_OPEN, DOORS_CLOSING → IDLE

---

## State Machine Properties

### Deterministic
- Given current state and conditions, next state is always the same
- No randomness or non-determinism

### Complete
- Every state has at least one outgoing transition (including self-loop)
- No dead ends

### Reachable
- All 11 states are reachable from IDLE
- No unreachable states

### Safe
- No invalid transitions
- Guardrai enforces physical constraints

---

## Adding a New State

To add a new state (e.g., `MAINTENANCE_MODE`):

1. **Add enum value:**
   ```c
   typedef enum {
       IDLE,
       // ...
       MAINTENANCE_MODE,    // NEW
       EMERGENCY_STOP,
   } ElevatorState;
   ```

2. **Define transitions:**
   ```c
   static const StateTransition transition_table[] = {
       // From MAINTENANCE_MODE
       { MAINTENANCE_MODE, IDLE, maintenance_complete },  // NEW
       
       // To MAINTENANCE_MODE
       { IDLE, MAINTENANCE_MODE, user_pressed_test },     // NEW
   };
   ```

3. **Add condition function:**
   ```c
   static bool maintenance_complete(const Elevator *elev) {
       return /* condition */;
   }
   ```

4. **Add name:**
   ```c
   case MAINTENANCE_MODE: return "MAINTENANCE_MODE";
   ```

5. **Handle in tick (if needed):**
   ```c
   switch (elev->state) {
       case MAINTENANCE_MODE:
           // Special behavior
           break;
   }
   ```

---

## References

- **Harel Statecharts:** State hierarchy (not used here, but good reference)
- **UML State Machines:** Similar notation and concepts
- **Executable Specification:** State table IS the spec

---

**Document Version:** 1.0  
**Created:** 2026-06-15
