# Elevator Lab

The browser operates the original C elevator through a C HTTP server with embedded SQLite telemetry. The original terminal program, core state machine, and guardrail implementation remain available. The supplied Metriczone logo is copied unmodified to `web/assets/metriczone-logo.png` and served locally. `web/metriczone.css` provides the deep teal and white identity, sans-serif headings, measurement cards and responsive logo layout. There are no frontend network dependencies or frontend build step. The earlier logo remains available for historical documentation. See [telemetry, experiments, feedback and data API documentation](TELEMETRY.md).

## Start on Windows

Install MinGW GCC and put `gcc` on PATH, then run from PowerShell:

```powershell
.\run-web.ps1 -Port 8090 -OpenBrowser
```

The script builds the C executable and starts it in the foreground. Open <http://localhost:8090>; Ctrl+C stops it. Without `-Port`, the default is 8099. Choose another port if the address is already in use. Assets are resolved against the repository even if the script is invoked from another working directory.

```powershell
.\build.ps1 -Test -Terminal  # Build web + original terminal, and run C/HTTP tests
.\build\elevator_guardrails.exe # Optional original terminal UI
.\build\elevator_web.exe --port 8090 --web-root web # Run an existing build
```

`build.ps1 -Test` runs the HTTP integration tests when Node is installed. The web app itself only needs the compiled C executable and `web/`.

## Start on Linux/macOS

```sh
make web
./elevator_web --port 8090 --web-root web
make test
node tests/test_web.mjs ./elevator_web # Optional HTTP tests
```

CMake is also supported:

```sh
cmake -S . -B build/cmake
cmake --build build/cmake
ctest --test-dir build/cmake --output-on-failure
./build/cmake/bin/elevator_web --port 8090 --web-root web
```

On Windows, use a MinGW CMake generator or compile using the provided PowerShell script. The CMake configuration copies `web/` to its build directory at configure time. Point `--web-root` at the source `web/` when actively editing the frontend.

## Use the interface

1. Add a named person or group, selecting different origin/destination floors. People appear at the origin while waiting, board when doors open in their direction, and leave at their destination.
2. Add more people while the cabin moves. Riders share trips and intermediate stops. Eight people fit in the cabin; excess riders wait for another trip.
3. Use the floor panel or a floor's `+` button to request a stop manually. Open/close controls are enabled only in valid states.
4. Use Emergency stop to secure the cabin at the next landing, then Resume to continue queued journeys. The existing C guardrail status is displayed beneath the building.
5. Try a busy morning adds six varied journeys. Reset archives the current experiment and returns to floor 1 in a new run. All browser windows share one simulation.
6. Use each passenger's Feedback button for text and optional ratings. Open Telemetry & experiments to inspect history, start named experiments and export data.
   Choose **Export records → All record types** to download samples, events, journeys and feedback together as JSON or CSV.
7. Open **Architecture & sequences** in the sidebar or the **Architecture & sequence history** dashboard link to browse every implementation stage's diagrams. Choose a stage, filter diagram kinds, enlarge/zoom, or download SVGs, editable Mermaid sources and stage notes. A URL such as `/?stage=telemetry#evolution` bookmarks a particular version.
8. Open **Current run sequence** to watch recent interactions and cabin progress. Pause the diagram, resume it, read the textual sequence, or download an SVG.

The browser polls authoritative snapshots and never drives the simulation clock. Closing a browser does not stop the elevator or telemetry recorder. Runtime disconnects disable controls and trigger automatic reconnection. Live cabin state resets on restart; recorded samples, journey timings, events and feedback persist in SQLite. The live limit is 1,000 passenger records per experiment, including completed journeys. Reset starts a fresh experiment without deleting history.

## API

By default, the server accepts only local hosts. When started with `--public-origin https://hostname`, it also accepts that exact public hostname and requires its exact HTTPS Origin for browser POST commands. Forwarded headers never grant permission to arbitrary origins.

The service accepts HTTP/1.1, closes each response connection, and returns JSON errors. POST bodies must be `application/x-www-form-urlencoded` without a charset suffix. It binds to `127.0.0.1` by default; `--listen-address 0.0.0.0` enables direct IPv4 network access. Host must match localhost, the actual destination IP with the configured port, or the explicitly configured public hostname. Browser Origin must match the corresponding HTTP or public HTTPS origin; command-line clients can omit Origin. There is no CORS support.

| Endpoint | Fields | Result |
| --- | --- | --- |
| `GET /api/state` | None | Authoritative snapshot |
| `POST /api/people` | `name`, `from`, `to`, `count` | Validated group insertion and snapshot |
| `POST /api/control` | `action=request`, `floor=0..9` | Manual floor request |
| `POST /api/control` | `action=open/close/stop/resume/reset/demo` | Control command and snapshot |

Names are limited to 40 UTF-8 bytes and cannot contain control characters. Floors are integers 0–9, origin and destination must differ, and group size is 1–8. Group insertion is atomic. Invalid input returns 400, disallowed host/origin 403, unavailable control 409. Request buffers are capped at 8191 bytes and there are at most 16 concurrent HTTP connections; partial requests time out after three seconds. State includes fractional `position`, integer `floor`/`target`, core state, doors, capacity/occupancy, requests, passengers with timestamps in ticks, guardrail flags, and bounded event history. One tick is 100 ms.

## Validation

`tests/test_simple.c` is the original dependency-free suite (53 assertions). `tests/test_simulation.c` checks passenger lifecycle, opposite directions, full cabins, intermediate pickups, door timing/interlocks, stops both upward/downward, invalid input, JSON escaping/bounds, and deterministic traffic with 1,000 people, plus scheduled presets, custom streams, VIP priority and aging. `tests/test_web.mjs` checks real C HTTP behavior, logo/assets, input validation, framing, host/origin checks, stop/resume, and that a partial HTTP request does not block the C clock.

Optional browser regression checks (installed Edge on Windows, or Playwright Chromium on other platforms):

```powershell
npm install --prefix build/browser-tools playwright --no-save
node tests/test_browser.mjs
```

The browser script launches its own C process on port 18090, checks desktop and phone layouts, exercises real passenger trips and controls, validates shared state/offline controls, and writes screenshots under `build/`. HTTP tests use port 18089. Test tools never replace the C backend.

## Evolution diagrams

- [Before: architecture and two interaction sequences](evolution/01-before.md), captured before implementation changes.
- [After: architecture and three interaction sequences](evolution/02-after.md), including mixed passengers and emergency stop behavior.
- [Rendered diagram gallery](evolution/diagrams.html); SVG and Mermaid source exports are in [evolution/diagrams](evolution/diagrams).
- [Internet access architecture and sequence](evolution/03-public-access.md).
- [Embedded telemetry and passenger feedback architecture and sequences](evolution/04-telemetry.md).
- [Direct network access architecture and sequence](evolution/05-network-access.md).
- [Evolution viewer architecture and publishing sequence](evolution/06-evolution-viewer.md).
- [Live run sequence and combined export architecture and sequences](evolution/07-live-sequence-and-export.md).
- [Metriczone visual identity architecture and sequence](evolution/08-metriczone-identity.md).
- [Source commenting conventions architecture and verification sequence](evolution/09-commenting-conventions.md).

To regenerate the offline gallery and SVG/Mermaid pairs after editing the diagram sources:

```powershell
npm install --prefix build/browser-tools playwright --no-save
npm install --prefix build/diagram-tools mermaid --no-save
node tools/render-diagrams.mjs
node tools/check-evolution.mjs
```

The generator reads `docs/evolution/stages.json` and publishes the same stage snapshots to both the offline gallery and `web/evolution/`. The web timeline discovers the generated catalog, so adding a stage does not require another UI or C route change. Each stage must include at least one architecture and one sequence diagram. The checker verifies source hashes, complete diagram coverage and matching generated web assets using only Node's standard library; it runs in CI and in PowerShell builds when Node is installed. Rendering dependencies are development tools, not requirements for the running web interface.

The repository's `AGENTS.md` records the user's requirement to append and publish diagrams with every meaningful implementation step while preserving older snapshots. Optional end-to-end checks use `node tests/test_evolution.mjs` and the same Playwright installation described above.

The dashboard's **Explore the diagrams** links and sidebar expose both **Current run sequence** and **Architecture & sequences**. The live sequence follows the current experiment's latest 20 runtime interactions, ordered by simulation tick, with a current cabin status. Pause freezes the view while the simulator continues. Resume follows the newest state, including a new run after reset. You can download its SVG or read the interactions as text. Delayed runtime updates are labelled; the full persisted event history remains in Telemetry & experiments. The live SVG is drawn locally without a Mermaid runtime or an external service.

Optional browser verification: `node tests/test_live_dashboard.mjs build/evolution/elevator_web.exe` checks real journey progression, pause/resume/reset, all-record exports and mobile layouts.

## Direct network access on port 8099

The Windows scripts read `lab-network.json`. This lab saves `ListenAddress` as `0.0.0.0` and allows `lazythought.cse.chalmers.se`, so normal starts retain direct hostname access. Command-line parameters override those settings. The standalone C executable defaults to loopback when no listen address is given.

```powershell
.\run-web.ps1 -Port 8099 -ListenAddress 0.0.0.0
# Or retain the HTTPS tunnel while also enabling direct access:
.\run-public.ps1 -Port 8099 -ListenAddress 0.0.0.0
# Accept the computer's DNS name as well as its IP address:
.\run-public.ps1 -Port 8099 -ListenAddress 0.0.0.0 -AllowedHost lazythought.cse.chalmers.se
```

In an administrator PowerShell, allow TCP port 8099 on all profiles, matching the 5G and SDV dashboards:

```powershell
.\tools\enable-lab-firewall.ps1
# Optional restriction to a particular executable:
.\tools\enable-lab-firewall.ps1 -Program "$PWD\build\elevator_web.exe"
```

Open `http://lazythought.cse.chalmers.se:8099/` or `http://<this-computer-IPv4-address>:8099` from another device. The firewall rule covers all Windows network profiles, including Public. Direct access requires the network between the devices to permit inbound TCP 8099; routers or institutional firewalls may require a separate rule. All visitors share the simulator controls. For tunnel-only access without an inbound firewall exception, run `.\run-public.ps1 -ListenAddress 127.0.0.1 -AllowedHost @()`.

DNS names must be present in `lab-network.json` or supplied with `-AllowedHost hostname` (also supported by `run-web.ps1`). Multiple names can be passed as a PowerShell array. The C executable accepts repeated `--allowed-host hostname` options, without a scheme or port. Each name is allowed only on the configured server port; browser commands must use that same HTTP origin. Apply saved settings to a running session with `.\tools\restart-public-server.ps1`; it retains the tunnel URL and telemetry database. To create a new tunnel, stop the existing session first with `.\stop-public.ps1`.

Verify network binding and host/origin checks with `node tests/test_network.mjs build/network/elevator_web.exe` after compiling the updated server to that path, or pass another build path.

## Share over the internet

```powershell
.\run-public.ps1                 # Check port 8099, build C, and publish an HTTPS tunnel
.\run-public.ps1 -Port 8100      # Choose a different free local port
Get-Content build/public/session.json # Read the current public URL and process details
.\stop-public.ps1                # Stop this session's tunnel and C server
```

The script downloads the official Windows x64 cloudflared release if necessary and checks its pinned SHA-256. No account, router port forwarding, inbound firewall exception, or administrator access is needed for tunnel access. The C server's direct binding and hostname allowlist come from `lab-network.json`; direct TCP 8099 access uses the separate firewall rule above. The public URL uses standard HTTPS port 443 and forwards to C on local port 8099. Use the printed public URL or configured network hostname from other computers; `localhost:8099` only reaches this computer.

This is a temporary Quick Tunnel: keep the computer awake and online. All visitors share one live simulator and persistent experiment history, and can operate its controls. The URL changes when the tunnel restarts; no auto-start on Windows login is configured. The current URL and process information are stored under ignored `build/public/`. If local DNS has negatively cached a newly generated hostname, use `http://localhost:8099` on this computer while it propagates. See [Cloudflare's Quick Tunnel documentation](https://developers.cloudflare.com/tunnel/get-started/quick-tunnels/) for availability and hostname limitations.
