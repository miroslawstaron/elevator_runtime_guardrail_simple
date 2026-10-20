# Evolution documentation

The user requires architecture and sequence diagrams for every implementation step, published in the running web interface so the system's evolution remains traceable.

- Treat each meaningful feature, architecture, deployment, or behavior change as a new documented stage. Routine inspection, individual tool calls, and fixes within an unfinished stage do not need separate snapshots.
- Before changing implementation, read the most recent stage and preserve it as the baseline. If an earlier stage has no snapshot, document the observed baseline before implementing the new stage.
- Add a new numbered Markdown file under `docs/evolution/` describing the resulting system. Include at least one architecture diagram (`flowchart`) and one relevant `sequenceDiagram` in Mermaid fences, plus what changed and why. Add more sequences when independent behaviors need explanation.
- Architecture diagrams must show implementation modules and dependencies: identify the actual C source/header pairs, web HTML/JavaScript/CSS files, and relevant build, deployment and documentation modules. Group by subsystem and label relationships such as includes, API use, loads or generates. Mermaid `flowchart` is the rendering syntax, not a request for workflow diagrams. Keep execution order and user workflows in sequence diagrams. Historical architecture views must describe only modules available at their stage.
- Append the stage to `docs/evolution/stages.json`; preserve earlier stage files and IDs. Correcting a factual or rendering error is allowed, but do not rewrite old snapshots to describe newer behavior.
- Run `node tools/render-diagrams.mjs` to update the offline gallery and generated web catalog, SVGs, Mermaid sources and stage notes. The viewer discovers stages from the catalog; do not hardcode another timeline in the UI.
- Run `node tools/check-evolution.mjs` before completing work. Verify the new stage in the browser, including its architecture and sequence diagrams, and keep diagram and source downloads working.
- Include the new stage in the public web interface as part of the implementation. Keep the existing C backend, database, public tunnel, network settings and historical telemetry intact when deploying updates.
- Do not change an existing release tag when adding a stage. Commit/push only when authorized by the user's request.

# Source commenting conventions

- Follow `src/guardrails.c` when adding or changing authored code: use a module-purpose banner, `/** ... */` function descriptions explaining behavior and relevant return values, and inline comments for important checks, state transitions, ordering and cleanup.
- Use numbered comment stages for multi-step logic when they make the flow clearer. Explain the actual implementation and its limits; do not add promises or behavior that the code does not enforce.
- Use the equivalent native comment syntax in PowerShell, HTML, CSS, Make, CMake and CI configuration. Keep JSON valid and annotate its meaning in nearby documentation instead.
- Preserve the original guardrails and elevator core when working on the generated web system. Do not rewrite third-party dependencies or generated diagram outputs merely to add comments; annotate the authored sources and regenerate documentation normally.
