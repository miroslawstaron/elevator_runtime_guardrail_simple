# Before: terminal elevator

Baseline captured on 2026-10-20 before implementation changes. Source: `src/main.c`, `src/elevator.c`, `src/guardrails.c` and their headers.

## Architecture

```mermaid
flowchart LR
    User[Operator] -->|Keyboard 0–9, S, R, W, H, Q| Main
    subgraph Process[Single C process]
        Main[main.c: terminal UI and 100 ms loop]
        Core[elevator.c: table-driven state machine]
        Guards[guardrails.c: check and enforcement]
        State[(Elevator struct in memory)]
        Main --> Core
        Main --> Guards
        Core <--> State
        Guards <--> State
    end
    Main -->|ASCII cabin, status and warnings| Terminal[Terminal screen]
    Terminal --> User
```

## Sequence: request and travel

```mermaid
sequenceDiagram
    actor Operator
    participant UI as main.c
    participant Guard as guardrails.c
    participant Core as elevator.c
    participant State as Elevator
    Operator->>UI: Press floor key
    UI->>Core: elevator_press_button(floor)
    Core->>State: Set button and target if idle
    loop Every approximately 100 ms
        UI->>Guard: guardrail_check / guardrail_enforce
        Guard->>State: Inspect and correct violations
        UI->>Core: elevator_tick
        Core->>State: Transition state, move every five ticks
        UI->>Operator: Redraw cabin, buttons and warnings
    end
    Note over Core,State: Highest pending floor served first upward,<br/>lowest pending floor served first downward.<br/>Door-open state currently lasts one tick.
```

## Sequence: emergency stop and resume

```mermaid
sequenceDiagram
    actor Operator
    participant UI as main.c
    participant Core as elevator.c
    participant Guard as guardrails.c
    Operator->>UI: S
    UI->>Core: elevator_press_stop
    Note over Core: Set EMERGENCY_STOP and record wall time
    loop Simulation loop
        UI->>Guard: check and enforce
        opt Stop with motion_ticks greater than zero
            Guard->>Core: Force movement toward adjacent floor
        end
        UI->>Core: elevator_tick
        UI->>Operator: Show stop status and periodic 20-second prompt
    end
    Operator->>UI: R
    UI->>Core: elevator_release_stop
    Note over Core: Clear stop flag and return to IDLE
```

## Baseline limits

There is one elevator, ten floors (0–9), and one shared set of floor requests. There are no passenger identities, boarding rules, capacity, directional hall calls, network API, or persistence. Time advances in the terminal loop. `elevator_get_position` reports integer floors. This is an educational simulation, not a physical elevator controller.
