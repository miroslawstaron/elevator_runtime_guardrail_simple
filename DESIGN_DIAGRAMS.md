# Elevator Control System - Design Documentation

## Overview

This document provides visual representations of the elevator control system design using state machine diagrams and component architecture diagrams.

---

## Part 1: Elevator State Machine

### Main State Machine Diagram

The elevator follows a sophisticated state machine with 11 distinct states and smooth acceleration/deceleration transitions.

```mermaid
stateDiagram-v2
    [*] --> IDLE
    
    IDLE --> DOORS_OPENING: button_pressed_at_current_floor
    IDLE --> SMOOTH_START_UP: should_go_up
    IDLE --> SMOOTH_START_DOWN: should_go_down
    
    SMOOTH_START_UP --> MOVING_UP: after_1_tick
    SMOOTH_START_DOWN --> MOVING_DOWN: after_1_tick
    
    MOVING_UP --> SMOOTH_STOP_UP: at_target_floor
    MOVING_DOWN --> SMOOTH_STOP_DOWN: at_target_floor
    
    SMOOTH_STOP_UP --> IDLE: after_1_tick
    SMOOTH_STOP_DOWN --> IDLE: after_1_tick
    
    DOORS_OPENING --> DOORS_OPEN: doors_fully_open_2_ticks
    DOORS_OPEN --> DOORS_CLOSING: after_10_ticks
    DOORS_CLOSING --> IDLE: doors_fully_closed_2_ticks
    
    IDLE --> EMERGENCY_STOP: stop_button_pressed
    MOVING_UP --> EMERGENCY_STOP: stop_button_pressed
    MOVING_DOWN --> EMERGENCY_STOP: stop_button_pressed
    
    EMERGENCY_STOP --> IDLE: stop_button_released
    
    note right of SMOOTH_START_UP
        Smooth acceleration
        (1 tick duration)
    end note
    
    note right of MOVING_UP
        Linear motion
        (5 ticks per floor)
    end note
    
    note right of SMOOTH_STOP_UP
        Smooth deceleration
        (1 tick duration)
    end note
    
    note right of DOORS_OPENING
        Door animation
        (2 ticks)
    end note
    
    note right of DOORS_OPEN
        Doors fully open
        (10 ticks, then close)
    end note
```

### State Descriptions

| State | Purpose | Duration | Next State |
|-------|---------|----------|-----------|
| **IDLE** | Waiting at floor, doors closed | ∞ | Next floor request |
| **SMOOTH_START_UP** | Acceleration upward | 1 tick | MOVING_UP |
| **MOVING_UP** | Moving upward | 5 ticks/floor | SMOOTH_STOP_UP |
| **SMOOTH_STOP_UP** | Deceleration upward | 1 tick | IDLE |
| **SMOOTH_START_DOWN** | Acceleration downward | 1 tick | MOVING_DOWN |
| **MOVING_DOWN** | Moving downward | 5 ticks/floor | SMOOTH_STOP_DOWN |
| **SMOOTH_STOP_DOWN** | Deceleration downward | 1 tick | IDLE |
| **DOORS_OPENING** | Opening animation | 2 ticks | DOORS_OPEN |
| **DOORS_OPEN** | Fully open at floor | 10 ticks | DOORS_CLOSING |
| **DOORS_CLOSING** | Closing animation | 2 ticks | IDLE |
| **EMERGENCY_STOP** | Emergency stop active | ∞ | IDLE (on release) |

---

## Part 2: Button Press Logic Flow

```mermaid
flowchart TD
    A["Button Pressed for Floor N"] --> B{Current State?}
    B -->|IDLE| C["Set target_floor = N"]
    B -->|MOVING| D["Queue in button_pressed array"]
    C --> E["Check if N == current_floor"]
    E -->|Yes| F["Trigger door opening"]
    E -->|No| G["Start movement SMOOTH_START"]
    D --> H["When reaching IDLE:<br/>Check queued buttons"]
    H --> I["Find next target"]
    I --> G
    F --> J["Enter DOORS_OPENING state"]
    G --> K["MOVING_UP or MOVING_DOWN"]
```

---

## Part 3: Movement Timing Diagram

```mermaid
timeline
    title Floor Movement Timing (Moving Up)
    SMOOTH_START_UP : 1 tick : Accelerate
    MOVING_UP Tick 1 : 1 tick : At floor N (between N and N+1)
    MOVING_UP Tick 2 : 1 tick : Still between N and N+1
    MOVING_UP Tick 3 : 1 tick : Still between N and N+1
    MOVING_UP Tick 4 : 1 tick : Still between N and N+1
    MOVING_UP Tick 5 : 1 tick : Reach floor N+1
    Check Target : Instant : At_target?
    SMOOTH_STOP_UP : 1 tick : Decelerate
    IDLE : Wait : At floor N+1
```

---

## Part 4: Component Architecture Diagram

```mermaid
graph TB
    subgraph "Elevator System"
        EM["Elevator Module"]
        subgraph "Core Components"
            SM["State Machine<br/>(11 states)"]
            BM["Button Manager<br/>(10 floors)"]
            MM["Motion Manager<br/>(Floor tracking)"]
            DM["Door Manager<br/>(3 phases)"]
        end
        
        GM["Guardrails Module"]
        subgraph "Safety Components"
            VD["Violation Detector"]
            VE["Violation Enforcer"]
            TT["Timing Tracker"]
        end
    end
    
    subgraph "Interfaces"
        UI["User Input<br/>(Buttons, Stop)"]
        DO["Display Output<br/>(Status, Warnings)"]
    end
    
    subgraph "Data Structures"
        ES["Elevator State"]
        GS["Guardrail Status"]
    end
    
    UI --> EM
    SM --> BM
    SM --> MM
    SM --> DM
    EM --> ES
    ES --> GM
    VD --> VE
    VE --> TT
    GM --> GS
    SM -.->|Safety Check| VD
    VE -.->|Enforcement| SM
    GS --> DO
    ES --> DO
    
    style EM fill:#e1f5ff
    style GM fill:#fff3e0
    style SM fill:#f3e5f5
    style VD fill:#ffe0b2
    style VE fill:#ffcc80
```

---

## Part 5: Guardrails Architecture

```mermaid
graph LR
    subgraph "Monitoring"
        DC["Door State<br/>Check"]
        MC["Motion State<br/>Check"]
        TC["Time<br/>Tracking"]
    end
    
    subgraph "Detection"
        DOV["Doors Open<br/>Violation"]
        BFV["Between Floors<br/>Violation"]
        SBS["Stop Button<br/>Status"]
    end
    
    subgraph "Enforcement"
        DOEF["Close Doors<br/>Stop Motion"]
        BFEF["Force to<br/>Nearest Floor"]
        SBEF["Operator<br/>Prompt"]
    end
    
    subgraph "Reporting"
        MSG["Generate<br/>Messages"]
        LOG["Log Events"]
    end
    
    DC --> DOV
    MC --> DOV
    MC --> BFV
    TC --> SBS
    
    DOV --> DOEF
    BFV --> BFEF
    SBS --> SBEF
    
    DOEF --> MSG
    BFEF --> MSG
    SBEF --> MSG
    MSG --> LOG
    
    style DC fill:#ffcdd2
    style DOV fill:#ef5350
    style DOEF fill:#c62828
```

---

## Part 6: Violation Detection Logic

### Doors Open While Moving Violation

```mermaid
graph TD
    A["Check: doors_open?"] -->|False| B["No violation"]
    A -->|True| C["Check: between_floors?"]
    C -->|False| D["Safe - at floor"]
    C -->|True| E["VIOLATION:<br/>Doors Open While Moving"]
    E --> F["ENFORCE:<br/>Set State to IDLE"]
    F --> G["Next tick:<br/>Smooth Stop"]
    G --> H["Doors Close"]
```

### Emergency Stop Between Floors Violation

```mermaid
graph TD
    A["Check: stop_button_pressed?"] -->|False| B["No violation"]
    A -->|True| C["Check: state == EMERGENCY_STOP?"]
    C -->|False| D["Stop at floor"]
    C -->|True| E["Check: between_floors?"]
    E -->|False| F["Safe - at floor"]
    E -->|True| G["VIOLATION:<br/>Stopped Between Floors"]
    G --> H{Current Floor<br/>Position}
    H -->|Lower half< 5| I["ENFORCE:<br/>Move UP to next floor"]
    H -->|Upper half≥ 5| J["ENFORCE:<br/>Move DOWN to next floor"]
    I --> K["Resume normal operation<br/>to nearest floor"]
    J --> K
```

---

## Part 7: System Data Flow

```mermaid
graph TD
    A["User Input"] -->|Button Press| B["Elevator Module"]
    A -->|Stop Button| C["Guardrails Module"]
    
    B -->|Current State| D["State Machine"]
    D -->|State Change| E["Motion Update"]
    E -->|Floor Update| F["Elevator State"]
    
    F -->|Position/State| C
    C -->|Violation Detected?| G{Check}
    G -->|Yes| H["Enforce Guardrails"]
    H -->|Modify State| D
    G -->|No| I["Continue Normal Op"]
    
    D -->|Status| J["Display Output"]
    H -->|Warning Message| J
    
    style A fill:#c8e6c9
    style B fill:#bbdefb
    style C fill:#ffe0b2
    style H fill:#ffcdd2
    style J fill:#f0f4c3
```

---

## Part 8: Tick-by-Tick Operation

```mermaid
graph LR
    T1["Tick N"] --> T2["Check Violations<br/>guardrail_check"]
    T2 --> T3["Enforce Guardrails<br/>guardrail_enforce"]
    T3 --> T4["Update Elevator<br/>elevator_tick"]
    T4 --> T5["Advance Time"]
    T5 --> T6["Repeat"]
    T6 -.-> T1
    
    T1 -.->|100ms cycle| T7["Real Time"]
    
    style T2 fill:#ffe0b2
    style T3 fill:#ffcc80
    style T4 fill:#bbdefb
```

---

## Part 9: Safety Guarantee Layers

```mermaid
graph TB
    subgraph "Layer 1: State Machine"
        A["Defines valid<br/>state transitions"]
    end
    
    subgraph "Layer 2: Guardrails"
        B["Detects violations"]
        C["Enforces safe state"]
    end
    
    subgraph "Layer 3: Testing"
        D["49 unit tests<br/>verify safety"]
        E["100% pass rate"]
    end
    
    A --> B
    B --> C
    C --> D
    D --> E
    
    E --> F["Safe Elevator<br/>Operation"]
    
    style A fill:#c8e6c9
    style B fill:#fff3e0
    style C fill:#ffcc80
    style D fill:#bbdefb
    style E fill:#c8e6c9
    style F fill:#81c784
```

---

## Part 10: Emergency Stop Sequence

```mermaid
sequenceDiagram
    participant U as User
    participant E as Elevator
    participant G as Guardrails
    
    U->>E: Press Stop Button
    E->>E: Set state = EMERGENCY_STOP
    E->>G: Check guardrails
    G->>G: Detect stop_button_pressed
    G->>G: Track seconds elapsed
    G->>U: Prompt every 20 seconds
    U->>E: Release Stop Button
    E->>E: Set state = IDLE
    E->>G: Check guardrails
    G->>G: Resume normal operation
```

---

## Part 11: Door Operation Sequence

```mermaid
sequenceDiagram
    participant S as State Machine
    participant D as Doors
    participant B as Button
    
    S->>S: Reach target floor
    S->>S: State = DOORS_OPENING
    S->>D: doors_open = true
    Note over D: Opening animation (2 ticks)
    S->>S: State = DOORS_OPEN
    Note over D: Doors fully open (10 ticks)
    B->>S: Clear button pressed
    S->>S: State = DOORS_CLOSING
    S->>D: doors_open = false
    Note over D: Closing animation (2 ticks)
    S->>S: State = IDLE
    Note over D: Ready for next floor
```

---

## Part 12: Between Floors State Definitions

```mermaid
graph TB
    subgraph "BETWEEN FLOORS STATES"
        A["SMOOTH_START_UP"]
        B["SMOOTH_START_DOWN"]
        C["MOVING_UP"]
        D["MOVING_DOWN"]
        E["SMOOTH_STOP_UP"]
        F["SMOOTH_STOP_DOWN"]
    end
    
    subgraph "AT FLOOR STATES"
        G["IDLE"]
        H["DOORS_OPENING"]
        I["DOORS_OPEN"]
        J["DOORS_CLOSING"]
    end
    
    subgraph "SPECIAL STATES"
        K["EMERGENCY_STOP"]
    end
    
    note right of A
        Safe for motion violations
    end note
    
    note right of G
        Safe for door operations
    end note
    
    style A fill:#ffcccc
    style B fill:#ffcccc
    style C fill:#ffcccc
    style D fill:#ffcccc
    style E fill:#ffcccc
    style F fill:#ffcccc
    style G fill:#ccffcc
    style H fill:#ccffcc
    style I fill:#ccffcc
    style J fill:#ccffcc
```

---

## Key Design Principles

### 1. **State Machine Foundation**
- Clear, finite states with well-defined transitions
- Smooth motion with acceleration/deceleration phases
- Atomic state changes (no intermediate inconsistencies)

### 2. **Safety Through Layering**
- State machine ensures valid operations
- Guardrails catch edge cases and violations
- Testing verifies all scenarios

### 3. **Deterministic Behavior**
- All actions based on current state and inputs
- No race conditions or timing-dependent logic
- Predictable motion (constant ticks per floor)

### 4. **Separation of Concerns**
- Elevator module: Normal operation
- Guardrails module: Safety enforcement
- Easy to test, maintain, and extend

### 5. **Reactive Safety**
- Guardrails check on every tick
- Immediate enforcement when violations detected
- No prevention - only detection and correction

---

## Related Documentation

- [GUARDRAILS.md](GUARDRAILS.md) - Detailed guardrails specification
- [STATE_MACHINE.md](STATE_MACHINE.md) - State machine detailed design
- [TESTING.md](TESTING.md) - Test coverage and verification
- [test_simple.c](test_simple.c) - Test implementation

---

## References

- **State Machine Pattern**: Gang of Four Design Patterns
- **Safety Systems**: Defense-in-depth approach
- **Elevator Control**: ASME A17.1 Standard (simplified)
