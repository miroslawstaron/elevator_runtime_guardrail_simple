# Elevator Safety Guardrails

## Overview

Guardrails are safety mechanisms that prevent dangerous or invalid elevator states. They detect violations and enforce corrective actions to keep the elevator system safe and operational.

## What Are Guardrails?

Guardrails are **reactive safety checks** that:
1. **Detect** potentially dangerous elevator states
2. **Enforce** corrective actions immediately
3. **Track** emergency conditions with timing information
4. **Report** violations to the user/system

The guardrails system works alongside the main state machine to provide an additional layer of safety protection.

## Violations Detected

### 1. Doors Open While Moving ⚠️

**Description:** The most critical safety violation. Doors are open while the elevator is in motion.

**When Detected:**
- Doors are open (`elev->doors_open == true`)
- AND elevator is between floors (MOVING_UP, MOVING_DOWN, SMOOTH_START_*, SMOOTH_STOP_*)

**Example Scenarios:**
```
State: MOVING_UP,  doors_open: true  → VIOLATION
State: MOVING_DOWN, doors_open: true  → VIOLATION
State: SMOOTH_START_UP, doors_open: true → VIOLATION
State: SMOOTH_STOP_DOWN, doors_open: true → VIOLATION
State: IDLE, doors_open: true → NO VIOLATION (at floor)
State: DOORS_OPENING, doors_open: true → NO VIOLATION (state handles it)
```

**Enforcement Action:**
- Immediately set state to `IDLE`
- Closes doors and stops motion
- System transitions to smooth stop on next tick if needed

**Why It Matters:**
- Passengers could fall from open doors during motion
- Safety hazard for people on the elevator or outside
- Violates elevator safety codes

### 2. Emergency Stop Between Floors 🛑

**Description:** Elevator emergency stop button pressed while elevator is between floors.

**When Detected:**
- `elev->stop_button_pressed == true`
- AND elevator is between floors (detected via `elevator_is_between_floors()`)
- AND state is `EMERGENCY_STOP`

**Example Scenarios:**
```
Stop pressed at FLOOR_3 → SAFE (at floor, no violation)
Stop pressed while MOVING_UP between floors → VIOLATION
Stop pressed while SMOOTH_START_DOWN → VIOLATION
```

**Enforcement Action:**
- If stopped in lower half (current_floor < FLOOR_5):
  - Set target_floor to `current_floor + 1`
  - Resume moving UP to nearest floor
- If stopped in upper half (current_floor >= FLOOR_5):
  - Set target_floor to `current_floor - 1`
  - Resume moving DOWN to nearest floor

**Why It Matters:**
- Cannot safely stop elevator between floors
- People trapped between floors during stop
- Forces movement to nearest safe stop point (at a floor)

### 3. Stop Button Active (Informational) ℹ️

**Description:** Tracks when emergency stop button is actively pressed.

**When Detected:**
- `elev->stop_button_pressed == true`

**Information Tracked:**
- `seconds_stopped`: Time elapsed since stop button pressed
- `needs_user_prompt`: User should be prompted every 20 seconds

**User Prompt Timing:**
```
Stop pressed at t=0s
  t=20s → "Prompt user"
  t=40s → "Prompt user"
  t=60s → "Prompt user"
  ...
```

**Why It Matters:**
- Operators need to know how long elevator has been stopped
- Safety protocol requires periodic checks on stopped elevators
- Helps diagnose if stop button is stuck

## Guardrails API

### `guardrail_check(const Elevator *elev)`

**Purpose:** Detect all current violations without enforcing

**Returns:** `GuardrailStatus` struct with:
- `doors_open_violation` - Doors open while moving
- `between_floors_violation` - Stopped between floors
- `stop_button_pressed` - Stop button is active
- `seconds_stopped` - How long stopped (if applicable)
- `needs_user_prompt` - Should prompt user now

**Example Usage:**
```c
GuardrailStatus status = guardrail_check(&elevator);

if (status.doors_open_violation) {
    printf("CRITICAL: Doors open while moving!\n");
}

if (status.needs_user_prompt) {
    printf("Elevator stopped for %d seconds. Resume? (R/W)\n", 
           status.seconds_stopped);
}
```

### `guardrail_enforce(Elevator *elev)`

**Purpose:** Enforce guardrails by modifying elevator state

**Actions Taken:**
1. If doors open violation detected:
   - Set state to IDLE or force smooth stop
   - Closes doors and halts motion
2. If between floors violation detected:
   - Force move to nearest floor
   - Resume normal motion

**Example Usage:**
```c
GuardrailStatus status = guardrail_check(&elevator);
if (status.doors_open_violation || status.between_floors_violation) {
    guardrail_enforce(&elevator);  // Fix the violation
}
```

### `guardrail_violation_description(const GuardrailStatus *status)`

**Purpose:** Generate human-readable error message

**Returns:** String describing violation(s), or NULL if no violations

**Example Output:**
```
"VIOLATION: Doors open while moving! Force stop.\n"
"VIOLATION: Cannot stop between floors. Continuing to next floor.\n"
"Elevator stopped for 20 seconds. Press 'R' to resume or 'W' to wait.\n"
```

## GuardrailStatus Structure

```c
typedef struct {
    bool doors_open_violation;      // Attempting to move while doors open
    bool between_floors_violation;  // Attempting to stop between floors
    bool stop_button_pressed;       // Stop button is currently active
    int seconds_stopped;            // Seconds elapsed since stop pressed
    bool needs_user_prompt;         // User should be prompted (every 20s)
} GuardrailStatus;
```

## Typical Usage Pattern

```c
// Main elevator loop
while (running) {
    // Check for violations
    GuardrailStatus status = guardrail_check(&elevator);
    
    // Display any violations
    const char *violation_msg = guardrail_violation_description(&status);
    if (violation_msg) {
        printf("%s", violation_msg);
    }
    
    // Enforce guardrails
    guardrail_enforce(&elevator);
    
    // Run normal elevator logic
    elevator_tick(&elevator);
    
    // Handle user prompts
    if (status.needs_user_prompt && status.stop_button_pressed) {
        // Prompt user for action
    }
}
```

## Safety Scenarios

### Scenario 1: Doors Open During Motion

```
User presses button to floor 5
Elevator starts MOVING_UP
[BUG] Doors open accidentally while between floors

CHECK: doors_open=true, state=MOVING_UP
VIOLATION DETECTED: Doors open while moving!

ENFORCE: 
  - Set state to IDLE
  - Next tick: smooth stop
  - Doors close
  - Motion halts
```

### Scenario 2: Emergency Stop Between Floors

```
Elevator MOVING_UP between floor 3 and 4
Panic button pressed

STATE: EMERGENCY_STOP, current_floor=3 (between 3 and 4)
VIOLATION: Cannot stop between floors!

ENFORCE:
  - Since floor 3 < FLOOR_5 (lower half)
  - Set target_floor = 4
  - Set state = MOVING_UP
  - Continue to floor 4, then stop safely
```

### Scenario 3: Long Emergency Stop

```
Stop button pressed at t=0
t=20s: PROMPT USER
       "Elevator stopped for 20 seconds. Resume? (R/W)"
t=40s: PROMPT USER
       "Elevator stopped for 40 seconds. Resume? (R/W)"
t=60s: PROMPT USER
       "Elevator stopped for 60 seconds. Resume? (R/W)"
```

### Scenario 4: Safe Door Opening at Floor

```
Elevator at FLOOR_3, state=IDLE
Doors open=true

CHECK: doors_open=true, state=IDLE
NO VIOLATION: At floor, not moving
Safe to have doors open
```

## Between Floors Detection

The `elevator_is_between_floors()` function identifies motion states:

**Between Floors States:**
- `MOVING_UP` - Moving toward higher floor
- `MOVING_DOWN` - Moving toward lower floor  
- `SMOOTH_START_UP` - Acceleration phase upward
- `SMOOTH_START_DOWN` - Acceleration phase downward
- `SMOOTH_STOP_UP` - Deceleration phase upward
- `SMOOTH_STOP_DOWN` - Deceleration phase downward

**At Floor States:**
- `IDLE` - Stopped at floor
- `DOORS_OPENING` - Opening doors at floor
- `DOORS_OPEN` - Doors fully open at floor
- `DOORS_CLOSING` - Closing doors at floor
- `EMERGENCY_STOP` - Stopped in emergency (special case)

## Implementation Details

### Violation Check Logic

```c
// Check 1: Doors open and moving
if (elev->doors_open && elevator_is_between_floors(elev)) {
    status.doors_open_violation = true;
}

// Check 2: Stopped between floors
if (elevator_is_between_floors(elev) && elev->state == EMERGENCY_STOP) {
    status.between_floors_violation = true;
}

// Check 3: Emergency stop timing
if (elev->stop_button_pressed) {
    status.stop_button_pressed = true;
    time_t now = time(NULL);
    status.seconds_stopped = (int)(now - elev->stop_start_time);
    
    // Prompt every 20 seconds
    if (status.seconds_stopped > 0 && 
        status.seconds_stopped % 20 == 0) {
        status.needs_user_prompt = true;
    }
}
```

### Enforcement Logic

```c
void guardrail_enforce(Elevator *elev) {
    GuardrailStatus status = guardrail_check(elev);
    
    // ENFORCE: Doors open violation
    if (status.doors_open_violation) {
        elev->state = IDLE;
        // Let state machine move to smooth stop on next tick
        if (elevator_is_between_floors(elev)) {
            // Force to appropriate smooth stop
            if (moving upward) {
                elev->state = SMOOTH_STOP_UP;
            } else {
                elev->state = SMOOTH_STOP_DOWN;
            }
        }
    }
    
    // ENFORCE: Between floors violation
    if (status.between_floors_violation && 
        elevator_is_between_floors(elev)) {
        // Force move to nearest floor
        if (elev->current_floor < FLOOR_5) {
            elev->target_floor = (Floor)(elev->current_floor + 1);
            elev->state = MOVING_UP;
        } else {
            elev->target_floor = (Floor)(elev->current_floor - 1);
            elev->state = MOVING_DOWN;
        }
    }
}
```

## Testing Guardrails

The guardrails are thoroughly tested in test_guardrails.c and test_simple.c:

**Test Coverage:**
- 21 dedicated guardrail tests
- Violation detection in all states
- Enforcement actions verification
- Edge case handling
- Integration with elevator state machine

**Running Tests:**
```bash
./test_simple  # Includes 21 guardrail tests
```

## Guardrails vs State Machine

| Aspect | State Machine | Guardrails |
|--------|---------------|-----------|
| Purpose | Normal operation | Safety backup |
| Trigger | State transitions | Violation detection |
| Action | State changes | Force corrective state |
| Priority | Follows rules | Overrides if needed |
| Monitoring | Continuous | Reactive |

**Key Difference:** The state machine defines how the elevator *should* behave normally. Guardrails detect when something *wrong* happens and force a safe state.

## Error Codes & Messages

### Critical Violation
```
VIOLATION: Doors open while moving! Force stop.
```
**Action:** Immediate state change to IDLE, doors close, motion stops

### Safety Constraint Violation
```
VIOLATION: Cannot stop between floors. Continuing to next floor.
```
**Action:** Force movement to nearest floor

### User Prompt (Non-Critical)
```
Elevator stopped for 20 seconds. Press 'R' to resume or 'W' to wait.
```
**Action:** Operator decision required

## Best Practices

1. **Always Check Before Enforcing**
   ```c
   status = guardrail_check(&elev);
   if (status.doors_open_violation) {
       guardrail_enforce(&elev);
   }
   ```

2. **Log Violations**
   ```c
   if (status.doors_open_violation) {
       log_event("CRITICAL: Doors open violation detected");
   }
   ```

3. **Inform Operators**
   ```c
   const char *msg = guardrail_violation_description(&status);
   if (msg) {
       display_to_operator(msg);
   }
   ```

4. **Respect Safety Constraints**
   - Don't disable or bypass guardrails
   - Guardrails take precedence over normal operation
   - When violated, use enforced state

## Limitations & Assumptions

1. **Doors Open Detection:** Assumes `doors_open` boolean accurately reflects physical door state
2. **Between Floors Detection:** Based on state machine state, not actual physical position
3. **Time Tracking:** Uses system `time()` function, assumes monotonic time
4. **Frequency Check:** Violations checked on every tick (100ms assumed frequency)

## Future Enhancements

Potential improvements to guardrails system:

- [ ] Weight-based safety (detect overloaded elevator)
- [ ] Temperature monitoring (overheat detection)
- [ ] Door sensor redundancy
- [ ] Multiple safety zones
- [ ] Fault recovery strategies
- [ ] Detailed audit logging
- [ ] Preventive maintenance tracking

## Related Files

- [elevator.c](elevator.c) / [elevator.h](elevator.h) - Core state machine
- [guardrails.c](guardrails.c) / [guardrails.h](guardrails.h) - Guardrails implementation
- [test_guardrails.c](test_guardrails.c) - Comprehensive tests
- [TESTING.md](TESTING.md) - Test documentation
- [STATE_MACHINE.md](STATE_MACHINE.md) - Elevator state machine details

## Summary

Guardrails are the **safety net** of the elevator system:

✓ Detect dangerous states (doors open while moving, emergency stop between floors)
✓ Enforce immediate corrective actions
✓ Track emergency conditions with timing
✓ Report violations to operators
✓ Provide a failsafe mechanism

By combining the state machine (defines normal behavior) with guardrails (prevents bad behavior), the elevator system achieves both correct operation and safety assurance.
