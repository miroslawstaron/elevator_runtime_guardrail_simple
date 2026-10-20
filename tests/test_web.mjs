/****************************************
*
* Elevator Lab -- C HTTP Integration Verification
*
* SPDX-License-Identifier: MIT
*
******************************************/

// Node is used only to test HTTP; the running backend is the compiled C executable.
import assert from 'node:assert/strict';
import {spawn} from 'node:child_process';
import net from 'node:net';
import {fileURLToPath} from 'node:url';
import path from 'node:path';

const root = fileURLToPath(new URL('../', import.meta.url));
const port = 18089;
const binary = process.argv[2] || path.join(root, 'build', process.platform === 'win32' ? 'elevator_web.exe' : 'elevator_web');
const publicOrigin = process.argv[3];
const server = spawn(binary, ['--port', String(port), '--database', ':memory:', '--web-root', path.join(root, 'web'), ...(publicOrigin ? ['--public-origin', publicOrigin] : [])], {cwd:root, windowsHide:true, stdio:['ignore','pipe','pipe']});
let log = '';
server.stdout.on('data', data => { log += data; });
server.stderr.on('data', data => { log += data; });
let spawnError;
server.on('error', error => { spawnError = error; });
const base = `http://127.0.0.1:${port}`;
/**
 * Wait a bounded interval while the independent C clock or socket deadlines advance
 */
const pause = ms => new Promise(resolve => setTimeout(resolve, ms));
/**
 * Send a form command with an explicit Origin and assert its HTTP result
 * Return the parsed response for subsequent simulation assertions
 */
async function post(endpoint, data, expected = 200, origin = base) {
  const response = await fetch(base + endpoint, {method:'POST', headers:{'Content-Type':'application/x-www-form-urlencoded', Origin:origin}, body:new URLSearchParams(data)});
  assert.equal(response.status, expected);
  return response.json();
}
/**
 * Send exact HTTP bytes through a TCP socket to test framing and header rejection
 * Bound socket waits so malformed-request scenarios cannot hang the suite
 */
async function raw(request) {
  return new Promise((resolve, reject) => {
    const socket = net.connect(port, '127.0.0.1');
    let result = '';
    socket.setTimeout(5000, () => { socket.destroy(); reject(new Error('socket timeout')); });
    socket.on('connect', () => socket.write(request));
    socket.on('data', data => { result += data; });
    socket.on('end', () => resolve(result));
    socket.on('error', reject);
  });
}
// SCENARIO 1: Wait for the real C service and verify only intended static assets are served
try {
  for (let attempt = 0; attempt < 60 && !log.includes('Open http://'); attempt++) {
    if (spawnError) throw spawnError;
    if (server.exitCode !== null) throw new Error(log);
    await pause(50);
  }
  assert.match(log, /Open http:\/\//, 'server must start');
  for (const [route, type] of [['/','text/html'], ['/app.js','text/javascript'], ['/app.css','text/css'], ['/metriczone.css','text/css'], ['/assets/metriczone-logo.png','image/png'], ['/assets/agenticai-logo.png','image/png']]) {
    const response = await fetch(base + route);
    assert.equal(response.status, 200);
    assert.ok(response.headers.get('content-type').startsWith(type));
    assert.ok((await response.arrayBuffer()).byteLength > 100);
    assert.ok(response.headers.get('content-security-policy').includes("frame-ancestors 'none'"));
  }
  assert.equal((await fetch(base + '/src/main.c')).status, 404);
  assert.equal((await fetch(base + '/assets/%2e%2e/src/main.c')).status, 404);
  // SCENARIO 2: Verify authoritative state and atomic passenger validation
  const initial = await (await fetch(base + '/api/state')).json();
  assert.equal(initial.people.length, 0);
  let state = await post('/api/people', {name:'Alex "<&',from:'1',to:'7',count:'2'});
  assert.equal(state.people.length, 2);
  assert.equal(state.people[0].name, 'Alex "<& 1');
  for (const data of [
    {name:'Wrong',from:'1',to:'1',count:'1'},
    {name:'Wrong',from:'-1',to:'2',count:'1'},
    {name:'Wrong',from:'2',to:'9',count:'9'},
    {name:'Wrong',from:'2',to:'9x',count:'1'},
    {name:'\u0000Bad',from:'2',to:'9',count:'1'},
    {name:'Bad\nname',from:'2',to:'9',count:'1'},
    {name:'x'.repeat(41),from:'2',to:'9',count:'1'}]) await post('/api/people', data, 400);
  assert.equal((await (await fetch(base + '/api/state')).json()).people.length, 2);
  const invalidUtf8 = 'name=%FF&from=1&to=2&count=1';
  assert.match(await raw(`POST /api/people HTTP/1.1\r\nHost: 127.0.0.1:${port}\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: ${invalidUtf8.length}\r\n\r\n${invalidUtf8}`), /400 Bad Request/);
  state = await post('/api/people', {name:'M\u00e5rten',from:'0',to:'5',count:'1'});
  assert.equal(state.people[2].name, 'M\u00e5rten');
  // SCENARIO 3: Reject unauthorized origins, malformed framing and invalid commands
  await post('/api/control', {action:'reset'}, 403, 'https://outside.example');
  await post('/api/control', {action:'request',floor:'10'}, 400);
  await post('/api/control', {action:'missing'}, 409);
  assert.match(await raw(`GET /api/state HTTP/1.1\r\nHost: outside.example:${port}\r\n\r\n`), /403 Forbidden/);
  if (publicOrigin) {
    const publicHost = new URL(publicOrigin).host;
    assert.match(await raw(`GET /api/state HTTP/1.1\r\nHost: ${publicHost}\r\n\r\n`), /200 OK/);
    const body = 'action=request&floor=5';
    const publicPost = origin => raw(`POST /api/control HTTP/1.1\r\nHost: ${publicHost}\r\nOrigin: ${origin}\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: ${body.length}\r\n\r\n${body}`);
    assert.match(await publicPost(publicOrigin), /200 OK/);
    assert.match(await publicPost('https://outside.example'), /403 Forbidden/);
    assert.match(await publicPost(publicOrigin.replace('https:', 'http:')), /403 Forbidden/);
    await post('/api/control', {action:'request',floor:'5'}, 403, publicOrigin);
    console.log('PASS: configured public HTTPS hostname accepted; mismatched origins rejected');
  }
  assert.match(await raw(`POST /api/control HTTP/1.1\r\nHost: 127.0.0.1:${port}\r\nContent-Length: 0\r\nContent-Length: 0\r\n\r\n`), /400 Bad Request/);
  assert.match(await raw(`POST /api/control HTTP/1.1\r\nHost: 127.0.0.1:${port}\r\nTransfer-Encoding: chunked\r\n\r\n`), /400 Bad Request/);
  assert.match(await raw(`POST /api/control HTTP/1.1\r\nHost: 127.0.0.1:${port}\r\nContent-Length: 999999\r\n\r\n`), /400 Bad Request/);
  // SCENARIO 4: Keep one HTTP request incomplete and verify the simulation clock still advances
  const slow = net.connect(port, '127.0.0.1');
  await new Promise(resolve => slow.once('connect', resolve));
  slow.write('GET /api/state HTTP/1.1\r\n');
  const before = await (await fetch(base + '/api/state')).json();
  await pause(650);
  const after = await (await fetch(base + '/api/state')).json();
  assert.ok(after.tick >= before.tick + 5, 'partial HTTP clients must not block the C clock');
  slow.destroy();
  // SCENARIO 5: Verify stop/resume, demo and reset against the shared C simulation
  await post('/api/control', {action:'stop'});
  await pause(650);
  state = await (await fetch(base + '/api/state')).json();
  assert.equal(state.stopped, true);
  const stoppedAt = state.position;
  await pause(350);
  state = await (await fetch(base + '/api/state')).json();
  assert.equal(state.position, stoppedAt);
  assert.equal(state.position, state.floor);
  await post('/api/control', {action:'resume'});
  await post('/api/control', {action:'reset'});
  state = await post('/api/control', {action:'demo'});
  assert.equal(state.people.length, 6);
  state = await post('/api/control', {action:'reset'});
  assert.equal(state.people.length, 0);
  // SCENARIOS: Validate large scheduled runs and reject bad streams before archiving any history.
  const morning = await post('/api/scenarios',{kind:'morning',count:300,duration:600,seed:42});
  assert.equal(morning.people.length,300); assert.equal(morning.maxPeople,1000);
  assert.ok(morning.people.every(p => p.from === 0 && p.to > 0));
  assert.equal(morning.people.filter(p => p.status === 'scheduled').length,299);
  const repeated = await post('/api/scenarios',{kind:'morning',count:300,duration:600,seed:42});
  assert.deepEqual(repeated.people.map(p => [p.from,p.to,p.addedTick]),morning.people.map(p => [p.from,p.to,p.addedTick]));
  for (const flows of ['0,0,1,0,0,0','0,9,1001,0,0,0','0,9,3,86400,1,0','0,9,1,0,0,2','0,9,1,0,0,0;','0,9,1,0,0,0,2','0,9,1,0,0','0,9,1,0,0,999999999999999999999']) {
    await post('/api/scenarios',{kind:'custom',count:1,seed:42,duration:0,flows},400);
    assert.equal((await (await fetch(base+'/api/state')).json()).runId,repeated.runId);
  }
  await post('/api/scenarios',{kind:'nope'},400);
  await post('/api/scenarios',{kind:'morning',seed:0},400);
  await post('/api/scenarios',{kind:'morning',count:300},403,'https://outside.example');
  const archived = (await (await fetch(base+`/api/telemetry/analysis?runId=${morning.runId}`)).json()).rows[0];
  assert.equal(archived.scheduledCount,0); assert.equal(archived.cancelledCount,300);
  for (const kind of ['afternoon','occasional','vip']) {
    const run = await post('/api/scenarios',{kind,count:300,duration:600,seed:123});
    if (kind === 'afternoon') assert.ok(run.people.every(p => p.from > 0 && p.to === 0));
    if (kind === 'vip') assert.equal(run.people.filter(p => p.vip).length,60);
  }
  const custom = await post('/api/scenarios',{kind:'custom',count:1000,duration:0,seed:42,flows:'0,9,600,0,0,0;9,0,400,100,0,1'});
  assert.equal(custom.people.length,1000); assert.equal(custom.people.filter(p => p.vip).length,400);
  await post('/api/control',{action:'speed',floor:2},409);
  const fast = await post('/api/control',{action:'speed',floor:20});
  await pause(650);
  const advanced = await (await fetch(base+'/api/state')).json();
  assert.ok(advanced.tick-fast.tick >= 80,'accelerated C clock advances without a browser');
  assert.equal(advanced.telemetry.lostEvents,0);
  const cancelled = await post('/api/control',{action:'cancel_scenario'});
  assert.equal(cancelled.people.filter(p => p.status === 'scheduled').length,0);
  assert.equal(cancelled.people.filter(p => p.status === 'cancelled').length,400);
  assert.ok(cancelled.people.some(p => ['waiting','riding','arrived'].includes(p.status)));
  const analysisResponse = await fetch(base+`/api/telemetry/analysis?runId=${custom.runId}`);
  assert.equal(analysisResponse.status,200);
  const analysis = (await analysisResponse.json()).rows[0];
  assert.equal(analysis.passengerCount,1000); assert.equal(analysis.releasedCount,600);
  assert.ok(analysis.sampleCount > 80 && analysis.boardedCount > 0);
  assert.equal((await fetch(base+'/api/telemetry/analysis?fromTick=0')).status,400);
  assert.equal((await fetch(base+'/api/telemetry/analysis?runId=2147483647')).status,404);
  console.log('PASS: existing HTTP controls/security; 300-person presets, deterministic schedules, invalid custom atomicity, archived cancellations, 1000-person custom streams, VIPs, accelerated independent clock, no event loss and full-run telemetry analysis');
// CLEANUP: Stop the isolated server even when an assertion fails
} finally { server.kill(); }
