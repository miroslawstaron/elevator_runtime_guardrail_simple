/****************************************
*
* Elevator Lab -- Experiment Explorer, Feedback and Exports
*
* SPDX-License-Identifier: MIT
*
******************************************/

'use strict';
(() => {
  /**
   * Look up an experiment or feedback element by its stable ID
   */
  const el = id => document.getElementById(id);
  const lab = window.ElevatorLab;
  /**
   * Escape data before building table cells, options or feedback attributes
   */
  const html = value => String(value).replace(/[&<>"']/g, c => ({'&':'&amp;', '<':'&lt;', '>':'&gt;', '"':'&quot;', "'":'&#39;'}[c]));
  let currentRun = 0, selectedRun = 0, followLive = true, offset = 0, lastPage = null;
  let summary = null, queryVersion = 0, runVersion = 0, refreshBusy = false, exporting = false;
  const runs = new Map();
  let nextRunOffset = 0;
  /**
   * Measure actual UTF-8 bytes to match backend text limits
   */
  const bytes = text => new TextEncoder().encode(text).length;
  /**
   * Mark a control unavailable independently of connection and in-flight request state
   */
  function unavailable(button, value) { button.dataset.unavailable = String(value); lab.updateControls(); }
  /**
   * Mark a control busy and ask the shared control policy to refresh its disabled state
   */
  function busy(button, value) { button.dataset.busy = String(value); lab.updateControls(); }
  /**
   * Display a recorder error or clear the error region when the message is empty
   */
  function showError(message) { el('telemetry-error').textContent = message; el('telemetry-error').hidden = !message; }
  /**
   * Perform a bounded telemetry request without treating its response as elevator state
   * Throw a readable API error for unsuccessful HTTP responses
   */
  async function request(path, values) {
    const controller = new AbortController();
    const timeout = setTimeout(() => controller.abort(), 8000);
    try {
      const response = await fetch(path, values ? {method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:new URLSearchParams(values),signal:controller.signal} : {signal:controller.signal});
      const result = await response.json();
      if (!response.ok) throw new Error(result.error || 'The recorder could not complete this request.');
      return result;
    } finally { clearTimeout(timeout); }
  }
  /**
   * Build a telemetry read URL using encoded dataset parameters
   */
  function endpoint(dataset, parameters) { return `/api/telemetry/${dataset}?${new URLSearchParams(parameters)}`; }
  /**
   * Capture the selected experiment, tick range and page size
   * Include a passenger filter only for datasets that support it
   */
  function parameters() {
    const data = {runId:selectedRun,fromTick:el('telemetry-from').value || '0',limit:el('telemetry-limit').value};
    if (el('telemetry-to').value) data.toTick = el('telemetry-to').value;
    if (el('telemetry-passenger').value && ['passengers','feedback'].includes(el('telemetry-dataset').value)) data.passengerId = el('telemetry-passenger').value;
    return data;
  }
  /**
   * Render known experiment identities in newest-first order
   * Avoid replacing unchanged options so selection remains stable during refresh
   */
  function renderRunPicker() {
    const sorted = [...runs.values()].sort((a,b) => b.runId - a.runId);
    const options = sorted.map(run => `<option value="${run.runId}">${html(run.name)} · #${run.runId}${run.runId === currentRun ? ' · Live' : ''}</option>`).join('');
    if (el('experiment-run').dataset.options !== options) { el('experiment-run').innerHTML = options; el('experiment-run').dataset.options = options; }
    el('experiment-run').value = String(selectedRun);
  }
  /**
   * Load the newest or next older experiment page into the local identity map
   * Ignore replies superseded by a newer run-list request
   */
  async function loadRuns(older = false) {
    const version = ++runVersion;
    const page = await request(endpoint('runs',{limit:200,offset:older ? nextRunOffset : 0}));
    if (version !== runVersion) return;
    page.rows.forEach(run => runs.set(run.runId,run));
    nextRunOffset = page.nextOffset;
    el('older-runs').hidden = !page.hasMore;
    if (!selectedRun) selectedRun = currentRun || page.rows[0]?.runId || 0;
    renderRunPicker();
  }
  /**
   * Read aggregate statistics for the selected experiment
   * Discard a reply when the user switched experiments while it was in flight
   */
  async function loadSummary() {
    const target = selectedRun;
    const data = await request(endpoint('summary',{runId:target}));
    if (target !== selectedRun) return;
    summary = data.rows[0];
    if (!summary) return;
    el('recorded-samples').textContent = summary.sampleCount.toLocaleString();
    el('recorded-quality').textContent = summary.missedWrites || summary.lostEvents ? `${summary.missedWrites} write failures · ${summary.lostEvents} missing events` : '100 ms between ticks';
    el('recorded-journeys').textContent = String(summary.completedCount);
    el('recorded-run-id').textContent = `Experiment #${summary.runId} · ${summary.passengerCount} passengers`;
    el('recorded-wait').textContent = summary.averageWaitSeconds === null ? '—' : `${summary.averageWaitSeconds.toFixed(1)}s`;
    el('recorded-ride').textContent = summary.averageRideSeconds === null ? 'Ride time not yet measured' : `${summary.averageRideSeconds.toFixed(1)}s average ride`;
    el('recorded-feedback').textContent = String(summary.feedbackCount);
    el('recorded-rating').textContent = summary.averageRating === null ? 'No ratings yet' : `${summary.averageRating.toFixed(1)} / 5 average rating`;
  }
  /**
   * Render the server-provided schema, typed values and pagination metadata
   * Preserve table columns for empty results and add feedback buttons to journey rows
   */
  function renderPage(page) {
    const columns = page.columns || (page.rows.length ? Object.keys(page.rows[0]) : []);
    const passengerRows = page.dataset === 'passengers';
    el('telemetry-head').innerHTML = columns.length ? `<tr>${columns.map(c => `<th>${html(c.replace(/([A-Z])/g,' $1').toUpperCase())}</th>`).join('')}${passengerRows ? '<th>FEEDBACK</th>' : ''}</tr>` : '';
    el('telemetry-rows').innerHTML = page.rows.map(row => `<tr>${columns.map(c => `<td>${row[c] === null ? '—' : html(row[c])}</td>`).join('')}${passengerRows ? `<td><button type="button" class="feedback-button" data-feedback-id="${row.passengerId}" data-feedback-run="${row.runId}" data-feedback-name="${html(row.name)}">Feedback ↗</button></td>` : ''}</tr>`).join('');
    el('telemetry-empty').hidden = page.rows.length > 0;
    el('telemetry-empty').textContent = 'No records match these filters. Try another experiment or tick range.';
    el('telemetry-result-label').textContent = `${page.dataset === 'samples' ? 'Elevator samples' : page.dataset === 'passengers' ? 'Passenger journeys' : page.dataset === 'events' ? 'Runtime events' : 'Passenger feedback'} · Experiment #${page.runId}`;
    el('telemetry-page').textContent = page.rows.length ? `Rows ${page.offset + 1}–${page.offset + page.rows.length}${page.hasMore ? ' · more available' : ''}` : '0 records';
    unavailable(el('telemetry-previous'),page.offset === 0);
    unavailable(el('telemetry-next'),!page.hasMore);
  }
  /**
   * Fetch a filtered page and apply only the latest issued table query
   * Keep the direct API link aligned with the page shown to the user
   */
  async function loadPage() {
    if (!selectedRun) return;
    const version = ++queryVersion;
    const dataset = el('telemetry-dataset').value;
    const url = endpoint(dataset,{...parameters(),offset});
    busy(el('query-telemetry'),true);
    try {
      const page = await request(url);
      if (version !== queryVersion) return;
      lastPage = page;
      el('telemetry-api-link').href = url;
      el('telemetry-api-link').textContent = url;
      renderPage(page); showError('');
    } catch (error) { if (version === queryVersion) showError(error.message); }
    finally { if (version === queryVersion) busy(el('query-telemetry'),false); }
  }
  /**
   * Refresh experiments, summary and the visible first page without overlapping refreshes
   * Skip background work when the explorer is off-screen unless explicitly requested
   */
  async function refresh(force = false) {
    if (!currentRun || refreshBusy || (!force && !el('telemetry').matches(':target') && el('telemetry').getBoundingClientRect().top > innerHeight)) return;
    refreshBusy = true;
    try { await loadRuns(); await loadSummary(); if (offset === 0 || force) await loadPage(); }
    catch (error) { showError(error.message); }
    finally { refreshBusy = false; }
  }
  // LIVE RUN TRACKING: Follow resets only while the explorer is following the current run
  window.addEventListener('elevator-state', event => {
    const value = event.detail;
    if (!value.runId) return;
    el('recording-status').classList.toggle('recording-error',!value.telemetry.healthy);
    el('recording-status').textContent = value.telemetry.healthy ? '● Recording · 10 Hz · SQLite' : 'Recording interrupted · retrying';
    if (value.runId !== currentRun) {
      currentRun = value.runId;
      if (followLive || !selectedRun) { selectedRun = currentRun; offset = 0; }
      refresh(true);
    }
  });
  // EXPERIMENT SELECTION: Keep historical exploration separate from the current live run
  el('experiment-run').addEventListener('change',async () => {
    selectedRun = Number(el('experiment-run').value); followLive = selectedRun === currentRun; offset = 0;
    try { await loadSummary(); await loadPage(); } catch (error) { showError(error.message); }
  });
  el('older-runs').addEventListener('click',() => loadRuns(true).catch(error => showError(error.message)));
  // NEW EXPERIMENT: Ask C to archive and reset before refreshing the explorer
  el('experiment-form').addEventListener('submit',async event => {
    event.preventDefault();
    const name = el('experiment-name').value.trim();
    if (!name || bytes(name) > 80) { showError('Use an experiment name up to 80 UTF-8 bytes.'); return; }
    busy(el('start-experiment'),true);
    try { followLive = true; await lab.api('/api/experiments',{name}); el('experiment-name').value = ''; await refresh(true); lab.notify('New experiment started. Previous data is saved.'); }
    catch (error) { showError(error.message); }
    finally { busy(el('start-experiment'),false); }
  });
  // QUERY CONTROLS: Reset pagination when filters or datasets change
  el('telemetry-filter').addEventListener('submit',event => { event.preventDefault(); offset = 0; loadPage(); });
  el('telemetry-dataset').addEventListener('change',() => { el('passenger-filter-field').hidden = !['passengers','feedback'].includes(el('telemetry-dataset').value); offset = 0; loadPage(); });
  el('telemetry-previous').addEventListener('click',() => { offset = Math.max(0,offset - Number(el('telemetry-limit').value)); loadPage(); });
  el('telemetry-next').addEventListener('click',() => { if (lastPage?.hasMore) { offset = lastPage.nextOffset; loadPage(); } });

  /**
   * Serialize one CSV cell while preserving numbers and empty values
   * Quote text, double embedded quotes and prefix formula-like strings for spreadsheet imports
   */
  function csvCell(value) {
    if (value === null || value === undefined) return '';
    if (typeof value === 'number') return String(value);
    let text = String(value);
    if (/^[=+\-@\t\r\n]/.test(text)) text = "'" + text;
    return '"' + text.replace(/"/g,'""') + '"';
  }
  /**
   * Download every page of the selected dataset or all four record types
   * Capture the run and filters before asynchronous reads so a reset cannot change export identity
   * Use a shared upper tick and retain original fields when merging typed records
   */
  async function exportData(format) {
    if (exporting || !selectedRun) return;
    exporting = true;
    busy(el('export-json'),true); busy(el('export-csv'),true);
    try {
      const all = el('telemetry-export-scope').value === 'all';
      const dataset = all ? 'all' : el('telemetry-dataset').value;
      const filters = {...parameters(),limit:200};
      // EXPORT 1: A combined download covers all passengers, regardless of the passenger-only filter
      if (all) delete filters.passengerId;
      // EXPORT 2: Fix one cutoff for all pages when the user did not supply an end tick
      if (!filters.toTick) {
        const snapshot = await request(endpoint('summary',{runId:filters.runId}));
        filters.toTick = snapshot.rows[0].lastTick;
      }
      const rows = []; let columns = all ? ['recordType','runId','tick'] : [];
      const datasets = all ? ['samples','events','passengers','feedback'] : [dataset];
      const counts = {};
      // EXPORT 3: Read every API page and attach a record type when merging datasets
      for (const kind of datasets) {
        let next = 0, more = true; counts[kind] = 0;
        while (more) {
          const page = await request(endpoint(kind,{...filters,offset:next}));
          counts[kind] += page.rows.length;
          rows.push(...page.rows.map(row => all ? {recordType:kind,...row,tick:row.tick ?? row.addedTick} : row));
          columns = all ? [...new Set([...columns,...page.columns])] : page.columns;
          next = page.nextOffset; more = page.hasMore;
        }
      }
      // EXPORT 4: Order the combined records by simulation tick, then dataset order
      if (all) rows.sort((a,b) => a.tick - b.tick || datasets.indexOf(a.recordType) - datasets.indexOf(b.recordType));
      const contents = format === 'json' ? JSON.stringify({schemaVersion:1,tickDurationMs:100,exportedAt:new Date().toISOString(),runId:filters.runId,dataset,filters,...(all ? {counts,journeyTimeBasis:'addedTick; journey fields reflect the time each page was read'} : {}),columns,rows},null,2) : [columns.map(csvCell).join(','),...rows.map(row => columns.map(c => csvCell(row[c])).join(','))].join('\r\n');
      // DOWNLOAD: Release the temporary object URL after the browser has begun saving the file
      const url = URL.createObjectURL(new Blob([contents],{type:format === 'json' ? 'application/json;charset=utf-8' : 'text/csv;charset=utf-8'}));
      const link = document.createElement('a'); link.href = url; link.download = `elevator-run-${filters.runId}-${dataset}.${format}`; link.click(); setTimeout(() => URL.revokeObjectURL(url),30000);
      lab.notify(`Exported ${rows.length} records${all ? ' across all four types' : ` from ${dataset}`}.`);
    } catch (error) { showError(error.message); }
    finally { exporting = false; busy(el('export-json'),false); busy(el('export-csv'),false); }
  }
  el('export-json').addEventListener('click',() => exportData('json'));
  el('export-csv').addEventListener('click',() => exportData('csv'));
  el('telemetry-export-scope').addEventListener('change',() => {
    el('export-scope-note').textContent = el('telemetry-export-scope').value === 'all'
      ? 'Includes every page of samples, runtime events, journey summaries and feedback for this experiment and tick range. Ignores Passenger ID. Journeys use their added tick; journey status is read live.'
      : 'Exports all pages of the selected data set within the tick range.';
  });

  // FEEDBACK 1: Capture the run/passenger pair when the form opens, even if the run later resets
  document.addEventListener('click',event => {
    const button = event.target.closest('[data-feedback-id]');
    if (!button || button.disabled) return;
    el('feedback-form').reset(); el('feedback-error').hidden = true;
    el('feedback-run').value = button.dataset.feedbackRun;
    el('feedback-person').value = button.dataset.feedbackId;
    el('feedback-passenger').textContent = `${button.dataset.feedbackName} · Passenger #${button.dataset.feedbackId} · Experiment #${button.dataset.feedbackRun}`;
    el('feedback-byte-count').textContent = '0 / 2000 UTF-8 bytes';
    el('feedback-dialog').showModal();
  });
  el('feedback-text').addEventListener('input',() => { el('feedback-byte-count').textContent = `${bytes(el('feedback-text').value)} / 2000 UTF-8 bytes`; });
  el('cancel-feedback').addEventListener('click',() => el('feedback-dialog').close());
  el('feedback-dialog').addEventListener('cancel',event => { if (el('save-feedback').dataset.busy === 'true') event.preventDefault(); });
  // FEEDBACK 2: Validate UTF-8 bytes, submit the captured identity and refresh saved feedback
  el('feedback-form').addEventListener('submit',async event => {
    event.preventDefault();
    const text = el('feedback-text').value.trim();
    el('feedback-error').hidden = true;
    if (!text || bytes(text) > 2000) { el('feedback-error').textContent = 'Enter feedback up to 2000 UTF-8 bytes.'; el('feedback-error').hidden = false; return; }
    busy(el('save-feedback'),true); busy(el('cancel-feedback'),true);
    try {
      const values = {runId:el('feedback-run').value,passengerId:el('feedback-person').value,text};
      if (el('feedback-rating').value) values.rating = el('feedback-rating').value;
      await request('/api/feedback',values);
      el('feedback-dialog').close(); lab.notify('Passenger feedback saved.');
      await refresh(true);
    } catch (error) { el('feedback-error').textContent = error.message; el('feedback-error').hidden = false; }
    finally { busy(el('save-feedback'),false); busy(el('cancel-feedback'),false); }
  });
  window.addEventListener('hashchange',() => { if (location.hash === '#telemetry') refresh(true); });
  // BACKGROUND REFRESH: Periodically update visible telemetry without driving simulation time
  setInterval(() => refresh(),3000);
  const initial = lab.getState();
  if (initial) window.dispatchEvent(new CustomEvent('elevator-state',{detail:initial}));
})();
