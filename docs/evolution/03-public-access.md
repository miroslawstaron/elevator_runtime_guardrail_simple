# Internet access: HTTPS tunnel to the C backend

Added on 2026-10-20 after the local web interface. The original [before](01-before.md) and [after](02-after.md) documents remain snapshots of those stages. The default local port is now 8099. Public access adds a transport process; elevator state and passenger logic still run in C.

## Architecture

```mermaid
flowchart TB
    Remote[Remote browser] -->|HTTPS on port 443| Edge[Cloudflare public HTTPS endpoint]
    subgraph Computer[This Windows computer]
        Tunnel[cloudflared<br/>Outbound encrypted tunnel]
        Server[C HTTP server<br/>127.0.0.1 port 8099]
        Sim[C passenger dispatcher<br/>Independent 100 ms clock]
        Core[Existing C elevator state machine]
        Guards[Existing C runtime guardrails]
        Assets[Local HTML, CSS, JavaScript and logo]
        Tunnel -->|Forward HTTP and preserve Host and Origin| Server
        Server --> Sim
        Server --> Assets
        Sim --> Core
        Sim --> Guards
    end
    Tunnel <-->|Encrypted outbound connection| Edge
    Local[Local browser] -->|http://localhost:8099| Server
```

## Sequence: operate the simulator through the internet

```mermaid
sequenceDiagram
    actor Visitor
    participant Browser as Remote browser
    participant Edge as Public HTTPS endpoint
    participant Tunnel as cloudflared on this computer
    participant Server as C server on port 8099
    participant Sim as C simulation
    Visitor->>Browser: Open generated public URL
    Browser->>Edge: GET / over HTTPS
    Edge->>Tunnel: Route through encrypted tunnel
    Tunnel->>Server: HTTP/1.1 request with public Host
    Server->>Server: Match exact configured public hostname
    Server-->>Browser: HTML, JavaScript, CSS and supplied logo
    Visitor->>Browser: Add passenger and destination
    Browser->>Edge: POST /api/people with public HTTPS Origin
    Edge->>Tunnel: Forward request
    Tunnel->>Server: Preserve Host and Origin headers
    Server->>Server: Validate exact allowed origin and passenger input
    Server->>Sim: Add passenger
    Sim-->>Browser: Authoritative state via tunnel and HTTPS
    loop Independent C clock and browser polling
        Server->>Sim: Advance at 10 Hz
        Browser->>Server: Poll state through public HTTPS tunnel
        Server-->>Browser: Shared elevator and passenger state
    end
```

`run-public.ps1` checks an exclusive bind on the requested port, compiles a separate C executable, verifies a pinned official cloudflared binary's SHA-256, starts hidden background processes, discovers the assigned HTTPS origin, and starts C with that exact origin allowed. Both local and public HTTP checks must pass before the session is recorded. Public DNS is used to verify newly assigned addresses if the local DNS cache has not resolved them yet; machine DNS settings remain unchanged.

`build/public/session.json` records the current URL, port, process IDs, executable paths, and process start times. `stop-public.ps1` checks process identity before stopping the session. Logs and state remain under ignored `build/`. Other services and firewall rules are not changed.

This deployment uses a temporary Cloudflare Quick Tunnel, not an always-on hosted server. The URL works while this computer is awake, online, and both background processes are running. Restarting creates a different hostname and a fresh in-memory simulation. All visitors share the same elevator and can operate its controls. Stable hostnames and availability independent of this computer require a named tunnel or a hosted C server. See [Cloudflare Quick Tunnel behavior and limitations](https://developers.cloudflare.com/tunnel/get-started/quick-tunnels/).
