/****************************************
 * Elevator Lab -- Explainable Telemetry Steering Recommendations
 * SPDX-License-Identifier: MIT
 ****************************************/
'use strict';
(() => {
  const el = id => document.getElementById(id), lab = window.ElevatorLab;
  let version = 0, reportRun = 0;
  /** Format nullable measurements without representing missing observations as zero. */
  function number(value, suffix = '') { return value === null || value === undefined ? '—' : `${Number(value).toFixed(1)}${suffix}`; }
  /**
   * Build deterministic hypotheses from measured evidence, with explicit thresholds.
   * These rules do not simulate alternative policies or claim a measured improvement.
   */
  function recommendations(m) {
    const items = [];
    if (m.missedWrites || m.lostEvents) items.push(['Repair measurement gaps first',`${m.missedWrites} failed writes and ${m.lostEvents} lost events were recorded. Re-run the same seed with healthy recording before comparing algorithms.`]);
    if (m.guardViolations) items.push(['Investigate guardrail violations',`${m.guardViolations} samples reported a safety violation. Review those samples and events before evaluating any steering change.`]);
    if (m.releasedCount < 10 || m.boardedCount < 10) items.push(['Collect more completed journeys',`Only ${m.releasedCount} people have been released and ${m.boardedCount} have boarded. This is preliminary evidence; let traffic run before tuning policy.`]);
    if (m.releasedCount >= 10 && m.lobbyDepartures / m.releasedCount >= 0.6) items.push(['Test a morning lobby-return policy',`${m.lobbyDepartures}/${m.releasedCount} released journeys start at floor 0 (at least 60%). After completing rider stops, send an empty cabin back to the lobby while its queue is nonempty; test grouping compatible destinations.`]);
    if (m.releasedCount >= 10 && m.lobbyArrivals / m.releasedCount >= 0.6) items.push(['Test demand-weighted afternoon pickups',`${m.lobbyArrivals}/${m.releasedCount} released journeys end at floor 0 (at least 60%). Score pickup floors by queue size and oldest wait, then sweep downward with compatible riders; retain age-based fairness.`]);
    if ((m.p95WaitSeconds ?? 0) > 60 || (m.oldestWaitingSeconds ?? 0) > 120) items.push(['Test earlier age-based pickup priority',`The boarded-passenger p95 wait is ${number(m.p95WaitSeconds,'s')}; the oldest still-waiting passenger has waited ${number(m.oldestWaitingSeconds,'s')}. Compare the current 120-second aging threshold with 60 and 90 seconds, preserving rider destinations.`]);
    if ((m.fullTravelPercent ?? 0) >= 30 && m.peakQueue > 8) items.push(['Batch compatible destinations under saturation',`The cabin is full in ${number(m.fullTravelPercent,'%')} of motion samples and the queue peaked at ${m.peakQueue}. Test destination grouping and fewer redundant pickup stops; keep the eight-seat limit. Steering alone may not meet the arrival rate.`]);
    if ((m.emptyTravelPercent ?? 0) >= 25) items.push(['Test demand-based idle positioning',`${number(m.emptyTravelPercent,'%')} of motion samples have an empty cabin. Compare parking near the next scheduled demand with the current scan route; avoid repositioning while riders remain aboard.`]);
    if (m.vipWaitSeconds !== null && m.regularWaitSeconds > 30 && m.regularWaitSeconds > m.vipWaitSeconds * 1.5) items.push(['Bound consecutive VIP pickups',`Regular passengers average ${number(m.regularWaitSeconds,'s')} waiting versus ${number(m.vipWaitSeconds,'s')} for VIPs. Test a maximum consecutive VIP pickup budget while retaining aging, then compare both classes' tail waits.`]);
    if (m.stoppedSeconds > 10) items.push(['Separate emergency-stop time from dispatch comparisons',`${number(m.stoppedSeconds,'s')} of recorded simulation time was emergency-stopped. Re-run without operator stops to isolate steering effects; preserve all safety interlocks.`]);
    if (m.averageRating !== null && m.averageRating < 3) items.push(['Inspect passenger feedback alongside timing',`${m.feedbackCount} feedback entries have an average rating of ${number(m.averageRating,' / 5')}. Read the comments to identify whether complaints concern pickup order, waiting, or ride duration before selecting a change.`]);
    if (!items.length) items.push(['Establish a comparison before changing steering',`This run has ${m.completedCount} completed journeys and a p95 wait of ${number(m.p95WaitSeconds,'s')}. No configured warning threshold fired. Compare a candidate policy against this run using the same traffic configuration.`]);
    return items;
  }
  /** Invalidate a report when its experiment changes; discard outstanding responses too. */
  function invalidate() {
    version++; reportRun = 0;
    el('analysis-results').hidden = true;
    el('analysis-status').textContent = 'Ready to analyze the selected experiment. Full run; table filters do not apply. Suggestions do not change the running algorithm.';
    el('analyze-telemetry').dataset.busy = 'false'; lab.updateControls();
  }
  el('experiment-run').addEventListener('change',invalidate);
  // ANALYZE: Fix the selected run identity and render only the latest requested snapshot.
  el('analyze-telemetry').addEventListener('click',async () => {
    const runId = Number(el('experiment-run').value);
    if (!runId) return;
    const requestVersion = ++version;
    el('analysis-results').hidden = true;
    el('analysis-status').textContent = `Analyzing stored telemetry for experiment #${runId}…`;
    el('analyze-telemetry').dataset.busy = 'true'; lab.updateControls();
    const controller = new AbortController(), timer = setTimeout(() => controller.abort(),10000);
    try {
      const response = await fetch(`/api/telemetry/analysis?runId=${runId}`,{signal:controller.signal});
      const data = await response.json();
      if (!response.ok) throw new Error(data.error || 'Analysis unavailable.');
      if (requestVersion !== version || Number(el('experiment-run').value) !== runId) return;
      const m = data.rows[0];
      if (!m) throw new Error('No telemetry found for this experiment.');
      reportRun = runId;
      el('analysis-status').textContent = `${m.name} · #${runId} · Snapshot through tick ${m.lastTick} (${number(m.lastTick / 10,' simulation seconds')}) · ${m.endedAtMs === null ? 'Live / preliminary' : 'Archived'} · ${m.sampleCount} samples · ${m.completedCount} arrived / ${m.passengerCount} planned · ${m.waitingCount} waiting / ${m.ridingCount} riding / ${m.scheduledCount} scheduled / ${m.cancelledCount} cancelled. Deterministic rule-based suggestions; no alternative algorithm has been benchmarked.`;
      el('analysis-metrics').replaceChildren();
      const metrics = [['Average wait',number(m.averageWaitSeconds,'s')],['p95 wait',number(m.p95WaitSeconds,'s')],['Oldest waiting',number(m.oldestWaitingSeconds,'s')],['Peak queue',String(m.peakQueue ?? '—')],['Moving cabin use',number(m.movingLoadPercent,'%')],['Empty travel',number(m.emptyTravelPercent,'%')],['VIP / regular wait',`${number(m.vipWaitSeconds,'s')} / ${number(m.regularWaitSeconds,'s')}`],['Average ride',number(m.averageRideSeconds,'s')]];
      for (const [label,value] of metrics) {
        const cell = document.createElement('div'), strong = document.createElement('strong');
        cell.textContent = label; strong.textContent = value; cell.append(strong); el('analysis-metrics').append(cell);
      }
      el('analysis-recommendations').replaceChildren();
      for (const [title,evidence] of recommendations(m)) {
        const item = document.createElement('li'), strong = document.createElement('strong');
        strong.textContent = title; item.append(strong,document.createElement('br'),document.createTextNode(evidence));
        el('analysis-recommendations').append(item);
      }
      el('analysis-next-step').textContent = `Next experiment: replay the same scenario, seed and arrival streams; change one steering rule at a time. Compare p95/max waits, VIP and regular waits, ride times and completion rate, with zero guardrail violations. Wait statistics include ${m.boardedCount} boarded passengers only; waiting ages are reported separately, and future arrivals are excluded from demand. Occupancy percentages use motion samples, not energy measurements. Re-analyze to refresh this snapshot.`;
      el('analysis-results').hidden = false;
    } catch (error) { if (requestVersion === version) el('analysis-status').textContent = `Analysis failed: ${error.message}`; }
    finally { clearTimeout(timer); if (requestVersion === version) { el('analyze-telemetry').dataset.busy = 'false'; lab.updateControls(); } }
  });
  window.addEventListener('elevator-state',() => { if (reportRun && Number(el('experiment-run').value) !== reportRun) invalidate(); });
})();
