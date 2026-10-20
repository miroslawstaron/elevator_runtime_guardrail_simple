/****************************************
*
* Elevator Lab -- Experiment and Feedback Browser Verification
*
* SPDX-License-Identifier: MIT
*
******************************************/

// Uses the real C/SQLite backend. Optional Playwright/Edge developer dependency.
import assert from 'node:assert/strict';
import {spawn} from 'node:child_process';
import {createRequire} from 'node:module';
import {fileURLToPath} from 'node:url';
import fs from 'node:fs/promises';
import path from 'node:path';
const require = createRequire(import.meta.url);
const {chromium} = require('../build/browser-tools/node_modules/playwright');
const root = fileURLToPath(new URL('../',import.meta.url));
const port = 18092, base = `http://localhost:${port}`;
const binary = path.resolve(process.argv[2] || path.join(root,'build/elevator_web.exe'));
const server = spawn(binary,['--port',String(port),'--database',':memory:','--web-root',path.join(root,'web')],{windowsHide:true});
let log = ''; server.stdout.on('data',data => { log += data; }); server.stderr.on('data',data => { log += data; });
let browser;
// BROWSER SETUP: Use the real C/SQLite service and capture unexpected browser errors
try {
  for (let i = 0; i < 100 && !log.includes('Open http://'); i++) await new Promise(resolve => setTimeout(resolve,30));
  assert.match(log,/Open http:\/\//);
  browser = await chromium.launch(process.platform === 'win32' ? {channel:'msedge',headless:true} : {headless:true});
  const page = await browser.newPage({viewport:{width:1440,height:1050},acceptDownloads:true});
  const errors = []; page.on('pageerror',error => errors.push(error.message));
  page.on('console',message => { if (message.type() === 'error') errors.push(message.text()); });
  await page.goto(base);
  await page.waitForFunction(() => document.querySelector('#recording-status').textContent.includes('Recording'));
  // SCENARIO 1: Create a named run and store UTF-8 feedback through the passenger form
  await page.locator('#experiment-name').fill('Morning experiment <&>');
  await page.locator('#start-experiment').click();
  await page.waitForFunction(() => document.querySelector('#experiment-run').selectedOptions[0]?.textContent.includes('Morning experiment'));
  const runId = await page.locator('#experiment-run').inputValue();
  await page.locator('#passenger-name').fill('Ari <&>');
  await page.locator('#from-floor').selectOption('1'); await page.locator('#to-floor').selectOption('4');
  await page.locator('#add-button').click();
  await page.waitForFunction(() => document.querySelector('#people-total').textContent === '1');
  await page.locator('#passenger-rows .feedback-button').click();
  const text = '=Smooth "ride", thank you.\nCaf\u00e9 <img src=x onerror=alert(1)>';
  await page.locator('#feedback-text').fill(text); await page.locator('#feedback-rating').selectOption('5');
  await page.locator('#save-feedback').click();
  await page.waitForFunction(() => !document.querySelector('#feedback-dialog').open);
  await page.locator('#telemetry-dataset').selectOption('feedback');
  await page.waitForFunction(() => document.querySelector('#telemetry-rows').textContent.includes('Smooth'));
  assert.equal(await page.locator('#telemetry-rows img').count(),0);
  await page.waitForFunction(() => document.querySelector('#recorded-feedback').textContent === '1');
  assert.match(await page.locator('#recorded-rating').textContent(),/5.0/);
  await page.locator('#telemetry-to').fill('0'); await page.locator('#query-telemetry').click();
  await page.waitForFunction(() => document.querySelector('#telemetry-empty').hidden === false);
  assert.ok(await page.locator('#telemetry-head th').count() > 0,'empty results retain schema');
  await page.locator('#telemetry-to').fill(''); await page.locator('#query-telemetry').click();
  await page.waitForFunction(() => document.querySelector('#telemetry-rows').textContent.includes('Smooth'));
  // SCENARIO 2: Inspect downloaded JSON and CSV contents rather than merely checking button clicks
  let downloadPromise = page.waitForEvent('download'); await page.locator('#export-json').click();
  let download = await downloadPromise;
  const exported = JSON.parse(await fs.readFile(await download.path(),'utf8'));
  assert.equal(exported.rows[0].text,text); assert.equal(exported.tickDurationMs,100); assert.equal(exported.rows[0].rating,5);
  downloadPromise = page.waitForEvent('download'); await page.locator('#export-csv').click();
  download = await downloadPromise;
  assert.match(await fs.readFile(await download.path(),'utf8'),/"'=Smooth ""ride""/);
  await page.locator('#telemetry-dataset').selectOption('samples');
  await page.locator('#telemetry-limit').selectOption('25'); await page.locator('#query-telemetry').click();
  await page.waitForFunction(() => !document.querySelector('#telemetry-next').disabled);
  await page.locator('#telemetry-next').click();
  await page.waitForFunction(() => document.querySelector('#telemetry-page').textContent.startsWith('Rows 26'));
  await page.locator('#telemetry-previous').click();
  await page.waitForFunction(() => document.querySelector('#telemetry-previous').disabled);
  // A feedback form keeps its experiment identity even when another visitor resets the simulator.
  await page.locator('#passenger-rows .feedback-button').click();
  await page.locator('#feedback-text').fill('Written after another visitor reset the elevator.');
  const response = await page.request.post(base + '/api/control',{headers:{'Content-Type':'application/x-www-form-urlencoded',Origin:base},data:'action=reset'});
  assert.equal(response.status(),200);
  await page.waitForFunction(() => document.querySelector('#people-total').textContent === '0');
  await page.locator('#save-feedback').click();
  await page.waitForFunction(() => !document.querySelector('#feedback-dialog').open);
  await page.locator('#experiment-run').selectOption(runId);
  await page.locator('#telemetry-dataset').selectOption('feedback');
  await page.waitForFunction(() => document.querySelectorAll('#telemetry-rows tr').length === 2);
  await page.locator('#telemetry').screenshot({path:path.join(root,'build/telemetry-desktop.png')});
  assert.equal(await page.evaluate(() => document.documentElement.scrollWidth > innerWidth),false);
  // SCENARIO 3: Read archived feedback on a narrow phone viewport
  const mobile = await browser.newPage({viewport:{width:390,height:844},isMobile:true,hasTouch:true});
  await mobile.goto(base + '/#telemetry');
  await mobile.waitForFunction(() => document.querySelector('#experiment-run').options.length >= 2);
  await mobile.locator('#experiment-run').selectOption(runId);
  await mobile.locator('#telemetry-dataset').selectOption('feedback');
  await mobile.waitForFunction(() => document.querySelectorAll('#telemetry-rows tr').length === 2);
  assert.equal(await mobile.evaluate(() => document.documentElement.scrollWidth > innerWidth),false);
  await mobile.locator('#telemetry').screenshot({path:path.join(root,'build/telemetry-mobile.png')});
  await mobile.locator('#telemetry-dataset').selectOption('passengers');
  await mobile.waitForFunction(() => document.querySelector('#telemetry-rows .feedback-button'));
  await mobile.locator('#telemetry-rows .feedback-button').click();
  assert.equal(await mobile.locator('#feedback-run').inputValue(),runId);
  await mobile.locator('#feedback-dialog').screenshot({path:path.join(root,'build/feedback-mobile.png')});
  await mobile.locator('#cancel-feedback').click();
  assert.deepEqual(errors,[]);
  console.log('PASS: desktop/phone telemetry explorer, named experiments, live and archived feedback, safe text rendering, optional ratings, filtered queries, pagination, JSON/CSV downloads, feedback across resets, and no page overflow.');
// CLEANUP: Close the test browser and isolated C process
} finally { server.kill(); if (browser) await browser.close(); }
