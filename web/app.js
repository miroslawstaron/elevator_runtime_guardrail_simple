'use strict';

const $ = id => document.getElementById(id);
const escapeHtml = value => String(value).replace(/[&<>"']/g, c => ({'&':'&amp;', '<':'&lt;', '>':'&gt;', '"':'&quot;', "'":'&#39;'}[c]));
const floorName = floor => floor === 0 ? '0 · Lobby' : `Floor ${floor}`;
const clock = ticks => { const seconds = Math.floor(ticks / 10); return `${String(Math.floor(seconds / 60)).padStart(2, '0')}:${String(seconds % 60).padStart(2, '0')}`; };
const personSvg = '<svg viewBox="0 0 20 32" aria-hidden="true"><circle cx="10" cy="5" r="3" fill="currentColor"/><path d="M6 11 Q10 8 14 11 L16 20 M6 11 L4 20 M7 12 L7 21 L6 30 M13 12 L13 21 L14 30" fill="none" stroke="currentColor" stroke-width="2.8" stroke-linecap="round"/></svg>';
let state = null;
let connected = false;
let requestId = 0;
let appliedRequest = 0;
let eventKey = '';
let tableKey = '';
let commandChain = Promise.resolve();
let toastTimer;
let submitting = false;
let positioned = false;
let nameIndex = 0;
const names = ['Alex', 'Sam', 'Jamie', 'Robin', 'Morgan', 'Casey', 'Taylor', 'Jordan'];

for (let floor = 9; floor >= 0; floor--) {
  const row = document.createElement('div');
  row.className = 'floor-row';
  row.id = `floor-${floor}`;
  row.innerHTML = `<div class="floor-number">${String(floor).padStart(2, '0')}<small>${floor === 0 ? 'LOBBY' : 'FLOOR'}</small></div><div class="floor-waiting" id="waiting-${floor}"><span class="floor-empty">No one waiting</span></div><button class="floor-call" data-floor="${floor}" aria-label="Call elevator to ${floorName(floor)}" title="Call elevator to ${floorName(floor)}">+</button>`;
  $('floors').append(row);
}
for (let floor = 0; floor < 10; floor++) {
  $('from-floor').add(new Option(floorName(floor), String(floor)));
  $('to-floor').add(new Option(floorName(floor), String(floor)));
  const button = document.createElement('button');
  button.className = 'floor-button';
  button.dataset.floor = String(floor);
  button.textContent = String(floor);
  button.setAttribute('aria-label', `Request ${floorName(floor)}`);
  button.setAttribute('aria-pressed', 'false');
  $('floor-buttons').append(button);
}
$('from-floor').value = '1';
$('to-floor').value = '7';

function notify(message) {
  $('toast').textContent = message;
  $('toast').hidden = false;
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => { $('toast').hidden = true; }, 4000);
}

function setConnection(value) {
  connected = value;
  $('connection').classList.toggle('offline', !value);
  $('connection').lastElementChild.textContent = value ? 'C runtime connected' : 'Runtime disconnected · reconnecting';
  updateControls();
  if (!value) {
    $('motion-status').textContent = 'Disconnected';
    $('guard-status').textContent = 'Runtime connection unavailable';
    $('guard-dot').style.backgroundColor = '#bf6556';
  }
}

function updateControls() {
  document.querySelectorAll('button').forEach(button => { button.disabled = !connected; });
  if (!state || !connected) return;
  $('add-button').disabled = submitting || state.people.length >= 160;
  $('demo-button').disabled = state.people.length + 6 > 160;
  $('open-button').disabled = state.state !== 'IDLE' || state.stopped || state.stopPending;
  $('close-button').disabled = !['DOORS_OPEN', 'DOORS_OPENING'].includes(state.state) || state.stopped || state.stopPending;
}

async function api(path, values) {
  const id = ++requestId;
  const controller = new AbortController();
  const timer = setTimeout(() => controller.abort(), 4000);
  let response;
  try {
    response = await fetch(path, values ? { method: 'POST', body: new URLSearchParams(values), headers: {'Content-Type': 'application/x-www-form-urlencoded'}, signal: controller.signal } : { signal: controller.signal });
    const result = await response.json();
    if (!response.ok) throw new Error(result.error || 'The request could not be completed.');
    if (id >= appliedRequest) {
      appliedRequest = id;
      state = result;
      setConnection(true);
      render();
    }
    return result;
  } catch (error) {
    if (!response) setConnection(false);
    throw error;
  } finally { clearTimeout(timer); }
}

function control(action, extra = {}) {
  commandChain = commandChain.catch(() => {}).then(async () => {
    try {
      await api('/api/control', {action, ...extra});
      if (action === 'demo') notify('Six people, six journeys. Watch the building come to life.');
      if (action === 'reset') notify('A fresh start. All journeys have been cleared.');
    } catch (error) { notify(error.message); }
  });
  return commandChain;
}

document.addEventListener('click', event => {
  const button = event.target.closest('button');
  if (!button || button.disabled) return;
  if (button.dataset.floor !== undefined) control('request', {floor: button.dataset.floor});
  else if (button.dataset.action) control(button.dataset.action);
});
$('demo-button').addEventListener('click', () => control('demo'));
$('reset-button').addEventListener('click', () => control('reset'));
$('journey-filter').addEventListener('change', () => { tableKey = ''; renderJourneys(); });
$('passenger-form').addEventListener('submit', event => {
  event.preventDefault();
  if (!connected || submitting) return;
  $('form-error').hidden = true;
  const data = Object.fromEntries(new FormData(event.currentTarget));
  data.name = data.name.trim();
  if (!data.name || new TextEncoder().encode(data.name).length > 40) {
    $('form-error').textContent = 'Enter a name up to 40 UTF-8 bytes long.';
    $('form-error').hidden = false; return;
  }
  if (data.from === data.to) {
    $('form-error').textContent = 'Choose a destination different from the starting floor.';
    $('form-error').hidden = false; return;
  }
  submitting = true; updateControls();
  commandChain = commandChain.catch(() => {}).then(async () => {
    try {
      await api('/api/people', data);
      notify(Number(data.count) === 1 ? `${data.name} joined the queue on ${floorName(Number(data.from))}.` : `${data.count} passengers joined the queue.`);
      $('passenger-name').value = names[++nameIndex % names.length];
    } catch (error) {
      $('form-error').textContent = error.message;
      $('form-error').hidden = false;
    } finally { submitting = false; updateControls(); }
  });
});

function render() {
  const waiting = state.people.filter(p => p.status === 'waiting');
  $('waiting-count').textContent = String(waiting.length);
  $('riding-count').textContent = String(state.occupancy);
  $('arrived-count').textContent = String(state.completed);
  $('stops-count').textContent = `${state.stops} ${state.stops === 1 ? 'stop' : 'stops'} made`;
  $('people-total').textContent = String(state.people.length);
  $('runtime-clock').textContent = clock(state.tick);
  const moving = /MOVING|SMOOTH/.test(state.state);
  const status = state.stopped ? 'Emergency stop' : state.stopPending ? 'Stopping at landing' : state.doorsOpen ? 'Doors open' : moving ? (state.direction === 1 ? '↑ Moving up' : '↓ Moving down') : state.state === 'DOORS_CLOSING' ? 'Doors closing' : 'Standing by';
  $('motion-status').textContent = status;
  $('current-floor').textContent = String(state.floor).padStart(2, '0');
  $('direction-label').textContent = status;
  $('target-label').textContent = moving ? `Next stop · Floor ${state.target}` : `${state.occupancy} of ${state.capacity} spaces occupied`;
  $('car-floor').textContent = String(state.floor).padStart(2, '0');
  $('car-people').textContent = String(state.occupancy);
  const car = $('elevator-car');
  if (!positioned) car.style.transition = 'none';
  car.style.transform = `translateY(${(9 - state.position) * 48}px)`;
  if (!positioned) {
    car.getBoundingClientRect();
    car.style.removeProperty('transition');
    positioned = true;
  }
  $('elevator-car').classList.toggle('open', state.doorsOpen);
  $('elevator-car').setAttribute('aria-label', `Elevator at position ${state.position}, ${state.occupancy} passengers, doors ${state.doorsOpen ? 'open' : 'closed'}`);
  for (let floor = 0; floor < 10; floor++) {
    $(`floor-${floor}`).classList.toggle('current', state.floor === floor);
    const people = waiting.filter(p => p.from === floor);
    const content = people.slice(0, 6).map(p => `<span class="waiting-person person-color-${p.id % 5}" role="img" aria-label="${escapeHtml(p.name)}, going to floor ${p.to}" title="${escapeHtml(p.name)} → Floor ${p.to}">${personSvg}</span>`).join('') + (people.length > 6 ? `<span class="more">+${people.length - 6}</span>` : '');
    const element = $(`waiting-${floor}`);
    const html = content || '<span class="floor-empty">No one waiting</span>';
    if (element.dataset.content !== html) { element.innerHTML = html; element.dataset.content = html; }
  }
  document.querySelectorAll('[data-floor]').forEach(button => {
    const floor = Number(button.dataset.floor);
    button.classList.toggle('requested', state.requests[floor]);
    button.classList.toggle('current', state.floor === floor);
    button.setAttribute('aria-pressed', String(state.requests[floor]));
  });
  const violation = state.guardrails.doors || state.guardrails.betweenFloors || state.guardrails.invalidFloor;
  $('guard-status').textContent = violation ? 'Guardrail correction active' : 'All runtime guardrails active';
  $('guard-dot').style.backgroundColor = violation ? '#bf6556' : '#65875d';
  $('door-status').textContent = state.doorsOpen ? 'Doors open' : 'Doors closed';
  $('stop-notice').hidden = !state.stopped && !state.stopPending;
  $('stop-title').textContent = state.stopPending ? 'Securing the cabin' : 'Emergency stop active';
  $('stop-description').textContent = state.stopPending ? 'Finishing the current floor interval, then holding at the landing.' : `Holding safely at floor ${state.floor} for ${state.guardrails.secondsStopped}s.${state.guardrails.needsPrompt ? ' Please resume or continue holding.' : ' Resume when ready.'}`;
  $('stop-button').textContent = state.stopped || state.stopPending ? '↻ Resume elevator' : '▣ Emergency stop';
  $('stop-button').dataset.action = state.stopped || state.stopPending ? 'resume' : 'stop';
  renderJourneys();
  const newEventKey = JSON.stringify(state.events.map(e => [e.id, e.message]));
  if (newEventKey !== eventKey) {
    eventKey = newEventKey;
    $('event-list').innerHTML = [...state.events].reverse().slice(0, 20).map(e => `<li>${escapeHtml(e.message)}<time class="mono">${clock(e.tick)}</time></li>`).join('');
  }
  updateControls();
}

function renderJourneys() {
  if (!state) return;
  const filter = $('journey-filter').value;
  const people = state.people.filter(p => filter === 'all' || p.status === filter).slice().reverse();
  const newTableKey = JSON.stringify([filter, people.map(p => [p.id, p.name, p.from, p.to, p.status, Math.floor(((p.status === 'waiting' ? state.tick : p.boardedTick) - p.addedTick) / 10)])]);
  if (newTableKey === tableKey) return;
  tableKey = newTableKey;
  $('passenger-rows').innerHTML = people.map(p => {
    const wait = Math.max(0, ((p.status === 'waiting' ? state.tick : p.boardedTick) - p.addedTick) / 10);
    return `<tr><td><div class="passenger-cell"><span class="avatar person-color-${p.id % 5}">${escapeHtml(p.name[0].toUpperCase())}</span>${escapeHtml(p.name)}</div></td><td>${p.from === 0 ? 'Lobby' : `F${p.from}`}<span class="journey-arrow">→</span>${p.to === 0 ? 'Lobby' : `F${p.to}`}</td><td><span class="journey-status ${p.status}">${{waiting:'Waiting', riding:'On board', arrived:'Arrived'}[p.status]}</span></td><td>${Math.floor(wait)}s</td></tr>`;
  }).join('');
  $('empty-state').hidden = people.length > 0;
  $('empty-state').querySelector('h3').textContent = state.people.length ? 'No journeys in this view.' : 'A building full of possibilities.';
  $('empty-state').querySelector('p').textContent = state.people.length ? 'Choose another status to see more passengers.' : 'Add your first passenger or try a busy morning.';
  $('table-footer').textContent = `${people.length} ${people.length === 1 ? 'journey' : 'journeys'} shown · ${state.people.length}/160 session places used · Updates live`;
}

updateControls();
(async function poll() {
  try { await api('/api/state'); } catch (_) { /* Connection status explains failures; retry automatically. */ }
  setTimeout(poll, connected ? 120 : 1000);
})();
