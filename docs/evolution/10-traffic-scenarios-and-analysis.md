# Repeatable traffic, explainable steering advice

Stage 09 is the preserved baseline: the Metriczone browser operates a C simulation,
SQLite records experiments, and a generated catalog exposes evolution diagrams.
This stage adds runnable usage scenarios and a dedicated analysis action.

## What changed and why

- **Busy morning:** 300 people by default, evenly arriving over 600 simulation seconds,
  travelling from floor 0 to seeded destinations on floors 1–9.
- **Busy afternoon:** 300 people over 600 seconds, from seeded office floors to floor 0.
- **Occasional travellers:** 24 mixed-floor journeys over 1,200 seconds.
- **VIPs:** 40 mixed-floor journeys over 300 seconds, every fifth person is a VIP.
- **Custom:** up to 20 streams specify origin, destination, people, first arrival,
  interarrival seconds and VIP status. Interval zero creates a simultaneous group.
  Each run supports up to 1,000 people, with arrivals within 86,400 seconds.
- Presets expose people, arrival window and seed. The seeded integer generator makes
  routes repeatable. Arrivals are evenly spaced, not a stochastic arrival model.
  Custom routes and times are explicit, its seed is retained but does not change them.
- Scenario configuration is validated before archiving the current experiment.
  New runs reset to floor 1 and 1× speed. Previous telemetry and feedback remain,
  unfinished previous journeys, including future arrivals, become cancelled.
- The C clock releases scheduled arrivals independently of browsers. The eight-seat
  cabin and original elevator/guardrail modules remain. Empty cabins prioritize VIP
  pickups, compatible boarding also prioritizes VIPs. A 120-second-old waiting person
  outranks new VIPs, with oldest arrival and passenger ID breaking ties. Priority is
  a preference at dispatch/service boundaries, not a guaranteed wait deadline or an
  exclusive VIP cabin, occupied cabins keep serving rider destinations.
- Server-side 1×/5×/20× speed preserves 100 ms simulation steps and records each step.
  Wall-clock timestamps accelerate relative to simulation time. Cancel future arrivals
  leaves existing waiting/riding people active. Schedules live in C memory, restarting
  archives interrupted runs and does not resume their schedules.
- The dedicated **Analyze telemetry & suggest improvements** button queries the whole
  selected live or archived experiment. SQL computes wait mean/p95/max, waiting age,
  VIP/regular mean waits, ride mean, lobby flows, peak queue, motion occupancy, empty
  and full travel fractions, emergency stops, guardrail violations and recording gaps.
  Future arrivals do not count as released demand. Wait percentiles use boarded
  passengers only, with nearest-rank p95, unfinished waits are shown separately.
- Browser rules explain evidence and thresholds for morning lobby return, afternoon
  demand scoring, earlier aging, destination batching, idle positioning and bounded
  VIP priority. Low evidence, recording gaps, stops and feedback are also identified.
  These are hypotheses, not benchmarked improvements, AI inference or automatic tuning.
  Reports are explicit snapshots, re-analysis refreshes them. Table filters do not apply.
- SQLite schema 3 adds a default-zero VIP column in place and retains historical rows.
  Journey status supports scheduled/cancelled, no boarding timestamp is written for
  people who never boarded. Scenario parameters and custom streams are retained in
  durable run events. The live journey table shows at most 100 matching rows, paged
  telemetry remains available. Tick-filtered journey queries select by added tick,
  so future scheduled rows appear only when the requested range includes their tick.

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

## Sequence: start and run reproducible traffic

```mermaid
sequenceDiagram
    actor Operator
    participant UI as Scenario editor
    participant HTTP as C HTTP server
    participant Store as SQLite recorder
    participant Clock as C simulation clock
    participant Dispatch as Passenger dispatcher
    participant Core as Elevator and guardrails
    Operator->>UI: Select preset or configure custom arrival streams
    UI->>HTTP: POST scenario type, people, window, seed, streams
    HTTP->>HTTP: Validate all streams and bounds before mutation
    alt Invalid configuration
        HTTP-->>UI: 400, current experiment remains intact
    else Valid configuration
        HTTP->>Store: Capture and archive previous run, create named experiment
        Store-->>HTTP: New run identity
        HTTP->>Clock: Install scheduled journeys, reset speed to 1x
        HTTP->>Store: Persist schedule, VIP flags and configuration events
        HTTP-->>UI: Authoritative state with scheduled/waiting people
        loop Every 100 ms wall interval, repeated by selected speed
            Clock->>Dispatch: Release due arrivals at 100 ms simulation steps
            Dispatch->>Dispatch: Respect rider stops, direction, capacity, VIP and aging
            Dispatch->>Core: Request safe landing and service passengers
            Core-->>Dispatch: Cabin state and enforced guardrails
            Dispatch->>Store: Persist each simulation step and changed journeys
        end
        Note over Clock,Store: Continues without an open browser, restart archives interrupted schedules.
        Operator->>HTTP: Cancel future arrivals
        HTTP->>Store: Mark only scheduled people cancelled
        Note over Dispatch,Core: Existing waiting and riding journeys continue.
    end
```

## Sequence: analyze evidence and propose an experiment

```mermaid
sequenceDiagram
    actor Operator
    participant UI as Telemetry explorer
    participant HTTP as C read API
    participant DB as Existing SQLite history
    participant Rules as Browser recommendation rules
    Operator->>UI: Select live or archived experiment
    Operator->>UI: Click Analyze telemetry and suggest improvements
    UI->>HTTP: GET /api/telemetry/analysis?runId=selected
    HTTP->>DB: Aggregate whole-run passengers, samples and feedback
    Note over HTTP,DB: Exclude future arrivals from demand, boarded waits and waiting ages remain distinct.
    DB-->>HTTP: Metrics at the recorded tick, including data quality
    HTTP-->>UI: Typed metrics and experiment identity
    UI->>Rules: Apply documented evidence thresholds
    Rules-->>UI: Suggested steering experiments with supporting measurements
    UI-->>Operator: Snapshot report, mark incomplete/live evidence preliminary
    Note over UI,Rules: Switching experiments invalidates old reports and stale responses.
    Note over Operator,DB: Advice never changes steering. Replay the same traffic to benchmark a candidate.
```

The existing persistent database location, public tunnel, allowed hosts, listen address,
and prior stage IDs remain unchanged. New assets are served by the C backend.
Architecture, sequences, SVG and Mermaid downloads are published through the existing
catalog-driven viewer, there is no second hardcoded timeline.
