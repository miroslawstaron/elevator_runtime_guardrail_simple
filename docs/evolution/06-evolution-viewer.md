# Evolution diagrams in the web interface

The previous stages were preserved as Markdown, Mermaid, SVG exports and an offline gallery. This step adds their history to the live web page and establishes a repeatable workflow for subsequent implementation steps. Earlier stage files remain snapshots of their own versions.

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

## Sequence: publish and inspect the next system stage

```mermaid
sequenceDiagram
    actor Developer
    participant Docs as Stage documents and registry
    participant Generator as Diagram generator
    participant Assets as Generated web assets
    participant Server as C HTTP server
    actor User
    participant Browser as Evolution viewer
    Developer->>Docs: Preserve baseline and append a new implementation stage
    Note over Docs: Each stage requires architecture and sequence diagrams.
    Developer->>Generator: Render the stage registry
    Generator->>Docs: Read ordered snapshots and Mermaid sources
    Generator->>Assets: Write SVGs, editable sources, notes and hashed catalog
    Developer->>Assets: Run completeness and freshness checks
    User->>Browser: Open System evolution
    Browser->>Server: GET generated evolution catalog
    Server-->>Browser: Ordered stages and diagram metadata
    User->>Browser: Select a stage and architecture or sequences
    Browser->>Server: GET selected SVG diagram
    Server-->>Browser: Generated diagram with restricted SVG policy
    User->>Browser: Enlarge, zoom or download a diagram
    Browser->>Server: GET SVG or editable Mermaid source
    Server-->>User: Documentation for the selected system version
```

The web viewer consumes one generated catalog rather than duplicating the stage list. It provides stage descriptions, architecture/sequence filters, full-size viewing, zoom, and source downloads. The C server serves only documentation assets with constrained names and extensions inside the generated documentation directories; repository files and the SQLite database remain unavailable through these routes.

`AGENTS.md` records the user's ongoing requirement. Add new stages to `docs/evolution/stages.json`, include both diagram kinds, regenerate, verify, and publish the result with each meaningful implementation step. `tools/check-evolution.mjs` checks the source hashes, diagram completeness and mirrored assets without requiring Mermaid or a browser. Rendering tools are development dependencies only; the deployed backend remains C and the viewer needs no external runtime service.
