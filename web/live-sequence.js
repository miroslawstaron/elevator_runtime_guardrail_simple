/****************************************
*
* Elevator Lab -- Current Run Sequence Rendering
*
* SPDX-License-Identifier: MIT
*
******************************************/

'use strict';
(() => {
  /**
   * Look up a live sequence element by its dashboard ID
   */
  const el = id => document.getElementById(id);
  const ns = 'http://www.w3.org/2000/svg';
  const actors = ['Passengers','Operator','Dispatcher','Cabin','Recorder'];
  const positions = [100,300,500,700,900];
  let latest = null, shown = null, paused = false, signature = '', lastSeen = 0;
  /**
   * Convert a simulation tick into a one-decimal seconds label
   */
  const stamp = tick => `${(tick / 10).toFixed(1)}s`;
  /**
   * Create an SVG element in the correct namespace with attributes and optional text
   * Assign textContent so passenger or event text cannot become executable SVG markup
   */
  function node(tag, attributes = {}, text) {
    const result = document.createElementNS(ns, tag);
    for (const [key,value] of Object.entries(attributes)) result.setAttribute(key,value);
    if (text !== undefined) result.textContent = text;
    return result;
  }
  /**
   * Choose sequence participants from an actual runtime event
   * Match passenger events against structured journey records; treat other messages as cabin observations
   */
  function interaction(event, state) {
    for (const p of state.people) {
      if (event.message === `${p.name} is waiting: floor ${p.from} to ${p.to}.`) return [0,2];
      if (event.message === `${p.name} boarded at floor ${p.from}, going to ${p.to}.`) return [2,3];
      if (event.message === `${p.name} arrived at floor ${p.to}.`) return [3,0];
    }
    if (/^Floor [0-9] requested from cabin panel\.$/.test(event.message) || ['Stop requested. Securing cabin at next landing.','Elevator resumed.','Door opening requested.','Door closing requested.'].includes(event.message)) return [1,3];
    return [3,4];
  }
  /**
   * Wrap event labels to a bounded character width
   * Prefer word boundaries and split long words so adjacent-lane labels fit
   */
  function wrap(message, limit) {
    const lines = []; let line = '';
    for (let word of message.split(/\s+/)) {
      if (line && line.length + word.length + 1 > limit) { lines.push(line); line = ''; }
      while (word.length > limit) { lines.push(word.slice(0,limit)); word = word.slice(limit); }
      if (word) line += (line ? ' ' : '') + word;
    }
    if (line) lines.push(line);
    return lines;
  }
  /**
   * Render the current run caption and latest 20 chronological runtime interactions
   * Rebuild the SVG only when the visible event or passenger identity changes
   * Follow the latest arrow in the scroll viewport, including on narrow screens
   */
  function render(state) {
    shown = state;
    const waiting = state.people.filter(p => p.status === 'waiting').length;
    const motion = state.stopped ? 'EMERGENCY STOP' : state.stopPending ? 'STOPPING AT LANDING' : state.state;
    el('sequence-progress').textContent = `Run #${state.runId} · Tick ${state.tick} (${stamp(state.tick)}) · Floor ${state.floor} → ${state.target} · ${motion} · ${waiting} waiting / ${state.occupancy} riding / ${state.completed} arrived`;
    // VISIBLE WINDOW: Keep time flowing downward and distinguish this recent view from persisted history
    const events = state.events.slice(-20).sort((a,b) => a.tick - b.tick || a.id - b.id);
    const key = JSON.stringify([state.runId,events,state.people.map(p => [p.id,p.name,p.from,p.to])]);
    if (key === signature) return;
    signature = key;
    const height = 160 + Math.max(1,events.length) * 108;
    const svg = node('svg',{xmlns:ns,viewBox:`0 0 1000 ${height}`,role:'img','aria-labelledby':'live-svg-title live-svg-description'});
    svg.append(node('title',{id:'live-svg-title'},`Current run #${state.runId}: recent interaction sequence`));
    svg.append(node('desc',{id:'live-svg-description'},'Time flows downward between passengers, operator, dispatcher, cabin and recorder. This is a recent window, not the complete history.'));
    svg.append(node('rect',{width:1000,height,fill:'#f8fbfc'}));
    // SVG GEOMETRY: Define reusable arrows and participant lifelines in a self-contained export
    const defs = node('defs'); const marker = node('marker',{id:'live-arrow',viewBox:'0 0 10 10',refX:9,refY:5,markerWidth:7,markerHeight:7,orient:'auto-start-reverse'});
    marker.append(node('path',{d:'M 0 0 L 10 5 L 0 10 z',fill:'#196575'})); defs.append(marker); svg.append(defs);
    actors.forEach((actor,index) => {
      const x = positions[index];
      svg.append(node('line',{x1:x,x2:x,y1:59,y2:height-48,stroke:'#adbfc3','stroke-dasharray':'5 5'}));
      for (const y of [18,height-47]) {
        svg.append(node('rect',{x:x-72,y,width:144,height:34,rx:5,fill:index === 3 ? '#c1e2e8' : '#e7f0f2',stroke:'#adbfc3'}));
        svg.append(node('text',{x,y:y+22,'text-anchor':'middle',fill:'#003e4d','font-family':'Segoe UI, sans-serif','font-size':13},actor));
      }
    });
    svg.append(node('text',{x:500,y:height-2,'text-anchor':'middle',fill:'#5b7d84','font-family':'Consolas, monospace','font-size':9},`Run #${state.runId} · Latest ${events.length} runtime interactions${events.length ? ` · Ticks ${events[0].tick}–${events.at(-1).tick}` : ''}`));
    if (!events.length) svg.append(node('text',{x:500,y:120,'text-anchor':'middle',fill:'#5b7a80','font-family':'Segoe UI, sans-serif','font-size':14},'Waiting for runtime interactions.'));
    const list = document.createDocumentFragment();
    // INTERACTIONS: Draw one timestamped arrow and one accessible text entry for each observed event
    events.forEach((event,index) => {
      const [from,to] = interaction(event,state), y = 144 + index * 108;
      const start = positions[from], end = positions[to], center = (start+end)/2;
      svg.append(node('text',{x:10,y:y+4,fill:'#5b7d84','font-family':'Consolas, monospace','font-size':10},stamp(event.tick)));
      const lines = wrap(event.message, Math.max(24,Math.floor(Math.abs(end-start)/7)));
      const text = node('text',{x:center,y:y-15-(lines.length-1)*15,'text-anchor':'middle',fill:'#153e48','font-family':'Segoe UI, sans-serif','font-size':12,'paint-order':'stroke',stroke:'#f8fbfc','stroke-width':5,'stroke-linejoin':'round'});
      lines.forEach((line,i) => text.append(node('tspan',{x:center,dy:i ? 15 : 0},line)));
      svg.append(text,node('line',{x1:start,x2:end,y1:y,y2:y,stroke:'#196575','stroke-width':1.5,'marker-end':'url(#live-arrow)'}));
      const li = document.createElement('li'), time = document.createElement('time'); time.textContent = stamp(event.tick);
      li.append(time,document.createTextNode(`${actors[from]} → ${actors[to]}: ${event.message}`)); list.append(li);
    });
    el('sequence-canvas').replaceChildren(svg); el('sequence-events').replaceChildren(list);
    el('sequence-window').textContent = `Run #${state.runId} · Latest ${events.length} interactions${events.length ? ` · Ticks ${events[0].tick}–${events.at(-1).tick}` : ''} · Complete history is in Telemetry & analysis`;
    el('sequence-download').dataset.unavailable = 'false'; window.ElevatorLab.updateControls();
    const viewport = el('sequence-scroll');
    viewport.scrollTop = viewport.scrollHeight;
    if (events.length) {
      const [from,to] = interaction(events.at(-1),state);
      viewport.scrollLeft = (positions[from]+positions[to])/2 * svg.clientWidth/1000 - viewport.clientWidth/2;
    }
  }
  /**
   * Describe whether the view is live, paused or waiting on delayed runtime updates
   * Pausing changes the browser view only and never pauses the C simulation
   */
  function status() {
    const delayed = !!lastSeen && Date.now() - lastSeen > 3000;
    el('sequence-status').textContent = paused ? 'View paused · simulator continues' : delayed ? 'Updates delayed · last received state' : latest ? '● Live · current run' : 'Waiting for runtime';
    el('sequence-status').classList.toggle('sequence-delayed',delayed && !paused);
  }
  // STATE INPUT: Retain the newest snapshot while a paused view continues to show its previous state
  window.addEventListener('elevator-state', event => { latest = event.detail; lastSeen = Date.now(); if (!paused) render(latest); status(); });
  // PAUSE CONTROL: Freeze or resume rendering without sending a simulator command
  el('sequence-pause').addEventListener('click',() => {
    paused = !paused; el('sequence-pause').textContent = paused ? 'Resume diagram' : 'Pause diagram';
    el('sequence-pause').setAttribute('aria-pressed',String(paused)); if (!paused && latest) render(latest); status();
  });
  // SVG EXPORT: Download exactly the visible run window, including a paused historical view
  el('sequence-download').addEventListener('click',() => {
    const svg = el('sequence-canvas').querySelector('svg'); if (!svg) return;
    const url = URL.createObjectURL(new Blob([new XMLSerializer().serializeToString(svg)],{type:'image/svg+xml;charset=utf-8'}));
    const link = document.createElement('a'); link.href = url; link.download = `elevator-run-${shown.runId}-sequence.svg`; link.click(); setTimeout(() => URL.revokeObjectURL(url),30000);
  });
  // FRESHNESS: Mark stale snapshots even when no new state event arrives
  setInterval(status,1000);
  const initial = window.ElevatorLab.getState(); if (initial) { latest = initial; lastSeen = Date.now(); render(initial); status(); }
})();
