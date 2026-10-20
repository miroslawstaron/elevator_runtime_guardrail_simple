/****************************************
 * Elevator Lab -- Scenario and Analysis Browser Integration Checks
 * SPDX-License-Identifier: MIT
 ****************************************/
import assert from 'node:assert/strict';
import {spawn} from 'node:child_process';
import {createRequire} from 'node:module';
import {fileURLToPath} from 'node:url';
import path from 'node:path';
const require = createRequire(import.meta.url);
const {chromium} = require('../build/browser-tools/node_modules/playwright');
const root = fileURLToPath(new URL('../',import.meta.url));
const base = 'http://localhost:18097';
const server = spawn(path.join(root,'build/elevator_web.exe'),['--port','18097','--database',':memory:','--web-root',path.join(root,'web')],{windowsHide:true});
let log = '', browser;
server.stdout.on('data',data => { log += data; }); server.stderr.on('data',data => { log += data; });
/** Read real state without relying on browser presentation. */
const state = async () => (await fetch(base+'/api/state')).json();
try {
  for (let i = 0; i < 100 && !log.includes('Telemetry:'); i++) await new Promise(resolve => setTimeout(resolve,30));
  assert.match(log,/Telemetry:/);
  browser = await chromium.launch(process.platform === 'win32' ? {channel:'msedge',headless:true} : {headless:true});
  const page = await browser.newPage({viewport:{width:1440,height:1050}}), errors = [];
  page.on('pageerror',error => errors.push(error.message));
  page.on('console',message => { if (message.type() === 'error') errors.push(message.text()); });
  await page.goto(base+'/#scenarios');
  await page.waitForFunction(() => document.querySelector('#run-scenario').disabled === false);
  // PRESETS: Exercise every visible scenario and preserve their intended default traffic.
  for (const [kind,count] of [['morning',300],['afternoon',300],['occasional',24],['vip',40]]) {
    await page.locator(`[data-preset="${kind}"]`).click();
    assert.equal(await page.locator('#scenario-count').inputValue(),String(count));
    const previous = (await state()).runId;
    await page.locator('#run-scenario').click();
    await page.waitForFunction(run => window.ElevatorLab.getState().runId > run,previous);
    const current = await state();
    assert.equal(current.scenario,kind); assert.equal(current.people.length,count);
    assert.equal(current.speed,1);
  }
  // ANALYSIS: The report exposes evidence and ignores table-only filters.
  const vipRun = (await state()).runId;
  await page.waitForFunction(run => document.querySelector('#experiment-run').value === String(run),vipRun);
  await page.locator('#analyze-telemetry').click();
  await page.waitForFunction(() => document.querySelector('#analysis-results').hidden === false);
  assert.match(await page.locator('#analysis-status').textContent(),/Live \/ preliminary/);
  assert.match(await page.locator('#analysis-recommendations').textContent(),/Collect more completed journeys/);
  const beforeAnalysis = (await state()).runId;
  assert.equal(beforeAnalysis,vipRun);
  // CUSTOM: Validate editing, no mutation on rejected routes, timed VIPs and real completion.
  await page.locator('[data-preset="custom"]').click();
  const first = page.locator('.scenario-flow').first();
  await first.locator('[data-field="count"]').fill('8');
  await first.locator('[data-field="interval"]').fill('0');
  await first.locator('[data-field="to"]').selectOption('0');
  await page.locator('#run-scenario').click();
  assert.equal(await page.locator('#scenario-error').isVisible(),true);
  assert.equal((await state()).runId,vipRun);
  await first.locator('[data-field="to"]').selectOption('5');
  await page.locator('#add-scenario-flow').click();
  const second = page.locator('.scenario-flow').nth(1);
  await second.locator('[data-field="count"]').fill('2');
  await second.locator('[data-field="from"]').selectOption('5');
  await second.locator('[data-field="to"]').selectOption('0');
  await second.locator('[data-field="start"]').fill('3');
  await second.locator('[data-field="interval"]').fill('0');
  await second.locator('[data-field="vip"]').selectOption('1');
  await page.locator('#run-scenario').click();
  await page.waitForFunction(run => window.ElevatorLab.getState().runId > run,vipRun);
  assert.equal((await state()).people.filter(p => p.vip).length,2);
  await page.locator('#simulation-speed').selectOption('20');
  await page.waitForFunction(() => window.ElevatorLab.getState().completed === 10,null,{timeout:20000});
  assert.match(await page.locator('#scenario-status').textContent(),/Complete/);
  assert.equal(await page.locator('#passenger-rows tr').count(),10);
  assert.ok((await state()).people.every(p => p.status === 'arrived'));
  assert.equal((await state()).telemetry.lostEvents,0);
  const customRun = (await state()).runId;
  await page.waitForFunction(run => document.querySelector('#experiment-run').value === String(run),customRun);
  await page.locator('#analyze-telemetry').click();
  await page.waitForFunction(() => !document.querySelector('#analysis-results').hidden);
  assert.equal(await page.locator('.analysis-metrics>div').count(),8);
  await page.locator('.analysis-panel').screenshot({path:path.join(root,'build/scenario-analysis-desktop.png')});
  // HISTORY: Switching runs invalidates reports and archived runs remain analyzable.
  await page.locator('#experiment-run').selectOption(String(vipRun));
  assert.equal(await page.locator('#analysis-results').isVisible(),false);
  await page.locator('#analyze-telemetry').click();
  await page.waitForFunction(() => !document.querySelector('#analysis-results').hidden);
  assert.match(await page.locator('#analysis-status').textContent(),/Archived/);
  // BURST: The 300-person morning is rendered within the bounded live table.
  await page.locator('[data-preset="morning"]').click();
  await page.locator('#scenario-duration').fill('0');
  await page.locator('#run-scenario').click();
  await page.waitForFunction(() => window.ElevatorLab.getState().people.length === 300);
  assert.equal(await page.locator('#passenger-rows tr').count(),100);
  await page.locator('#scenarios').screenshot({path:path.join(root,'build/scenarios-desktop.png')});
  assert.equal(await page.evaluate(() => document.documentElement.scrollWidth > innerWidth),false);
  // PHONE: Stream editing, scenario progress and recommendations fit a narrow screen.
  const mobile = await browser.newPage({viewport:{width:390,height:844},isMobile:true,hasTouch:true});
  mobile.on('pageerror',error => errors.push(error.message));
  await mobile.goto(base+'/#scenarios');
  await mobile.waitForFunction(() => document.querySelector('#run-scenario').disabled === false);
  await mobile.locator('[data-preset="custom"]').click();
  await mobile.locator('#add-scenario-flow').click();
  assert.equal(await mobile.evaluate(() => document.documentElement.scrollWidth > innerWidth),false);
  await mobile.locator('#scenarios').screenshot({path:path.join(root,'build/scenarios-mobile.png')});
  await mobile.locator('#analyze-telemetry').click();
  await mobile.waitForFunction(() => !document.querySelector('#analysis-results').hidden);
  assert.match(await mobile.locator('#analysis-recommendations').textContent(),/morning lobby-return policy/);
  assert.equal(await mobile.evaluate(() => document.documentElement.scrollWidth > innerWidth),false);
  await mobile.locator('.analysis-panel').screenshot({path:path.join(root,'build/scenario-analysis-mobile.png')});
  assert.deepEqual(errors,[]);
  console.log('PASS: five scenario choices, 300-person morning/afternoon, custom validation/streams/VIP timing, accelerated completion, snapshot analysis, history invalidation, bounded live rows and desktop/phone layouts');
} finally { server.kill(); if (browser) await browser.close(); }
