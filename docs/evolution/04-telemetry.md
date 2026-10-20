# Telemetry and feedback: persistent experiments inside C

Added after the `web1` checkpoint. [The web-interface snapshot](02-after.md) and [public-access snapshot](03-public-access.md) document the previous architecture. Live state previously existed only in memory and the display event ring. The new embedded recorder preserves experimental data independently of browser polling.

## Architecture

```mermaid
flowchart LR
    subgraph Web[Browser modules]
        Page["web/index.html<br/>Dashboard structure and controls"]
        App["web/app.js<br/>Shared state, commands and rendering"]
        Styles["web/app.css<br/>Layout and component styles"]
        Page -->|loads| App
        Page -->|loads| Styles
        Explorer["web/telemetry.js<br/>web/telemetry.css<br/>Experiments, feedback and exports"]
        Page -->|loads| Explorer
        Explorer -->|uses ElevatorLab API| App
        Logo["web/assets/agenticai-logo.png<br/>Supplied logo"]
        Page -->|references| Logo
    end
    subgraph Backend[C backend modules]
        HTTP["src/web_server.c<br/>HTTP routes, validation and clock<br/>Includes simulation.h and telemetry.h"]
        Sim["src/simulation.c<br/>include/simulation.h<br/>Passenger dispatch, capacity and state JSON"]
        Elevator["src/elevator.c<br/>include/elevator.h<br/>Elevator types and state-machine API"]
        Guard["src/guardrails.c<br/>include/guardrails.h<br/>GuardrailStatus and safety API"]
        HTTP -->|uses simulation.h API| Sim
        Sim -->|uses elevator.h API| Elevator
        Sim -->|uses guardrails.h API| Guard
        Guard -->|uses Elevator type and helpers| Elevator
        Store["src/telemetry.c<br/>include/telemetry.h<br/>Recorder, feedback and SQL query API"]
        SQLite["vendor/sqlite/sqlite3.c<br/>vendor/sqlite/sqlite3.h<br/>Embedded SQLite library"]
        HTTP -->|uses telemetry.h API| Store
        Store -->|uses Simulation types| Sim
        Store -->|uses sqlite3.h API| SQLite
    end
    App -->|HTTP state and command API| HTTP
    Explorer -->|HTTP telemetry and feedback API| HTTP
    DB[("data/elevator.sqlite3<br/>Persistent experiment data")]
    SQLite -->|owns database I/O| DB
```

## Sequence: record and query an experiment

```mermaid
sequenceDiagram
    actor Operator
    participant Browser
    participant HTTP as C HTTP server
    participant Sim as C simulation
    participant Recorder as C telemetry recorder
    participant DB as Embedded SQLite
    Operator->>Browser: Start named experiment
    Browser->>HTTP: POST /api/experiments
    HTTP->>Recorder: Archive previous run and create new run
    Recorder->>DB: Transaction, retain all prior records
    Recorder->>Sim: Initialize fresh elevator state
    HTTP-->>Browser: New authoritative state and runId
    loop Every 100 ms, independently of browsers
        HTTP->>Sim: Advance one tick
        HTTP->>Recorder: Capture current state, changed passengers, new events
        Recorder->>DB: Atomic transaction with run and tick identity
    end
    Operator->>Browser: Select run, dataset and tick range
    Browser->>HTTP: GET /api/telemetry/samples with filters
    HTTP->>Recorder: Execute fixed query with bound parameters
    Recorder->>DB: Read bounded page in tick order
    DB-->>Browser: JSON rows, schema and pagination metadata
    Browser->>HTTP: Fetch remaining pages for export
    Browser->>Operator: Download filtered JSON or CSV
```

## Sequence: feedback survives a concurrent reset

```mermaid
sequenceDiagram
    actor Passenger
    actor OtherVisitor
    participant Browser
    participant HTTP as C HTTP server
    participant Recorder as C recorder
    participant DB as Embedded SQLite
    Passenger->>Browser: Open passenger feedback form
    Note over Browser: Capture original runId and passengerId
    OtherVisitor->>HTTP: Reset elevator
    HTTP->>Recorder: End old run and create new run
    Recorder->>DB: Preserve old passengers and telemetry
    Passenger->>Browser: Submit text and optional rating
    Browser->>HTTP: POST /api/feedback with original run and passenger
    HTTP->>Recorder: Validate UTF-8, length, rating and passenger identity
    Recorder->>DB: Insert feedback using composite passenger foreign key
    DB-->>Browser: Saved feedbackId
    Passenger->>HTTP: Query historical feedback
    HTTP->>DB: Read feedback joined on runId and passengerId
    DB-->>Passenger: Correct archived journey and feedback
```

The database uses WAL and full synchronization, one capture per tick plus immediate captures after commands, prepared statements for writes, and bounded fixed read queries. Startup creates a new run; reset/restart do not delete history. Failed capture attempts and event loss are observable. The single-recorder ownership lock permits independent SQLite readers. Full schema, API, data-quality semantics and experimentation examples are documented in [TELEMETRY.md](../TELEMETRY.md).
