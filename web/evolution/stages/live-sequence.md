# Live run sequences and complete exports

Stage 06 is the preserved baseline: six documented stages were available through the evolution timeline, and telemetry exports contained one selected dataset. This stage makes diagrams visible from the dashboard, adds a live sequence of the current run, and allows one export to include all recorded data kinds. The C runtime, embedded SQLite schema and existing telemetry APIs remain the system of record.

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
        Viewer["web/evolution.js<br/>web/evolution.css<br/>Stage and diagram viewer"]
        Page -->|loads| Viewer
        Viewer -->|uses control availability API| App
        Live["web/live-sequence.js<br/>web/live-sequence.css<br/>Runtime event diagram and SVG export"]
        Page -->|loads| Live
        Live -->|subscribes to elevator-state| App
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
    subgraph Documentation[Documentation modules and generated artifacts]
        Sources["docs/evolution/stages.json<br/>docs/evolution/NN-*.md<br/>Stage registry and diagram sources"]
        Library["tools/evolution-lib.mjs<br/>Registry validation and diagram extraction"]
        Render["tools/render-diagrams.mjs<br/>Mermaid SVG and catalog generation"]
        Check["tools/check-evolution.mjs<br/>Source and generated-asset validation"]
        Assets["web/evolution/catalog.json<br/>web/evolution/diagrams/*<br/>web/evolution/stages/*<br/>Generated viewer assets"]
        Render -->|imports| Library
        Check -->|imports| Library
        Library -->|reads| Sources
        Render -->|generates| Assets
        Check -->|validates| Assets
    end
    Viewer -->|reads catalog and diagram assets through HTTP| Assets
    HTTP -->|serves restricted static assets| Assets
```

## Sequence: follow the current run

```mermaid
sequenceDiagram
    actor User
    participant Browser as Dashboard and live sequence
    participant Server as C HTTP server
    participant Sim as C simulation
    participant DB as Embedded SQLite
    User->>Server: Add passengers or operate cabin controls
    Server->>Sim: Apply validated command
    loop Every simulation tick
        Sim->>Sim: Advance elevator and passenger journeys
        Sim->>DB: Record samples, journey changes and events
        Browser->>Server: Poll existing state endpoint
        Server-->>Browser: Current run, cabin state and recent timestamped events
        Browser->>Browser: Draw chronological interactions and current progress
    end
    User->>Browser: Pause diagram or download SVG
    Note over Browser,Sim: Pausing the view leaves the simulation running.
    User->>Server: Start a new experiment
    Server->>Sim: Reset into a new run
    Server-->>Browser: New run ID and fresh event sequence
    Browser->>Browser: Replace the previous run's live diagram
    User->>Browser: Open architecture and sequence history
    Browser->>Server: Read generated stage catalog and selected diagrams
```

## Sequence: export every recorded data kind

```mermaid
sequenceDiagram
    actor User
    participant Export as Browser exporter
    participant API as C telemetry read API
    participant DB as Embedded SQLite
    User->>Export: Choose experiment, tick range and All record types
    Export->>API: Read summary when no end tick was supplied
    API->>DB: Obtain last recorded tick
    API-->>Export: Shared export cutoff
    loop Samples, runtime events, passenger journeys and feedback
        loop Until all pages are read
            Export->>API: Read dataset with run ID, tick range, limit and offset
            API->>DB: Query ordered records
            DB-->>API: Next page
            API-->>Export: Typed rows and pagination metadata
        end
    end
    Export->>Export: Merge by tick with a recordType discriminator
    Export-->>User: One JSON or CSV download preserving original fields
```

The live diagram uses the actual recent runtime event buffer, ordered by tick and event ID. It identifies passenger interactions against the structured passenger records; other messages remain runtime observations. It shows the latest 20 interactions, clearly identifies the run and visible window, and does not claim to show the complete persisted history. Its current-state caption continues to show cabin progress between events. Pausing freezes only the view; delayed state updates are labelled. SVG downloads and a textual event list make the diagram usable beyond the preview.

All-record exports include `recordType` and a normalized `tick` while preserving each dataset's fields. They ignore the passenger-only filter, paginate every dataset, and retain the selected run even if another visitor resets the simulator. Tick ranges select journeys by `addedTick`, samples/events/feedback by their own ticks. Journey rows remain summaries as observed when read, rather than reconstructed historical snapshots. Existing selected-dataset JSON and CSV formats remain available. The shared cutoff bounds new records; it does not make the multiple API reads a database transaction.

Dashboard shortcuts and the navigation label explicitly expose architecture and sequence diagrams. This stage is appended to the same web evolution catalog as earlier stages.
