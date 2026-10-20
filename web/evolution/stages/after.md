# After: Elevator Lab web interface with C runtime

Implemented on 2026-10-20. Compare with [the baseline](01-before.md). Open [the diagram gallery](diagrams.html) for rendered diagrams; individual SVG and editable Mermaid exports are in `diagrams/`.

## Architecture

```mermaid
flowchart LR
    subgraph Web[Browser modules]
        Page["web/index.html<br/>Dashboard structure and controls"]
        App["web/app.js<br/>Shared state, commands and rendering"]
        Styles["web/app.css<br/>Layout and component styles"]
        Page -->|loads| App
        Page -->|loads| Styles
        Logo["web/assets/agenticai-logo.png<br/>Supplied logo"]
        Page -->|references| Logo
    end
    subgraph Backend[C backend modules]
        HTTP["src/web_server.c<br/>HTTP routes, validation and clock<br/>Includes simulation.h"]
        Sim["src/simulation.c<br/>include/simulation.h<br/>Passenger dispatch, capacity and state JSON"]
        Elevator["src/elevator.c<br/>include/elevator.h<br/>Elevator types and state-machine API"]
        Guard["src/guardrails.c<br/>include/guardrails.h<br/>GuardrailStatus and safety API"]
        HTTP -->|uses simulation.h API| Sim
        Sim -->|uses elevator.h API| Elevator
        Sim -->|uses guardrails.h API| Guard
        Guard -->|uses Elevator type and helpers| Elevator
    end
    App -->|HTTP state and command API| HTTP
    Build["build.ps1 / Makefile / CMakeLists.txt<br/>C executable build definitions"]
    Build -.->|compiles and links| HTTP
    Build -.->|compiles and links| Sim
```

## Sequence: add passengers and complete a shared trip

```mermaid
sequenceDiagram
    actor Operator
    participant Web as Browser
    participant HTTP as C HTTP server
    participant Sim as C passenger dispatcher
    participant Core as Existing C state machine
    participant Guards as Existing C guardrails
    Operator->>Web: Name, origin, destination and group size
    Web->>HTTP: POST /api/people (form encoded)
    HTTP->>HTTP: Validate framing, host, origin and fields
    HTTP->>Sim: simulation_add_people
    Sim->>Sim: Atomically add WAITING passengers and events
    HTTP-->>Web: Authoritative JSON state
    loop Independent 100 ms monotonic C clock
        HTTP->>Sim: simulation_tick
        Sim->>Guards: Check and enforce
        Sim->>Sim: Select nearest request along current direction
        Sim->>Core: Set next stop and elevator_tick
        opt Doors fully open at pickup floor
            Sim->>Sim: Unload, then board matching direction up to 8 people
            Note over Sim: WAITING becomes RIDING,<br/>destinations become cabin requests
        end
        opt Doors fully open at destination
            Sim->>Sim: RIDING becomes ARRIVED, record completion
        end
        Sim->>Guards: Check and enforce updated state
    end
    loop Approximately every 120 ms after previous response
        Web->>HTTP: GET /api/state
        HTTP-->>Web: Position, people, requests, guardrails and events
        Web->>Web: Animate cabin and render passenger journeys
    end
    Note over HTTP,Sim: Simulation continues with no browser connected.
```

## Sequence: new calls while moving and direction reversal

```mermaid
sequenceDiagram
    actor Alex
    actor Sam
    actor Jamie
    participant Web as Browser
    participant Sim as C dispatcher
    participant Core as C state machine
    Alex->>Web: Floor 1 to 9
    Web->>Sim: Add passenger via HTTP
    Sim->>Core: Open at 1, board Alex, travel upward
    Sam->>Web: Floor 3 to 7, while cabin is moving
    Web->>Sim: Add passenger via HTTP
    Jamie->>Web: Floor 6 to 0
    Web->>Sim: Add passenger via HTTP
    Sim->>Core: Reconsider next stop at each landing
    Sim->>Core: Stop at 3, board Sam going up
    Note over Sim: Jamie waits for the downward direction.
    Sim->>Core: Stop at 7, Sam exits
    Sim->>Core: Stop at 9, Alex exits
    Sim->>Sim: Reverse when no demand remains ahead
    Sim->>Core: Return to 6, board Jamie going down
    Sim->>Core: Stop at 0, Jamie exits
    Note over Sim,Core: Requests share stops, full cabins leave waiting people<br/>for a subsequent trip. Request handling remains in C.
```

## Sequence: stop, hold and resume

```mermaid
sequenceDiagram
    actor Operator
    participant Web as Browser
    participant HTTP as C HTTP server
    participant Sim as C simulation adapter
    participant Core as Existing C state machine
    participant Guards as Existing C guardrails
    Operator->>Web: Emergency stop
    Web->>HTTP: POST /api/control action=stop
    HTTP->>Sim: Set stop_requested
    HTTP-->>Web: stopPending=true
    loop Until current floor interval is complete, at most five motion ticks
        HTTP->>Sim: Monotonic clock tick
        Sim->>Core: Continue to next landing with doors closed
    end
    Sim->>Core: elevator_press_stop at motion_ticks=0
    Sim->>Guards: Check and enforce
    Note over Sim: Preserve passengers and requests, hold at landing.<br/>Report elapsed stop time and existing 20-second prompt.
    Web->>HTTP: GET /api/state
    HTTP-->>Web: stopped=true, position at integer landing
    Operator->>Web: Resume elevator
    Web->>HTTP: POST /api/control action=resume
    HTTP->>Sim: Cancel pending stop or release active stop
    Sim->>Core: elevator_release_stop, close doors first if needed
    Sim->>Sim: Continue queued passenger journeys
```

## Decisions and scope

| Concern | Before | After |
| --- | --- | --- |
| User interface | Keyboard and ASCII terminal | Responsive HTML/CSS/JavaScript, supplied agenticAI logo |
| Runtime authority | C terminal loop | C server clock, unaffected by browser refresh or slow clients |
| Elevator and guardrails | `elevator.c`, `guardrails.c` | Same files, unchanged |
| Requests | One undifferentiated button array, farthest destination first | C adapter merges manual calls, waiting origins, and onboard destinations; selects intermediate stops |
| People | None | Named passengers, origin, destination, direction, waiting/riding/arrived lifecycle |
| Capacity | None | Eight riders; excess passengers remain waiting |
| Doors | One core tick fully open | Adapter holds open for about two seconds; unloads before boarding |
| Stop | Core stop can invoke between-floor correction | Adapter finishes the current floor interval before latching the existing stop API |
| State transport | Terminal redraw | Same-origin JSON snapshots; form-encoded commands |
| Storage | In memory | In memory; shared across browser clients; reset/restart clears journeys |

The C adapter supplies one selected button to the existing core, so the core's original farthest-floor policy cannot skip the chosen intermediate stop. Dispatch continues in the current direction until there is no request ahead, then reverses. Opposite-direction passengers can remain waiting at a visited floor. A full cabin ignores pickup-only requests until space is available. Finite traffic is tested to completion; an infinite stream of new requests has no bounded waiting-time guarantee.

The local service binds to IPv4 loopback, accepts only the configured localhost/127.0.0.1 Host, rejects cross-origin browser commands, serves an explicit asset allowlist, caps input and simultaneous clients, and closes stalled connections. It is an educational local simulator, without authentication, persistence, TLS, hardware integration, or production elevator certification. The session retains at most 160 passenger records and the most recent 48 events (20 displayed). Node and Playwright are test tools only; neither is part of the running backend.
