/****************************************
*
* Elevator Lab -- Live Sequence and Complete Export Verification
*
* SPDX-License-Identifier: MIT
*
******************************************/

// Real C backend and browser verification for the live sequence and combined exports.
import assert from 'node:assert/strict';
import {spawn} from 'node:child_process';
import {createRequire} from 'node:module';
import fs from 'node:fs/promises';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
const require = createRequire(import.meta.url);
const {chromium} = require('../build/browser-tools/node_modules/playwright');
const root = fileURLToPath(new URL('../',import.meta.url));
const base = 'http://localhost:18095';
const binary = path.resolve(process.argv[2] || path.join(root,'build/elevator_web.exe'));
const server = spawn(binary,['--port','18095','--database',':memory:','--web-root',path.join(root,'web')],{windowsHide:true});
let log = ''; server.stdout.on('data',data => { log += data; }); server.stderr.on('data',data => { log += data; });
let browser;
// Parse quoted, multiline CSV rather than splitting feedback on commas or newlines.
/**
 * Parse quoted CSV fields including embedded commas, doubled quotes and line breaks
 * Return rows suitable for checking complete combined-export records
 */
function csv(text) {
  const rows = []; let row = [], field = '', quoted = false;
  for (let i = 0; i < text.length; i++) {
    const ch = text[i];
    if (ch === '"') { if (quoted && text[i+1] === '"') { field += '"'; i++; } else quoted = !quoted; }
    else if (ch === ',' && !quoted) { row.push(field); field = ''; }
    else if (ch === '\r' && text[i+1] === '\n' && !quoted) { row.push(field); rows.push(row); row = []; field = ''; i++; }
    else field += ch;
  }
  row.push(field); rows.push(row); return rows;
}
// SCENARIO 1: Follow a real passenger journey in the live sequence and freeze only its view
try {
  for (let i=0; i<100 && !log.includes('Telemetry:'); i++) await new Promise(resolve => setTimeout(resolve,30));
  assert.match(log,/Telemetry:/);
  browser = await chromium.launch(process.platform === 'win32' ? {channel:'msedge',headless:true} : {headless:true});
  const page = await browser.newPage({viewport:{width:1440,height:1050},acceptDownloads:true});
  const errors = []; page.on('pageerror',e => errors.push(e.message));
  await page.goto(base);
  await page.waitForFunction(() => document.querySelector('#sequence-canvas svg'));
  assert.equal(await page.locator('.diagram-shortcuts a').count(),2);
  await page.locator('.diagram-shortcuts a[href="#live-sequence"]').click();
  const runId = (await (await page.request.get(base+'/api/state')).json()).runId;
  const name = 'Ari <&> "Q"';
  await page.locator('#passenger-name').fill(name);
  await page.locator('#to-floor').selectOption('2');
  await page.locator('#add-button').click();
  await page.waitForFunction(() => document.querySelector('#sequence-events').textContent.includes('boarded'));
  const svg = page.locator('#sequence-canvas svg');
  assert.ok((await svg.textContent()).includes(name));
  assert.equal(await svg.locator('script,img,foreignObject').count(),0);
  await page.locator('#sequence-pause').click();
  const frozen = await svg.innerHTML(), frozenCaption = await page.locator('#sequence-progress').textContent();
  await page.waitForFunction(() => document.querySelector('#arrived-count').textContent === '1',{},{timeout:20000});
  assert.equal(await svg.innerHTML(),frozen);
  assert.equal(await page.locator('#sequence-progress').textContent(),frozenCaption);
  await page.locator('#sequence-pause').click();
  await page.waitForFunction(() => document.querySelector('#sequence-events').textContent.includes('arrived'));
  assert.match(await page.locator('#sequence-progress').textContent(),/1 arrived/);
  /**
   * Capture a browser download initiated by the selected button
   * Return its UTF-8 contents for schema, escaping and record-count assertions
   */
  const download = async button => {
    const promise = page.waitForEvent('download'); await page.locator(button).click();
    return fs.readFile(await (await promise).path(),'utf8');
  };
  const image = await download('#sequence-download');
  assert.match(image,/<svg/); assert.match(image,/Ari &lt;&amp;&gt;/); assert.ok(!image.includes('<script'));
  await page.locator('#sequence-scroll').evaluate(e => { e.scrollTop = 0; });
  await page.locator('#live-sequence').screenshot({path:path.join(root,'build/live-sequence-desktop.png')});
  // SCENARIO 2: Populate feedback and enough samples to force multi-page combined exports
  const feedbackText = '=Good, "ride"\nCafé <img src=x>';
  const feedback = await page.request.post(base+'/api/feedback',{form:{runId,passengerId:1,text:feedbackText,rating:5}});
  assert.equal(feedback.status(),200);
  await page.waitForFunction(() => window.ElevatorLab.getState().tick > 220,{},{timeout:30000});
  const cutoff = (await (await page.request.get(base+'/api/state')).json()).tick;
  await page.locator('#telemetry-to').fill(String(cutoff));
  await page.locator('#telemetry-dataset').selectOption('passengers');
  await page.locator('#telemetry-passenger').fill('160'); // All types must ignore this passenger-only filter.
  await page.locator('#telemetry-export-scope').selectOption('all');
  // SCENARIO 3: Check all four record types, run identity, chronology and preserved original values
  const exported = JSON.parse(await download('#export-json'));
  assert.equal(exported.dataset,'all'); assert.equal(exported.runId,runId);
  assert.equal(Number(exported.filters.toTick),cutoff);
  assert.equal(exported.filters.passengerId,undefined);
  assert.deepEqual([...new Set(exported.rows.map(r => r.recordType))].sort(),['events','feedback','passengers','samples']);
  assert.ok(exported.counts.samples > 200,'export must cross API page boundaries');
  assert.equal(exported.rows.find(r => r.recordType === 'feedback').text,feedbackText);
  assert.equal(exported.counts.passengers,1);
  assert.ok(exported.rows.every((r,i,rows) => r.tick <= cutoff && (!i || rows[i-1].tick <= r.tick)));
  const summary = (await (await page.request.get(base+`/api/telemetry/summary?runId=${runId}`)).json()).rows[0];
  assert.equal(exported.counts.events,summary.eventCount);
  // SCENARIO 4: Verify CSV column alignment and formula-safe multiline feedback
  const csvRows = csv(await download('#export-csv'));
  const columns = csvRows.shift();
  assert.equal(csvRows.length,exported.rows.length);
  assert.ok(csvRows.every(row => row.length === columns.length));
  assert.equal(csvRows.find(row => row[columns.indexOf('recordType')] === 'feedback')[columns.indexOf('text')],"'"+feedbackText);
  // An explicit zero end tick must not be mistaken for an unspecified cutoff.
  await page.locator('#telemetry-to').fill('0');
  const zero = JSON.parse(await download('#export-json'));
  assert.ok(zero.rows.length > 0 && zero.rows.every(r => r.tick === 0));
  await page.locator('#telemetry-to').fill(String(cutoff));
  await page.locator('#telemetry-from').fill('10');
  const ranged = JSON.parse(await download('#export-json'));
  assert.ok(ranged.rows.every(r => r.tick >= 10 && r.tick <= cutoff));
  // Pause retains the displayed run across a reset, then resume follows the new run.
  await page.locator('#sequence-pause').click();
  await page.request.post(base+'/api/control',{form:{action:'reset'}});
  await page.waitForFunction(old => window.ElevatorLab.getState().runId !== old,runId);
  assert.match(await page.locator('#sequence-progress').textContent(),new RegExp(`Run #${runId} `));
  await page.locator('#sequence-pause').click();
  await page.waitForFunction(() => !document.querySelector('#sequence-events').textContent.includes('Ari'));
  assert.equal(await page.locator('#sequence-events li').count(),1);
  // An archived run can still be exported; do not silently switch it to the current run.
  await page.locator('#experiment-run').selectOption(String(runId));
  await page.locator('#telemetry-from').fill('0');
  const archived = JSON.parse(await download('#export-json'));
  assert.equal(archived.runId,runId); assert.equal(archived.counts.feedback,1);
  // SCENARIO 5: Mark delayed updates while keeping local diagram controls available
  await page.route('**/api/state',route => route.abort());
  await page.waitForFunction(() => document.querySelector('#sequence-status').textContent.includes('Updates delayed'));
  assert.equal(await page.locator('#sequence-pause').isDisabled(),false);
  await page.unroute('**/api/state');
  await page.waitForFunction(() => document.querySelector('#sequence-status').textContent.includes('Live'));
  await page.locator('.diagram-shortcuts a[href="#evolution"]').click();
  await page.waitForFunction(() => document.querySelector('[data-stage="live-sequence"]'));
  await page.locator('[data-stage="live-sequence"]').click();
  assert.equal(await page.locator('.evolution-diagram').count(),3);
  // SCENARIO 6: Follow the latest interaction and keep exports usable on a phone
  const phone = await browser.newPage({viewport:{width:390,height:844},isMobile:true,hasTouch:true});
  await phone.goto(base+'/#live-sequence');
  await phone.waitForFunction(() => document.querySelector('#sequence-canvas svg'));
  assert.ok(await phone.locator('#sequence-scroll').evaluate(e => e.scrollLeft > 0),'phone follows the latest interaction instead of showing empty lanes');
  assert.equal(await phone.evaluate(() => document.documentElement.scrollWidth > innerWidth),false);
  await phone.locator('#live-sequence').screenshot({path:path.join(root,'build/live-sequence-mobile.png')});
  await phone.locator('#telemetry-export-scope').selectOption('all');
  await phone.locator('#telemetry').screenshot({path:path.join(root,'build/all-export-mobile.png')});
  assert.deepEqual(errors,[]);
  console.log('PASS: real journey sequence, pause/resume/reset, safe SVG download, delayed updates, dashboard/history links; all four export types, pagination, chronology, tick ranges, archived runs, CSV escaping and desktop/mobile layouts.');
// CLEANUP: Stop the isolated test server and close the browser
} finally { server.kill(); if (browser) await browser.close(); }
