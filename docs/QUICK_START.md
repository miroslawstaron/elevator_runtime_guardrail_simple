# Quick Start Guide

## Compilation

### Linux/macOS
```bash
cd elevator_guardrail
make
./elevator_sim
```

### Windows (PowerShell)
```powershell
cd elevator_guardrail
.\build.ps1
.\elevator_sim.exe
```

### Manual (any platform)
```bash
gcc -Wall -Wextra -std=c99 -O2 -o elevator_sim main.c elevator.c guardrails.c
./elevator_sim
```

## Running the Simulator

Once started, you'll see:
```
     ┌─────────────────────┐
  F5 │ [            ]  │  ○
  F4 │ [            ]  │  ○
  F3 │ [  ●  ELEV  ]  │  ●
  F2 │ [            ]  │  ○
  F1 │ [            ]  │  ○
     └─────────────────────┘

╔════════════════════════════════════════════════════╗
║ Current Floor:     Floor 3
║ Target Floor:      Floor 3
║ State:             IDLE
║ Doors:             CLOSED
║ Stop Button:       Released
╠════════════════════════════════════════════════════╣
║ CONTROLS:
║   [1-5] - Request floor       [S] - Stop elevator
║   [R]   - Resume (after stop) [Q] - Quit
╚════════════════════════════════════════════════════╝
```

## Interactive Controls

| Key | Action |
|-----|--------|
| `1-5` | Request a specific floor |
| `S` | Press emergency stop button |
| `R` | Resume after stop |
| `W` | Acknowledge stop prompt (wait) |
| `H` | Show help screen |
| `Q` | Quit simulator |

## Common Scenarios

### Scenario 1: Simple Up-Down Motion
```
Press: 5
Watch: Elevator accelerates up, moves, decelerates
       Stops at floor 5, opens doors
       Returns to idle

Press: 1
Watch: Elevator moves down to floor 1
```

### Scenario 2: Multiple Requests
```
Press: 5
Press: 2
Press: 4
Watch: Elevator queues requests
       Goes to 5 first (higher floor)
       Then 4, then 2
       Services each floor in order
```

### Scenario 3: Emergency Stop
```
Press: 5
(immediately) Press: S
Watch: Elevator stops at next floor (not between floors)
       Emergency stop light shows active
       Press R to resume
```

### Scenario 4: Stop Button Timeout
```
Press: S
Wait: 20 seconds
See: Prompt appears asking to resume or wait
Press: R to resume, or W to acknowledge wait
Wait: Another 20 seconds for next prompt
```

## Understanding the Display

### Elevator Cabin
```
  F3 │ [  ●  ELEV  ]  │  ●
      └─ floor     └─ visual indicator of cabin
```
- `●` = Elevator cabin (shown in exact position)
- `DOORS OPEN` = Doors currently open
- Right column = Button status (● = pressed, ○ = not pressed)

### State Indicators

**Green states** (normal operation):
- `IDLE` - Waiting at floor
- `MOVING_UP` / `MOVING_DOWN` - In transit
- `SMOOTH_START_UP` / `SMOOTH_START_DOWN` - Starting motion
- `SMOOTH_STOP_UP` / `SMOOTH_STOP_DOWN` - Stopping motion

**Blue states** (door operations):
- `DOORS_OPENING` - Opening animation
- `DOORS_OPEN` - Open and waiting
- `DOORS_CLOSING` - Closing animation

**Red states** (emergency):
- `EMERGENCY_STOP` - Stop button pressed

## Guardrails in Action

### Guardrail 1: No Stopping Between Floors
```
1. Press 5 (elevator starts moving up)
2. Immediately press S (emergency stop)
3. Result: Elevator CONTINUES to next floor before stopping
   (never stops between floors)
```

### Guardrail 2: Doors Open → Immediate Stop
```
1. Press 5
2. (In simulation, if doors somehow open while moving)
3. Result: Guardrail forces SMOOTH_STOP
   (warning: "DOORS OPEN WHILE MOVING!")
```

### Guardrail 3: Stop Button Prompt (20s)
```
1. Press S to trigger emergency stop
2. Wait 20 seconds
3. Prompt: "Elevator stopped for 20 seconds"
           "Press R to resume or W to wait"
4. Press R to continue
```

## Tips & Tricks

- **Spam buttons:** You can press multiple floor buttons rapidly. The elevator will queue them all.
- **Change direction mid-motion:** Press a button below current floor while moving up. Elevator will finish current request, then go down.
- **Monitor state transitions:** Watch the state field change (IDLE → SMOOTH_START_UP → MOVING_UP → SMOOTH_STOP_UP → IDLE)
- **Test guardrails:** Press stop while between floors and observe elevator continue to next floor.

## Troubleshooting

### Elevator stuck between floors?
This can't happen—guardrails prevent it. If you see this, it's a bug (report it).

### Doors won't close?
Doors close 10 ticks after opening. If they're stuck, press another floor button or stop.

### Buttons don't respond?
Your input might not have registered. Try pressing 'H' for help to refresh display.

### Program crashes?
Check that gcc is installed and in your PATH:
```bash
gcc --version
```

## Learning the Code

To understand the system, read in this order:

1. **[README.md](README.md)** - Overview of modules
2. **[DESIGN.md](DESIGN.md)** - Architecture and guardrails
3. **elevator.h** - State definitions
4. **elevator.c** - State machine logic (read find_next_state())
5. **guardrails.c** - Safety checks
6. **main.c** - UI loop

## Next Steps

Try these modifications:

1. **Change tick rate:** Modify `SLEEP_MS(100)` in main.c to 50ms (faster) or 200ms (slower)
2. **Add a state:** Add new enum value, update transition table, add condition function
3. **Add guardrail:** Add check in guardrail_check(), enforcement in guardrail_enforce()
4. **Multiple elevators:** Create `Elevator elevators[2]`, call tick() on both

---

Need help? Check [DESIGN.md](DESIGN.md) for detailed architecture or [README.md](README.md) for features.
