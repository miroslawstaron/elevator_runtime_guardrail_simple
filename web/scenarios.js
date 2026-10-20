/****************************************
 * Elevator Lab -- Scenario Configuration and Progress
 * SPDX-License-Identifier: MIT
 ****************************************/
'use strict';
(() => {
  const el = id => document.getElementById(id), lab = window.ElevatorLab;
  const presets = {
    morning:{name:'busy morning',count:300,duration:600,description:'Passengers arrive evenly during the arrival window and travel from floor 0 to seeded destinations on floors 1–9.'},
    afternoon:{name:'busy afternoon',count:300,duration:600,description:'Passengers leave seeded office floors 1–9 evenly during the arrival window and travel to floor 0.'},
    occasional:{name:'occasional travellers',count:24,duration:1200,description:'Occasional travellers make seeded journeys between different floors, spread evenly over 20 minutes by default.'},
    vip:{name:'VIP traffic',count:40,duration:300,description:'Mixed traffic with every fifth traveller marked VIP. Compare VIP and regular waits in telemetry analysis.'},
    custom:{name:'custom scenario',description:'Build a traffic pattern from independently timed streams. Stream order resolves ties in arrival and priority.'}
  };
  let selected = 'morning', flowId = 0;
  /** Build floor options; numeric values are controlled by the application. */
  function floors(value) { return Array.from({length:10},(_,i) => `<option value="${i}"${i === value ? ' selected' : ''}>${i === 0 ? '0 · Lobby' : `Floor ${i}`}</option>`).join(''); }
  /** Add an accessible, independently configurable arrival stream with bounded inputs. */
  function addFlow() {
    if (el('scenario-flows').children.length >= 20) return;
    const row = document.createElement('div'), id = ++flowId;
    row.className = 'scenario-flow';
    row.innerHTML = `<div><label for="flow-${id}-from">From</label><select id="flow-${id}-from" data-field="from">${floors(0)}</select></div><div><label for="flow-${id}-to">To</label><select id="flow-${id}-to" data-field="to">${floors(5)}</select></div>${[['count','People',30,1,1000],['start','First arrival (s)',0,0,86400],['interval','Interval (s)',2,0,86400]].map(([key,label,value,min,max]) => `<div><label for="flow-${id}-${key}">${label}</label><input id="flow-${id}-${key}" data-field="${key}" type="number" min="${min}" max="${max}" step="1" value="${value}" required></div>`).join('')}<div><label for="flow-${id}-vip">Priority</label><select id="flow-${id}-vip" data-field="vip"><option value="0">Regular</option><option value="1">VIP</option></select></div><button class="button button-quiet" type="button" data-remove-flow aria-label="Remove arrival stream ${id}">Remove</button>`;
    el('scenario-flows').append(row); updateTotal(); lab.updateControls();
  }
  /** Return form streams in stable insertion order for validation and submission. */
  function flows() { return [...el('scenario-flows').children].map(row => Object.fromEntries([...row.querySelectorAll('[data-field]')].map(input => [input.dataset.field,Number(input.value)]))); }
  /** Update the custom total and enforce the maximum number of stream editors. */
  function updateTotal() {
    el('scenario-custom-total').textContent = ` ${flows().reduce((n,f) => n + f.count,0)} / 1,000 people`;
    el('add-scenario-flow').dataset.unavailable = String(el('scenario-flows').children.length >= 20);
    lab.updateControls();
  }
  /** Select preset defaults without touching the live simulation until Run is pressed. */
  function choose(kind) {
    selected = kind;
    document.querySelectorAll('[data-preset]').forEach(button => button.setAttribute('aria-pressed',String(button.dataset.preset === kind)));
    el('scenario-description').textContent = presets[kind].description;
    el('run-scenario').textContent = `Run ${presets[kind].name} ▷`;
    const custom = kind === 'custom';
    el('custom-scenario').hidden = !custom;
    el('scenario-count-field').hidden = custom; el('scenario-duration-field').hidden = custom;
    el('scenario-count').disabled = custom; el('scenario-duration').disabled = custom;
    el('scenario-flows').querySelectorAll('input,select').forEach(input => { input.disabled = !custom; });
    if (!custom) { el('scenario-count').value = presets[kind].count; el('scenario-duration').value = presets[kind].duration; }
  }
  document.querySelectorAll('[data-preset]').forEach(button => button.addEventListener('click',() => choose(button.dataset.preset)));
  el('add-scenario-flow').addEventListener('click',addFlow);
  el('scenario-flows').addEventListener('input',updateTotal);
  el('scenario-flows').addEventListener('click',event => { const button = event.target.closest('[data-remove-flow]'); if (button) { button.parentElement.remove(); updateTotal(); } });
  // RUN: Submit a whole schedule atomically; the C server archives only after full validation.
  el('scenario-form').addEventListener('submit',async event => {
    event.preventDefault(); el('scenario-error').hidden = true;
    const config = {kind:selected,count:el('scenario-count').value,duration:el('scenario-duration').value,seed:el('scenario-seed').value};
    if (selected === 'custom') {
      const streams = flows();
      config.count = streams.reduce((n,f) => n + f.count,0); config.duration = 0;
      if (!streams.length || config.count > 1000 || streams.some(f => f.from === f.to || f.start + (f.count - 1) * f.interval > 86400)) {
        el('scenario-error').textContent = 'Add at least one stream with different floors, at most 1,000 total people and arrivals within 86,400 seconds.';
        el('scenario-error').hidden = false; return;
      }
      config.flows = streams.map(f => [f.from,f.to,f.count,f.start,f.interval,f.vip].join(',')).join(';');
    }
    el('run-scenario').dataset.busy = 'true'; lab.updateControls();
    try { await lab.api('/api/scenarios',config); lab.notify('Scenario started. Previous experiment data is saved.'); }
    catch (error) { el('scenario-error').textContent = error.message; el('scenario-error').hidden = false; }
    finally { el('run-scenario').dataset.busy = 'false'; lab.updateControls(); }
  });
  /** Apply shared runtime controls and display any server error without losing the form. */
  async function control(action, floor) {
    try { await lab.api('/api/control',{action,...(floor ? {floor} : {})}); }
    catch (error) { lab.notify(error.message); }
  }
  el('simulation-speed').addEventListener('change',() => control('speed',el('simulation-speed').value));
  el('cancel-scenario').addEventListener('click',() => control('cancel_scenario'));
  /** Render schedule progress from C; polling never releases people or advances the clock. */
  function progress(s) {
    const scheduled = s.people.filter(p => p.status === 'scheduled').length;
    const waiting = s.people.filter(p => p.status === 'waiting').length;
    const cancelled = s.people.filter(p => p.status === 'cancelled').length;
    const finished = !scheduled && !waiting && !s.occupancy;
    el('scenario-status').textContent = s.scenario ? `${presets[s.scenario]?.name || s.scenario} · Experiment #${s.runId} · ${finished ? 'Complete' : 'Running'} · ${scheduled} scheduled · ${waiting} waiting · ${s.occupancy} riding · ${s.completed}/${s.people.length} arrived${cancelled ? ` · ${cancelled} cancelled` : ''} · ${s.speed}×` : 'Choose a scenario to begin.';
    el('simulation-speed').value = String(s.speed || 1);
    el('cancel-scenario').dataset.unavailable = String(!scheduled);
    lab.updateControls();
  }
  window.addEventListener('elevator-state',event => progress(event.detail));
  addFlow(); choose('morning');
  if (lab.getState()) progress(lab.getState());
})();
