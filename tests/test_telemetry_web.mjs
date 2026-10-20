/****************************************
*
* Elevator Lab -- Persistent Telemetry HTTP Verification
*
* SPDX-License-Identifier: MIT
*
******************************************/

import assert from 'node:assert/strict';
import {spawn} from 'node:child_process';
import fs from 'node:fs/promises';
import net from 'node:net';
import os from 'node:os';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
const root = fileURLToPath(new URL('../',import.meta.url));
const directory = await fs.mkdtemp(path.join(os.tmpdir(),'elevator-telemetry-'));
const database = path.join(directory,'experiment.sqlite3');
const binary = path.resolve(process.argv[2] || path.join(root,'build',process.platform === 'win32' ? 'elevator_web.exe' : 'elevator_web'));
let port;
let base;
/**
 * Wait briefly while the independent C service advances the real simulation
 */
const pause = ms => new Promise(resolve => setTimeout(resolve,ms));
let server;
/**
 * Ask the operating system for an unused loopback port for one server session
 * Release the reservation before starting the separately spawned C server
 */
async function availablePort() {
  const reservation = net.createServer();
  await new Promise((resolve,reject) => {
    reservation.once('error',reject);
    reservation.listen(0,'127.0.0.1',resolve);
  });
  const {port} = reservation.address();
  await new Promise((resolve,reject) => reservation.close(error => error ? reject(error) : resolve()));
  return port;
}
/**
 * Start an isolated C service using the same on-disk test database
 * Wait for startup output and fail when initialization does not complete
 */
async function start() {
  let log = '';
  port = await availablePort();
  base = `http://127.0.0.1:${port}`;
  server = spawn(binary,['--port',String(port),'--database',database,'--web-root',path.join(root,'web')],{cwd:root,windowsHide:true});
  server.stdout.on('data',data => { log += data; });
  server.stderr.on('data',data => { log += data; });
  server.on('error',error => { log += error.message; });
  for (let i = 0; i < 100 && !log.includes('Open http://'); i++) {
    if (server.exitCode !== null) throw new Error(log);
    await pause(30);
  }
  assert.match(log,/Open http:\/\//);
}
/**
 * Terminate the test server and wait for process exit before reopening its database
 */
async function stop() {
  if (!server || server.exitCode !== null) return;
  const closed = new Promise(resolve => server.once('exit',resolve)); server.kill(); await closed;
}
/**
 * Read one JSON endpoint and assert its expected HTTP status
 */
async function get(endpoint,expected = 200) {
  const response = await fetch(base + endpoint);
  assert.equal(response.status,expected,endpoint);
  return response.json();
}
/**
 * Send a form request with a configurable Origin and assert the response status
 */
async function post(endpoint,data,expected = 200,origin = base) {
  const response = await fetch(base + endpoint,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded',Origin:origin},body:new URLSearchParams(data)});
  assert.equal(response.status,expected,endpoint);
  return response.json();
}
// SCENARIO 1: Record an actual trip and feedback in an isolated persistent experiment
try {
  await start();
  let state = await get('/api/state');
  assert.ok(state.runId > 0); assert.equal(state.telemetry.healthy,true);
  const first = state.runId;
  const initial = await get(`/api/telemetry/summary?runId=${first}`);
  assert.equal(initial.rows[0].averageWaitSeconds,null);
  state = await post('/api/experiments',{name:'Morning baseline <& "A"'});
  const run = state.runId;
  assert.ok(run > first);
  state = await post('/api/people',{name:'=Alex, "A"',from:1,to:2,count:1});
  assert.equal(state.runId,run);
  const text = 'Long wait, smooth ride. "Thanks"\nCaf\u00e9 <img src=x onerror=alert(1)>\n\'); DROP TABLE samples; --';
  let result = await post('/api/feedback',{runId:run,passengerId:1,text,rating:4});
  assert.ok(result.feedbackId > 0);
  // SCENARIO 2: Reject invalid feedback text, ratings, identities and request origins
  for (const values of [
    {runId:run,passengerId:1,text:''},
    {runId:run,passengerId:1,text:' \n\t'},
    {runId:run,passengerId:1,text:'x'.repeat(2001)},
    {runId:run,passengerId:1,text:'Bad\u0000text'},
    {runId:run,passengerId:1,text:'Rating',rating:6},
    {runId:run,passengerId:1,text:'Rating',rating:0},
    {passengerId:1,text:'Missing run'},
  ]) await post('/api/feedback',values,400);
  await post('/api/feedback',{runId:run,passengerId:2,text:'Not present'},404);
  await post('/api/feedback',{runId:2147483647,passengerId:1,text:'Wrong run'},404);
  await post('/api/feedback',{runId:run,passengerId:1,text:'Wrong origin'},403,'https://outside.example');
  for (let i = 0; i < 100; i++) { state = await get('/api/state'); if (state.completed === 1) break; await pause(80); }
  assert.equal(state.completed,1);
  const summary = (await get(`/api/telemetry/summary?runId=${run}`)).rows[0];
  assert.equal(summary.completedCount,1); assert.equal(summary.passengerCount,1); assert.equal(summary.feedbackCount,1); assert.equal(summary.averageRating,4);
  assert.ok(summary.averageWaitSeconds >= 0); assert.ok(summary.averageRideSeconds > 0);
  // SCENARIO 3: Verify bounded pagination, tick ranges, journey timing and export escaping
  const firstPage = await get(`/api/telemetry/samples?runId=${run}&limit=3`);
  assert.equal(firstPage.rows.length,3); assert.equal(firstPage.hasMore,true);
  const secondPage = await get(`/api/telemetry/samples?runId=${run}&limit=3&offset=${firstPage.nextOffset}&toTick=${firstPage.toTick}`);
  assert.ok(secondPage.rows[0].tick > firstPage.rows.at(-1).tick);
  const range = await get(`/api/telemetry/samples?runId=${run}&fromTick=1&toTick=2`);
  assert.deepEqual(range.rows.map(row => row.tick),[1,2]);
  const people = await get(`/api/telemetry/passengers?runId=${run}&passengerId=1`);
  assert.equal(people.rows[0].status,'arrived'); assert.ok(people.rows[0].rideSeconds > 0);
  const feedback = await get(`/api/telemetry/feedback?runId=${run}`);
  assert.equal(feedback.rows[0].text,text);
  const csv = await fetch(base + `/api/telemetry/passengers?runId=${run}&format=csv`);
  assert.equal(csv.status,200); assert.ok(csv.headers.get('content-type').startsWith('text/csv'));
  assert.match(await csv.text(),/"'=Alex, ""A"""/);
  assert.ok((await get(`/api/telemetry/events?runId=${run}`)).rows.some(event => event.message.includes('arrived')));
  // SCENARIO 4: Reject unsupported query options instead of silently changing query meaning
  for (const query of ['limit=201','limit=0','fromTick=9&toTick=1','runId=-1','runId=1%20OR%201=1','limit=1&limit=2','format=xml','formTick=1']) await get(`/api/telemetry/samples?${query}`,400);
  await get('/api/telemetry/summary?fromTick=1',400);
  await get('/api/telemetry/no-such-dataset',404);
  await get('/api/telemetry/samples?runId=2147483647',404);
  assert.equal((await fetch(base + '/data/elevator.sqlite3')).status,404);
  await post('/api/experiments',{name:'Bad\nlabel'},400);
  state = await post('/api/control',{action:'reset'});
  // SCENARIO 5: Retain history across reset and isolate reused passenger IDs by run
  const nextRun = state.runId;
  assert.notEqual(nextRun,run);
  assert.equal((await get(`/api/telemetry/feedback?runId=${run}`)).rows.length,1);
  await post('/api/people',{name:'New passenger with same ID',from:0,to:9,count:1});
  await post('/api/feedback',{runId:nextRun,passengerId:1,text:'New experiment'},200);
  await post('/api/feedback',{runId:run,passengerId:1,text:'Late feedback on old journey'},200);
  assert.equal((await get(`/api/telemetry/feedback?runId=${nextRun}`)).rows.length,1);
  const bytes = await fs.readFile(database); assert.equal(bytes.subarray(0,15).toString(),'SQLite format 3');
  // SCENARIO 6: Reopen the same file and verify feedback and archived experiments persist
  await stop(); await start();
  state = await get('/api/state'); assert.ok(state.runId > nextRun);
  assert.equal((await get(`/api/telemetry/feedback?runId=${run}`)).rows.length,2);
  assert.equal((await get(`/api/telemetry/samples?runId=${run}&limit=200`)).rows.length,summary.sampleCount);
  const runs = await get('/api/telemetry/runs?limit=200');
  assert.ok(runs.rows.some(r => r.name === 'Morning baseline <& "A"'));
  console.log('PASS: persistent SQLite API; 10 Hz samples/events/journeys; named experiments; feedback/ratings/UTF-8; input validation and origin checks; tick filtering/pagination; CSV escaping; reset/restart history and passenger identity isolation.');
} finally {
  await stop();
  // CLEANUP: Verify the generated temporary directory boundary before recursive test removal
  const resolved = path.resolve(directory);
  assert.ok(resolved.startsWith(path.resolve(os.tmpdir()) + path.sep) && path.basename(resolved).startsWith('elevator-telemetry-'));
  await fs.rm(resolved,{recursive:true,force:true});
}
