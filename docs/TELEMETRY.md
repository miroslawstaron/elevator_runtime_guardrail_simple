# Telemetry, experiments, and passenger feedback

The running C server embeds SQLite. It records independently of browser connections, so closing a browser does not stop recording. The original elevator state machine and guardrails remain unchanged. The database, recorder, passenger dispatcher, and HTTP query/feedback endpoints all execute in the same C backend.

## Use the web interface

Open **Telemetry & experiments** in the sidebar. Select a current or historical experiment, then choose passenger journeys, elevator samples, runtime events, or passenger feedback. Filter by simulation tick, and optionally passenger ID for passenger/feedback data. Page through records or export the entire filtered result as JSON/CSV; exports collect every API page rather than just the visible rows. The API link beneath the table exposes the current query for use in other tools.

**Start experiment** creates a named run and resets the live elevator. The existing Reset button also creates a new run. Neither operation deletes recorded history. Completed journeys remain completed; passengers still scheduled, waiting or riding when a run ends become `cancelled` in its historical records. A restart starts a new run instead of restoring the previous live cabin. Prior history and feedback remain available.

Every passenger row has a **Feedback** button, including in historical experiment results. Feedback can describe the wait, ride, or overall experience at any stage. Text is required; a 1–5 rating is optional. Multiple entries are retained, allowing feedback at different stages of a journey. A form opened before a reset still submits against its original run and passenger. Names and feedback are displayed as text, and are shared with other visitors and included in exports.

Average wait uses `boardedTick - addedTick` for passengers who boarded. Average ride uses `arrivedTick - boardedTick` only for completed trips. Both are multiplied by 0.1 to convert ticks to seconds. Missing measurements and unrated feedback are `null`, never zero. The summary always covers the entire selected run, independently of table filters.

## Storage and ownership

Default database: `data/elevator.sqlite3`, relative to the server's working directory. The supplied startup scripts run from the repository. Override it using:

```powershell
.\build\elevator_web.exe --port 8099 --database "C:\experiments\elevator.sqlite3" --web-root web
```

The custom parent directory must already exist. The default `data/` directory is created automatically. The public-start script uses the same persistent default database. SQLite source is vendored from its official distribution; builds need neither a SQLite installation nor an external database service. Schema version is `PRAGMA user_version=3`; versions 1 and 2 are upgraded in place without deleting existing records. Version 3 adds a default-zero VIP flag.

One simulator may own a database at a time. A sidecar `.lock` file prevents a second simulator from marking active runs interrupted or writing conflicting state. The operating system releases ownership if the process crashes; the empty lock file itself is harmless. Other SQLite tools may read the database normally. WAL mode and full synchronization protect committed transactions. Database files, WAL/SHM sidecars, and lock files are ignored by Git.

Recording is append-only across runs. There is no automatic retention/deletion policy, so disk use grows while the service runs, including idle periods. At 1× playback, ten samples per second means 864,000 samples per day, plus indexes, journeys, events, and feedback. For a consistent backup, use SQLite's backup API or stop the C server before copying the database. Copying only the main file of a running WAL database can omit committed records still in the WAL. The raw database is not served over HTTP.

## What is recorded

| Table | Identity and contents |
| --- | --- |
| `runs` | Globally increasing run ID, name, UTC start/end in Unix milliseconds, last tick, end reason, failed write attempts and missing event count |
| `samples` | One row per `(run_id,tick)`: timestamp, integer floor/target, fractional position, state, direction, doors, stop flags/duration, occupancy, waiting/completed counts, stops, pending floor mask, guardrail flags |
| `passengers` | `(run_id,passenger_id)`, name, origin/destination, lifecycle, added/boarded/arrived ticks, update timestamp |
| `events` | Durable copies of simulation events with source ID, run ID, tick, timestamp and message |
| `feedback` | Entry ID, run/passenger foreign key, timestamp, submission tick, exact UTF-8 text, optional rating |

A sample is captured every 100 ms of simulation time, including during an emergency stop. At 5× or 20× playback, each simulation step is still captured; wall-clock recording volume increases accordingly. Successful commands also capture state immediately. If several commands occur within a tick, its sample is updated to the latest observed state; their events are retained separately. Passenger rows update on lifecycle changes. The recorder copies new events before the simulation's small display ring normally overwrites them.

Ticks are simulation time, starting at zero for each run; wall timestamps are UTC Unix milliseconds. For historical feedback submitted after a run ended, `tick` is the final tick of that run and `createdAtMs` is the actual later submission time. Use timestamps for comparisons across runs. `requestMask` is a ten-bit mask: bit `n` indicates a requested floor `n`, including eligible pickups and onboard destinations. Upward direction is `1`, downward `-1`; boolean sample fields are `0`/`1`.

The recorder commits each capture atomically. If a write fails, the elevator continues, the UI reports recording interrupted, and recording retries once per second. Successful recovery captures current passenger state and available events. Missed samples cannot be reconstructed. If the event ring overflowed during the gap, `lostEvents` reports how many messages were unavailable. `missedWrites` counts failed capture attempts, not the exact number of missing ticks. Both counters persist in the run and appear in the data-quality indicator. Tick gaps can be checked directly in sample exports. Startup fails explicitly if the database cannot be initialized, is already owned, or has a newer unsupported schema.

## Read API

All endpoints use the same local/public host checks as elevator controls. No arbitrary SQL is accepted. Queries are fixed and parameters are bound. Responses are bounded and paginated.

| Endpoint | Purpose |
| --- | --- |
| `GET /api/telemetry/runs` | Runs, newest first, with lifecycle and data-quality counters |
| `GET /api/telemetry/analysis?runId=2` | Whole-run steering metrics; use the dedicated analysis button for explained recommendations ([usage](SCENARIOS.md)) |
| `GET /api/telemetry/summary?runId=2` | Whole-run sample/event/passenger/completion/feedback counts, mean wait/ride times and optional mean rating |
| `GET /api/telemetry/samples?runId=2` | Raw elevator observations ordered by tick |
| `GET /api/telemetry/passengers?runId=2` | Journey lifecycle and derived wait/ride durations |
| `GET /api/telemetry/events?runId=2` | Messages ordered by tick and entry ID |
| `GET /api/telemetry/feedback?runId=2` | Text/rating entries joined to the correct passenger |

For sample/passenger/event/feedback data, options are `runId` (defaults to current), `fromTick` (default 0), `toTick` (default last recorded tick), `limit` (1–200, default 100), `offset` (default 0), and `format=json|csv`. Passenger and feedback queries also accept `passengerId=1..1000`. Runs accept only limit/offset/format; summary and analysis additionally accept runId. Unsupported or duplicate options are rejected. Tick ranges are inclusive. Passenger ranges refer to the passenger's **added tick**; event, feedback, and sample ranges use their respective tick columns.

JSON returns `dataset`, `runId`, the resolved tick bounds, limit/offset, `columns`, `rows`, `hasMore`, and `nextOffset`. Continue using `nextOffset` while `hasMore` is true. Supply the first response's resolved `toTick` to later pages to keep a stable upper tick bound. CSV returns one requested page and includes a header even for no results; the web interface assembles all pages for full exports. Downloaded JSON has export-format `schemaVersion=1`, independently of SQLite's database schema version. JSON preserves original text. CSV doubles quotes, retains line breaks inside quoted fields, and prefixes spreadsheet-formula-like text with an apostrophe. A run may receive later feedback while it is being queried; exports are not a multi-request database transaction.

Examples:

```powershell
curl.exe "http://localhost:8099/api/telemetry/runs"
curl.exe "http://localhost:8099/api/telemetry/samples?runId=2&fromTick=0&toTick=500&limit=200"
curl.exe "http://localhost:8099/api/telemetry/feedback?runId=2&format=csv" -o feedback.csv
```

Python experimentation using only the standard library:

```python
import json
from urllib.parse import urlencode
from urllib.request import urlopen

base = "http://localhost:8099"  # Or the currently printed public HTTPS URL.
run_id = 2
query = {"runId": run_id, "limit": 200, "offset": 0}
records = []
while True:
    with urlopen(base + "/api/telemetry/passengers?" + urlencode(query)) as response:
        page = json.load(response)
    records.extend(page["rows"])
    query["toTick"] = page["toTick"]
    if not page["hasMore"]:
        break
    query["offset"] = page["nextOffset"]
completed = [p for p in records if p["status"] == "arrived"]
if completed:
    print("Mean ride seconds:", sum(p["rideSeconds"] for p in completed) / len(completed))
```

Use the SQLite database directly for joins or statistical work, for example:

```sql
SELECT p.run_id, p.passenger_id, p.name,
       (p.boarded_tick-p.added_tick)*0.1 AS wait_seconds,
       (p.arrived_tick-p.boarded_tick)*0.1 AS ride_seconds,
       f.text, f.rating, f.created_at_ms
FROM passengers p
LEFT JOIN feedback f
  ON f.run_id=p.run_id AND f.passenger_id=p.passenger_id
ORDER BY p.run_id, p.passenger_id, f.id;
```

## Write API

`POST /api/experiments` takes form-encoded `name` (1–80 UTF-8 bytes). It archives the current run and returns a fresh authoritative elevator state with a new `runId`.

`POST /api/feedback` requires `runId`, `passengerId`, and meaningful `text` up to 2000 UTF-8 bytes. `rating` is optional, or an integer 1–5. Unicode and line breaks/tabs are supported; invalid UTF-8, NUL, other control characters and whitespace-only messages are rejected. The passenger must already exist in that run. The response includes the new `feedbackId` and the run/passenger identity. Multiple feedback entries are allowed. There is no feedback editing/deletion endpoint.

POST content type is `application/x-www-form-urlencoded`; browser Origin must match the configured local HTTP or public HTTPS origin. Invalid input returns 400, missing experiments/passengers/datasets 404, storage failures 503. All visitors share the simulator and experiment history.

## Verification

In the web interface, **Export records → All record types** combines every page of samples, runtime events, passenger journeys and feedback into one JSON or CSV file. Choose the experiment and tick range first. The passenger-only filter is ignored for combined exports. Each row has a `recordType` (`samples`, `events`, `passengers` or `feedback`) and a normalized `tick`, with all original fields preserved. CSV uses the union of dataset columns and leaves inapplicable cells empty. JSON also includes per-type counts. Single-dataset exports retain their original format.

Without an explicit end tick, the exporter reads one cutoff from the run summary and applies it to every paginated read. Samples, events and feedback use their own ticks; journeys use `addedTick` and contain the journey summary as read, including later boarding/arrival information. Multiple reads are not a single database snapshot. Combined rows sort by tick and record type; equal-tick rows within each dataset keep API order.

`tests/test_live_dashboard.mjs` additionally verifies complete combined exports across API page boundaries, tick bounds including zero, archived-run identity and CSV quoting/formula escaping.

`tests/test_telemetry.c` checks real on-disk persistence, identities/foreign keys, CSV/JSON bounds and escaping, pagination, text validation, exclusive database ownership, write-lock recovery, event-overflow accounting, and interrupted-run recovery. `tests/test_telemetry_web.mjs` exercises the running C/SQLite HTTP service and verifies reset/restart persistence. `tests/test_telemetry_browser.mjs` checks desktop and phone feedback forms, archived data, ratings, filters, pagination, export downloads and safe rendering, including a reset while feedback is being written.

```powershell
.\build.ps1 -Test
node tests/test_telemetry_browser.mjs
```
