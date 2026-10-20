# Run traffic scenarios and analyze steering

Open **Usage scenarios** in the running Elevator Lab. Select a preset, adjust its
people, arrival window and seed, then press **Run**. Each run archives the previous
experiment, including cancelling unfinished journeys, without deleting its telemetry.

| Scenario | Default people | Arrival window | Routes |
| --- | ---: | ---: | --- |
| Busy morning | 300 | 600 seconds | Floor 0 → seeded floors 1–9 |
| Busy afternoon | 300 | 600 seconds | Seeded floors 1–9 → floor 0 |
| Occasional travellers | 24 | 1,200 seconds | Seeded mixed-floor journeys |
| VIPs | 40 | 300 seconds | Mixed floors, every fifth traveller is VIP |
| Your own traffic | Configurable | Per stream | Explicit origins, destinations and priorities |

Presets spread people evenly from time zero through the chosen arrival window. Zero
seconds releases everyone together. The same seed and settings reproduce the routes.
This is a deterministic simulation, not a statistical arrival distribution.

For custom traffic, **Add arrival stream** creates a row with origin, destination,
count, first arrival in seconds, interval between people, and Regular/VIP priority.
For example, combine 300 regular passengers from 0 to 5, first arrival 0, interval 2,
with 4 VIPs from 0 to 9, first arrival 60, interval 30. Up to 20 streams and 1,000 people
are allowed; every arrival must fall within 86,400 simulation seconds. The custom seed
is recorded for identification; explicit stream routes and timing do not use randomness.

Choose **1×, 5× or 20×** to change how quickly simulation time passes. The C server
continues to execute and record individual 100 ms simulation steps. Start a scenario
first, then choose the speed; every new experiment starts at 1×. Closing the page does
not stop arrivals. **Cancel future arrivals** cancels scheduled people only; existing
waiting and riding people continue. Emergency stop halts the cabin, while time and
scheduled arrivals continue. Restarting the C server archives an interrupted run and
starts a new one; schedules do not resume automatically.

The eight-seat cabin serves compatible directions. VIPs receive pickup priority when
the cabin is empty and priority among compatible people boarding at the same floor.
An ordinary passenger waiting at least 120 seconds outranks new VIPs; aged requests
are ordered by arrival time and then ID. Priority is evaluated at dispatch/service
boundaries and does not guarantee a maximum wait or evict existing riders.

In **Telemetry & experiments**, select the live or an archived experiment, then press
**Analyze telemetry & suggest improvements**. The report covers the entire selected
run, independently of table filters. It shows measured waiting and riding times, p95
wait, oldest unfinished wait, peak queue, cabin use and VIP/regular waits. Future arrivals
do not count as released demand. Wait statistics use boarded people, with nearest-rank
p95; still-waiting ages are separate. Reports remain snapshots until analyzed again.

Recommendations are deterministic rules with visible evidence and thresholds. They
propose experiments such as lobby returns, afternoon queue scoring, earlier aging,
destination grouping, demand-based parking or limits on consecutive VIP pickups.
They do not change steering automatically or claim benchmarked gains. Compare a
candidate with the same traffic and seed, checking tail waits, both priority classes,
completion rate, riding time and guardrails. The dashboard shows at most 100 matching
live journeys; the telemetry explorer provides paged access to the recorded journeys.

## API examples

Commands use URL-encoded form bodies, the same Host/Origin checks as existing controls,
and return the authoritative state. The server validates a full configuration before
archiving the previous run. A storage failure returns 503 rather than silently dropping
the previous experiment.

```text
POST /api/scenarios
kind=morning&count=300&duration=600&seed=42

POST /api/scenarios
kind=custom&count=304&duration=0&seed=42&flows=0,5,300,0,2,0;0,9,4,60,30,1

POST /api/control
action=speed&floor=20

POST /api/control
action=cancel_scenario

GET /api/telemetry/analysis?runId=2
```

The custom `flows` field contains semicolon-separated streams, each with six integer
values: `from,to,count,startSeconds,intervalSeconds,vip`. VIP is 0 or 1; `count` in the
request must equal the sum of stream counts. Preset types are `morning`, `afternoon`,
`occasional` and `vip`; pass their intended counts/windows explicitly (API defaults
are 300 people and 600 seconds). Seed is 1–2,147,483,647. State includes `maxPeople`,
`speed`, `scenario`, `scenarioSeed`, passenger `vip` and scheduled/cancelled states.

Analysis is a fixed, read-only SQLite aggregate dataset supporting runId and the common
limit/offset/format options. It rejects time and passenger filters because its scope is
the whole run. Scenario parameters and custom stream definitions are recorded in durable
events. Schema 3 adds VIP flags in place; older experiment rows default to regular.

The stage 10 [architecture and sequences](evolution/10-traffic-scenarios-and-analysis.md)
are also available in the public **Architecture & sequences** viewer.

Verification: `./build.ps1 -Test` runs C safety/dispatch/scenario/telemetry tests and HTTP
integration checks. With the development Playwright installation, run
`node tests/test_scenarios_browser.mjs` and `node tests/test_evolution.mjs` for real-browser
scenario, report, mobile, diagram and download verification.
