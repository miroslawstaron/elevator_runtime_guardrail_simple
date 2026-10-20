# Direct network access alongside the public tunnel

Recorded from the implemented network-access extension on 2026-10-20. This snapshot fills the deployment detail between [embedded telemetry](04-telemetry.md) and the evolution viewer. The local default remains loopback; network exposure is explicit.

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
    subgraph Deployment[Network and deployment modules]
        Settings["lab-network.json<br/>Listen address and allowed hosts"]
        Launch["run-web.ps1 / run-public.ps1<br/>tools/restart-public-server.ps1<br/>Server launch arguments"]
        Firewall["tools/enable-lab-firewall.ps1<br/>Explicit Windows firewall configuration"]
        Settings -->|configuration input| Launch
        Settings -->|configuration input| Firewall
    end
    Launch -->|configures and launches| HTTP
```

## Sequence: a direct network request

```mermaid
sequenceDiagram
    actor Visitor
    participant Browser as Network browser
    participant Firewall as Windows firewall
    participant Server as C server on port 8099
    participant Sim as C simulation
    Visitor->>Browser: Open configured hostname or destination IP
    Browser->>Firewall: Connect to TCP 8099
    Firewall->>Server: Permit the configured executable and port
    Server->>Server: Match destination IP or explicit hostname alias
    Server-->>Browser: Web interface and local assets
    Visitor->>Browser: Operate elevator or add feedback
    Browser->>Server: POST with matching HTTP Origin
    Server->>Server: Validate Host, port and Origin
    Server->>Sim: Execute validated command
    Sim-->>Browser: Authoritative JSON response
    Note over Server: Arbitrary Host names and cross-origin commands are rejected.
```

The `--listen-address` option defaults to `127.0.0.1`; `0.0.0.0` enables direct IPv4 network connections. Repeated `--allowed-host` arguments allow explicit DNS aliases on the configured port. Destination-IP checks do not accept arbitrary Host values. The public tunnel remains available, and browser commands must match their respective HTTP or HTTPS origin. Upstream routers and institutional firewalls can still restrict inbound reachability.

The PowerShell startup scripts preserve listen addresses and hostname aliases. The firewall helper is separate and requires administrator rights. This documentation step does not change firewall rules or invent network reachability beyond the tested server behavior. Telemetry, feedback and all elevator behavior remain in C.
