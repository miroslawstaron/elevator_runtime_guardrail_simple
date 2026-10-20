# Module-based architecture diagrams

Stage 11 is the baseline: an elevator-first interface, scheduled traffic scenarios,
SQLite telemetry and rule-based steering recommendations. This stage changes the
architecture documentation to describe software structure at the file and module level.

## What changed and why

- Architecture diagrams identify actual implementation modules: C files and their
  public headers, HTML/JavaScript/CSS components, documentation tools and deployment
  configuration. Labeled edges describe includes, shared APIs, HTTP dependencies,
  asset loading, generation or process ownership.
- The architecture diagrams for stages 01–11 are revised to show the modules available
  at each historical stage. This is a correction to their representation, not a change
  to the systems those stages describe. No later module is added to an earlier stage.
  Stage IDs, historical behavior, notes outside the architecture sections and sequence
  diagram sources are preserved. Earlier themes remain stage-specific.
- The current source architecture is separated into C, web, and tooling/deployment
  views below. Each box represents a real source file, header, grouped component or
  clearly named generated artifact. There are no decision branches or numbered tasks.
- Mermaid `flowchart` remains the supported rendering syntax for module/dependency
  graphs. It does not imply an execution flow. Sequence diagrams continue to explain
  ordering, calls and interactions.
- `AGENTS.md` records this distinction for future implementation stages. The existing
  renderer, generated catalog, SVG/Mermaid downloads and offline gallery publish the
  updated views. The documentation pipeline itself does not change behavior.
- The simulator, algorithm, database schema, running C process, public tunnel and
  network configuration are unchanged. Static documentation updates require no restart.

## Architecture: C files, public headers and include dependencies

Solid arrows point from an including file to its header. Labeled dotted edges show
library linkage or data-file ownership. Public APIs and shared types are declared
in headers, and their implementations reside in the matching C files.

```mermaid
flowchart TB
    subgraph Entry[Application entry points]
        HTTP["src/web_server.c<br/>HTTP routes, request validation and clock"]
        Main["src/main.c<br/>Original terminal application"]
    end
    subgraph Dispatch[Passenger simulation module]
        SimC["src/simulation.c<br/>Dispatch, scenario scheduling, VIP and aging policy"]
        SimH["include/simulation.h<br/>Simulation, Passenger, ScenarioConfig and API"]
        SimC -->|includes| SimH
    end
    subgraph Telemetry[Telemetry module]
        StoreC["src/telemetry.c<br/>Recording, feedback, SQL queries and analysis metrics"]
        StoreH["include/telemetry.h<br/>Opaque Telemetry handle and query API"]
        StoreC -->|includes| StoreH
    end
    subgraph Core[Elevator module]
        ElevatorC["src/elevator.c<br/>State-machine implementation"]
        ElevatorH["include/elevator.h<br/>Elevator, Floor, ElevatorState and API"]
        ElevatorC -->|includes| ElevatorH
    end
    subgraph Safety[Guardrail module]
        GuardC["src/guardrails.c<br/>Safety checks and enforcement"]
        GuardH["include/guardrails.h<br/>GuardrailStatus and API"]
        GuardC -->|includes| GuardH
    end
    subgraph Vendor[Embedded database library]
        SQLiteC["vendor/sqlite/sqlite3.c"]
        SQLiteH["vendor/sqlite/sqlite3.h"]
        DB[("data/elevator.sqlite3")]
        SQLiteC -->|includes| SQLiteH
        SQLiteC -.->|database file I/O| DB
    end
    HTTP -->|includes| SimH
    HTTP -->|includes| StoreH
    StoreH -->|includes| SimH
    SimH -->|includes| ElevatorH
    SimH -->|includes| GuardH
    GuardH -->|includes| ElevatorH
    Main -->|includes| ElevatorH
    Main -->|includes| GuardH
    StoreC -->|includes| SQLiteH
    StoreC -.->|linked library implementation| SQLiteC
```

## Architecture: HTML, JavaScript and CSS module dependencies

The scripts use a shared `window.ElevatorLab` interface and `elevator-state` events,
not ES module imports. Analysis report styles are defined in `web/scenarios.css`.
HTTP edges represent component dependencies on existing server APIs.

```mermaid
flowchart LR
    Page["web/index.html<br/>Page structure, controls and script/style loading"]
    subgraph Scripts[Browser JavaScript components]
        App["web/app.js<br/>window.ElevatorLab, state polling and commands"]
        Scenarios["web/scenarios.js<br/>Traffic configuration and progress"]
        Explorer["web/telemetry.js<br/>Run selection, feedback and export"]
        Analysis["web/analysis.js<br/>Recommendation rules and report rendering"]
        Live["web/live-sequence.js<br/>Runtime event SVG and download"]
        Evolution["web/evolution.js<br/>Catalog-driven diagram viewer"]
    end
    subgraph Styles[Stylesheets and image assets]
        Base["web/app.css<br/>web/metriczone.css<br/>Base layout and visual theme"]
        Components["web/scenarios.css<br/>web/telemetry.css<br/>web/live-sequence.css<br/>web/evolution.css<br/>Component and analysis styles"]
        Logo["web/assets/metriczone-logo.png"]
    end
    HTTP["src/web_server.c<br/>Existing HTTP API and static asset module"]
    Catalog["web/evolution/catalog.json<br/>web/evolution/diagrams/*<br/>web/evolution/stages/*<br/>Generated documentation"]
    Page -->|loads| App
    Page -->|loads| Scenarios
    Page -->|loads| Explorer
    Page -->|loads| Analysis
    Page -->|loads| Live
    Page -->|loads| Evolution
    Page -->|loads| Base
    Page -->|loads| Components
    Page -->|references| Logo
    Scenarios -->|uses shared API and state events| App
    Explorer -->|uses shared API and state events| App
    Analysis -->|uses shared state and control API| App
    Live -->|uses state events and control API| App
    Evolution -->|uses control availability API| App
    App -->|state, commands and scenario HTTP API| HTTP
    Explorer -->|telemetry and feedback HTTP API| HTTP
    Analysis -->|analysis HTTP API| HTTP
    Evolution -->|reads through static HTTP routes| Catalog
    HTTP -->|serves| Catalog
```

## Architecture: documentation, build and deployment modules

These relationships describe tool imports, file inputs/outputs, build linkage and
process configuration. The scripts and renderer are not modules of the running
C simulation. `cloudflared.exe` is an external executable, not authored source.

```mermaid
flowchart TB
    subgraph Documentation[Documentation tooling]
        Sources["docs/evolution/stages.json<br/>docs/evolution/NN-*.md"]
        Library["tools/evolution-lib.mjs<br/>Registry and diagram-source reader"]
        Renderer["tools/render-diagrams.mjs<br/>SVG, Mermaid, notes and catalog generator"]
        Checker["tools/check-evolution.mjs<br/>Generated-asset validator"]
        Generated["web/evolution/*<br/>docs/evolution/diagrams/*<br/>docs/evolution/diagrams.html"]
        Renderer -->|imports| Library
        Checker -->|imports| Library
        Library -->|reads| Sources
        Renderer -->|generates| Generated
        Checker -->|validates| Generated
    end
    subgraph Build[Build definitions and outputs]
        Definitions["build.ps1 / Makefile / CMakeLists.txt"]
        CModules["src/web_server.c / src/simulation.c<br/>src/telemetry.c / src/elevator.c<br/>src/guardrails.c and include/*.h"]
        Vendor["tools/build-sqlite.ps1<br/>vendor/sqlite/sqlite3.c and sqlite3.h"]
        Binary["elevator_web executable<br/>C modules linked with embedded SQLite"]
        Definitions -->|source inputs| CModules
        Definitions -->|compiles or reuses SQLite object| Vendor
        Definitions -->|produces| Binary
    end
    subgraph Deployment[Windows launch and network modules]
        Launch["run-web.ps1 / run-public.ps1<br/>tools/restart-public-server.ps1"]
        Stop["stop-public.ps1"]
        Settings["lab-network.json"]
        Session["build/public/session.json"]
        Firewall["tools/enable-lab-firewall.ps1"]
        Tunnel["cloudflared.exe<br/>External tunnel executable"]
        Launch -->|reads| Settings
        Launch -->|creates or reads process identities| Session
        Stop -->|checks process identities| Session
        Firewall -->|reads| Settings
        Launch -->|public-session transport dependency| Tunnel
    end
    Launch -->|launches or restarts| Binary
    Binary -->|serves generated documentation| Generated
```

## Sequence: analysis across the actual implementation modules

```mermaid
sequenceDiagram
    actor Operator
    participant HTML as web/index.html
    participant JS as web/analysis.js
    participant HTTP as src/web_server.c
    participant Store as src/telemetry.c
    participant SQLite as vendor/sqlite/sqlite3.c
    Operator->>HTML: Select a run and click Analyze telemetry
    HTML->>JS: Dispatch button event
    JS->>HTTP: GET /api/telemetry/analysis with runId
    HTTP->>Store: telemetry_read via include/telemetry.h
    Store->>SQLite: Prepare and execute fixed aggregate SQL
    SQLite-->>Store: Recorded metrics for the selected run
    Store-->>HTTP: Typed JSON dataset
    HTTP-->>JS: Analysis response
    JS->>JS: Apply recommendation rules
    JS->>HTML: Render metrics, evidence and candidate changes
    Note over HTML,JS: web/scenarios.css supplies the analysis report styles.
```

The architecture relationships can be checked against source includes, HTML asset
references, JavaScript shared API usage and build/script inputs. Generated SVGs and
Mermaid sources remain available from the public evolution viewer and offline gallery.
