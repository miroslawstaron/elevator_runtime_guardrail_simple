# Consistent source-code comments

Stage 08 preserves the Metriczone visual identity and the system's existing behavior. This stage applies the commenting conventions demonstrated in `src/guardrails.c` to the authored web simulator code: module banners, descriptive function blocks, and inline explanations of validation, state changes, persistence and cleanup. PowerShell, HTML, CSS and build configuration use their own comment syntax with the same explanatory style.

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
        Theme["web/metriczone.css<br/>web/assets/metriczone-logo.png<br/>Theme and supplied logo"]
        Page -->|loads| Theme
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
    Policy["AGENTS.md<br/>Source-commenting and evolution conventions"]
    Policy -.->|documents source conventions| HTTP
    Policy -.->|documents source conventions| App
```

## Sequence: annotate and verify without changing behavior

```mermaid
sequenceDiagram
    actor Maintainer
    participant Reference as guardrails.c and previous stage
    participant Source as Authored simulator sources
    participant Audit as Comment-only audit
    participant Checks as Compiler and existing checks
    participant Web as Public Elevator Lab
    Maintainer->>Reference: Read the commenting style and preserve the baseline
    Maintainer->>Source: Add module banners, function explanations and inline steps
    Note over Source: Use language-native comments and preserve executable code.
    Maintainer->>Audit: Compare with the saved source baseline
    Audit-->>Maintainer: Confirm only recorded comment insertions
    Maintainer->>Checks: Build C and run unit, HTTP and browser checks
    Checks-->>Maintainer: Existing simulator behavior remains verified
    Maintainer->>Source: Append stage 09 architecture and sequence diagrams
    Maintainer->>Web: Publish generated diagrams and commented frontend sources
    Web-->>Maintainer: New stage visible with earlier snapshots retained
```

The original `src/guardrails.c`, original elevator core, reference implementation and third-party SQLite distribution are not annotated by this change. Binary logo assets and machine-readable JSON remain in their native formats. Generated diagram files are regenerated from the documented sources rather than edited by hand. Comments describe the existing implementation, including failure cases and practical limits; they do not introduce new runtime behavior or a new deployment model.
