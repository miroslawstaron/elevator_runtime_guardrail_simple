# Elevator Lab

The browser operates the original C elevator through a new C HTTP server. The original terminal program, core state machine, and guardrail implementation remain available. The supplied logo is copied unmodified to `web/assets/agenticai-logo.png` and served locally; there are no frontend network dependencies or frontend build step.

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
5. Try a busy morning adds six varied journeys. Reset clears the shared session and returns to floor 1. All browser windows share one simulation.

The browser polls authoritative snapshots and never drives the simulation clock. Closing a browser does not stop the elevator. Runtime disconnects disable controls and trigger automatic reconnection. Passenger journeys, metrics, and activity are temporary; the limit is 160 passenger records per session, including completed journeys. Reset starts a fresh session.

## API

By default, the server accepts only local hosts. When started with `--public-origin https://hostname`, it also accepts that exact public hostname and requires its exact HTTPS Origin for browser POST commands. Forwarded headers never grant permission to arbitrary origins.

The service accepts HTTP/1.1, closes each response connection, and returns JSON errors. POST bodies must be `application/x-www-form-urlencoded` without a charset suffix. It binds only to `127.0.0.1`, with the exact configured local port or explicitly configured public hostname in the Host header. Browser Origin must match the corresponding local HTTP or public HTTPS origin; command-line clients can omit Origin. There is no CORS support.

| Endpoint | Fields | Result |
| --- | --- | --- |
| `GET /api/state` | None | Authoritative snapshot |
| `POST /api/people` | `name`, `from`, `to`, `count` | Validated group insertion and snapshot |
| `POST /api/control` | `action=request`, `floor=0..9` | Manual floor request |
| `POST /api/control` | `action=open/close/stop/resume/reset/demo` | Control command and snapshot |

Names are limited to 40 UTF-8 bytes and cannot contain control characters. Floors are integers 0–9, origin and destination must differ, and group size is 1–8. Group insertion is atomic. Invalid input returns 400, disallowed host/origin 403, unavailable control 409. Request buffers are capped at 8191 bytes and there are at most 16 concurrent HTTP connections; partial requests time out after three seconds. State includes fractional `position`, integer `floor`/`target`, core state, doors, capacity/occupancy, requests, passengers with timestamps in ticks, guardrail flags, and bounded event history. One tick is 100 ms.

## Validation

`tests/test_simple.c` is the original dependency-free suite (53 assertions). `tests/test_simulation.c` checks passenger lifecycle, opposite directions, full cabins, intermediate pickups, door timing/interlocks, stops both upward/downward, invalid input, JSON escaping/bounds, and deterministic traffic with 160 people. `tests/test_web.mjs` checks real C HTTP behavior, logo/assets, input validation, framing, host/origin checks, stop/resume, and that a partial HTTP request does not block the C clock.

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

To regenerate the offline gallery and SVG/Mermaid pairs after editing the diagram sources:

```powershell
npm install --prefix build/browser-tools playwright --no-save
npm install --prefix build/diagram-tools mermaid --no-save
node tools/render-diagrams.mjs
```

## Share over the internet

```powershell
.\run-public.ps1                 # Check port 8099, build C, and publish an HTTPS tunnel
.\run-public.ps1 -Port 8100      # Choose a different free local port
Get-Content build/public/session.json # Read the current public URL and process details
.\stop-public.ps1                # Stop this session's tunnel and C server
```

The script downloads the official Windows x64 cloudflared release if necessary and checks its pinned SHA-256. No account, router port forwarding, inbound firewall exception, or administrator access is needed. The server remains bound to loopback. The public URL uses standard HTTPS port 443 and forwards to C on local port 8099. Use the printed public URL from other computers; `localhost:8099` only reaches this computer.

This is a temporary Quick Tunnel: keep the computer awake and online. All visitors share one in-memory simulator and can operate its controls. The URL changes on restart; no auto-start on Windows login is configured. The current URL and process information are stored under ignored `build/public/`. If local DNS has negatively cached a newly generated hostname, use `http://localhost:8099` on this computer while it propagates. See [Cloudflare's Quick Tunnel documentation](https://developers.cloudflare.com/tunnel/get-started/quick-tunnels/) for availability and hostname limitations.
