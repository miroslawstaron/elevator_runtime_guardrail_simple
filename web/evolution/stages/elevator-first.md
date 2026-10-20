# Elevator-first interface and direct language

Stage 10 is the preserved baseline: the simulator supports scheduled traffic scenarios,
VIP and age-based priority, persistent telemetry, and rule-based steering recommendations.
Its page places the scenario editor before the live elevator and uses promotional headings.

## What changed and why

- The page now presents the live elevator and cabin controls first, followed by traffic
  scenarios, telemetry and algorithm analysis, passenger records and activity, live
  interaction sequences, and system evolution. This is the actual HTML order, including
  keyboard and screen-reader navigation. Cabin controls precede manual passenger input.
- The sidebar follows the same task order. Compact links to Elevator, Scenarios and
  Telemetry & analysis remain available when the sidebar collapses on smaller screens.
  Diagram shortcuts appear with the secondary diagnostics instead of preceding the elevator.
- The title is **Elevator simulator**. Headings, empty states, feedback prompts and
  command messages describe their purpose directly. Marketing slogans and decorative
  captions are removed. The existing logo, colors and available controls are retained.
- The interface describes the autoevolution workflow: run traffic scenarios, analyze
  telemetry, test steering changes and document each implementation stage. Telemetry
  analysis suggests candidates. Algorithm changes are implemented and tested separately,
  with new runs and evolution snapshots. The page does not claim autonomous algorithm
  editing, benchmarking or deployment.
- In telemetry, the experiment selector precedes its metrics and analysis action, so
  the selected run is clear before recommendations are requested. Existing full-run
  scope, snapshot reports and table filters retain their behavior.
- A smaller header and logo give more space to the live elevator. All existing section
  IDs, forms, runtime controls, scenario inputs, export formats and diagram downloads
  remain available. The C backend, steering policies, database, public tunnel and network
  settings are unchanged. The running server reads the revised static assets directly,
  so deployment does not require restarting the simulation.

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
        Scenarios["web/scenarios.js<br/>web/scenarios.css<br/>Scenario editor and runtime controls"]
        Advice["web/analysis.js<br/>Recommendation rules and report UI"]
        Page -->|loads| Scenarios
        Page -->|loads| Advice
        Scenarios -->|uses ElevatorLab API| App
        Advice -->|uses shared control API| App
    end
    subgraph Backend[C backend modules]
        HTTP["src/web_server.c<br/>HTTP routes, validation and clock<br/>Includes simulation.h and telemetry.h"]
        Sim["src/simulation.c<br/>include/simulation.h<br/>Passenger dispatch, scheduling and VIP policy"]
        Elevator["src/elevator.c<br/>include/elevator.h<br/>Elevator types and state-machine API"]
        Guard["src/guardrails.c<br/>include/guardrails.h<br/>GuardrailStatus and safety API"]
        HTTP -->|uses simulation.h API| Sim
        Sim -->|uses elevator.h API| Elevator
        Sim -->|uses guardrails.h API| Guard
        Guard -->|uses Elevator type and helpers| Elevator
        Store["src/telemetry.c<br/>include/telemetry.h<br/>Recorder, SQL queries and analysis aggregates"]
        SQLite["vendor/sqlite/sqlite3.c<br/>vendor/sqlite/sqlite3.h<br/>Embedded SQLite library"]
        HTTP -->|uses telemetry.h API| Store
        Store -->|uses Simulation types| Sim
        Store -->|uses sqlite3.h API| SQLite
    end
    App -->|HTTP state and command API| HTTP
    Explorer -->|HTTP telemetry and feedback API| HTTP
    DB[("data/elevator.sqlite3<br/>Persistent experiment data")]
    SQLite -->|owns database I/O| DB
    Advice -->|HTTP telemetry analysis API| HTTP
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

## Sequence: operate, analyze and evaluate a change

```mermaid
sequenceDiagram
    actor Operator
    participant UI as Elevator-first interface
    participant C as Existing C runtime
    participant DB as Existing telemetry database
    participant Analysis as Existing recommendation rules
    participant Development as Implementation and verification
    participant Evolution as Stage catalog and diagrams
    Operator->>UI: Open simulator and inspect elevator state
    UI->>C: Read state and send cabin controls
    C-->>UI: Live floor, cabin, passenger and safety state
    Operator->>UI: Configure and run a traffic scenario
    UI->>C: Submit the validated scenario configuration
    C->>DB: Record simulation samples and journeys
    Operator->>UI: Select the run before requesting analysis
    UI->>DB: Read whole-run metrics through the existing API
    DB-->>Analysis: Recorded measurements
    Analysis-->>UI: Candidate steering changes with evidence
    Operator->>Development: Select, implement and test a candidate separately
    Development->>Evolution: Append the verified implementation stage
    Evolution-->>UI: Architecture, sequences and downloadable sources
    Note over UI,Development: The interface explains the workflow without changing algorithm code automatically.
```

Earlier stage notes and IDs remain historical snapshots. The new diagrams and stage
notes are generated into the existing catalog-driven web and offline viewers.
