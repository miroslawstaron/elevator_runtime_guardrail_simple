// Optional browser QA: npm install --prefix build/browser-tools playwright --no-save
// EDGE_PATH can override the installed browser. No browser dependency is needed to run the app.
import assert from 'node:assert/strict';
import {createRequire} from 'node:module';
import {spawn} from 'node:child_process';
import {fileURLToPath} from 'node:url';
import path from 'node:path';
const require = createRequire(import.meta.url);
const {chromium} = require('../build/browser-tools/node_modules/playwright');
const root = fileURLToPath(new URL('../', import.meta.url));
const port = 18090;
const server = spawn(path.join(root, 'build', process.platform === 'win32' ? 'elevator_web.exe' : 'elevator_web'), ['--port', String(port), '--web-root', path.join(root, 'web')], {windowsHide:true});
let startup = '';
server.stdout.on('data', data => { startup += data; });
server.stderr.on('data', data => { startup += data; });
let browser;
try {
  for (let i = 0; i < 60 && !startup.includes('Open http://'); i++) await new Promise(resolve => setTimeout(resolve, 50));
  assert.match(startup, /Open http:\/\//);
  browser = await chromium.launch(process.env.EDGE_PATH ? {executablePath:process.env.EDGE_PATH, headless:true} : process.platform === 'win32' ? {channel:'msedge',headless:true} : {headless:true});
  const page = await browser.newPage({viewport:{width:1440,height:1050}});
  const errors = [];
  page.on('pageerror', error => errors.push(error.message));
  page.on('console', message => { if (message.type() === 'error') errors.push(message.text()); });
  await page.goto(`http://localhost:${port}`);
  await page.waitForFunction(() => document.querySelector('#connection').textContent.includes('C runtime connected'));
  assert.equal(await page.locator('.floor-row').count(), 10);
  assert.equal(await page.locator('.floor-button').count(), 10);
  assert.ok(await page.locator('.brand img').evaluate(image => image.complete && image.naturalWidth > 0));
  await page.locator('#to-floor').selectOption('1');
  await page.locator('#add-button').click();
  assert.match(await page.locator('#form-error').textContent(), /different/);
  await page.locator('#passenger-name').fill('<img src=x onerror=alert(1)>');
  await page.locator('#to-floor').selectOption('7');
  await page.locator('#people-count').selectOption('2');
  await page.locator('#add-button').click();
  await page.waitForFunction(() => document.querySelector('#people-total').textContent === '2');
  assert.equal(await page.locator('#passenger-rows img').count(), 0, 'names must render as text');
  await page.waitForFunction(() => document.querySelector('#riding-count').textContent === '2');
  await page.waitForFunction(() => document.querySelector('#motion-status').textContent.includes('Moving'));
  assert.ok(await page.locator('#open-button').isDisabled());
  await page.locator('#stop-button').click();
  await page.waitForFunction(() => document.querySelector('#motion-status').textContent === 'Emergency stop');
  assert.ok(await page.locator('#stop-notice').isVisible());
  await page.locator('#stop-button').click();
  await page.waitForFunction(() => document.querySelector('#arrived-count').textContent === '2', null, {timeout:15000});
  await page.locator('#journey-filter').selectOption('waiting');
  assert.equal(await page.locator('#passenger-rows tr').count(), 0);
  await page.locator('#journey-filter').selectOption('arrived');
  assert.equal(await page.locator('#passenger-rows tr').count(), 2);
  await page.locator('#reset-button').click();
  await page.waitForFunction(() => document.querySelector('#people-total').textContent === '0');
  await page.locator('#demo-button').click();
  await page.waitForFunction(() => document.querySelector('#people-total').textContent === '6');
  await page.screenshot({path:path.join(root,'build/desktop-demo.png'),fullPage:true});
  const context = await browser.newContext({viewport:{width:390,height:844},isMobile:true,hasTouch:true});
  const mobile = await context.newPage();
  await mobile.goto(`http://localhost:${port}`);
  await mobile.waitForFunction(() => document.querySelector('#people-total').textContent === '6');
  assert.equal(await mobile.evaluate(() => document.documentElement.scrollWidth > innerWidth), false, 'no horizontal overflow on phone');
  await mobile.screenshot({path:path.join(root,'build/mobile-demo.png'),fullPage:true});
  assert.equal(await page.evaluate(() => document.documentElement.scrollWidth > innerWidth), false);
  await page.locator('#reset-button').click();
  await page.waitForFunction(() => document.querySelector('#people-total').textContent === '0');
  // A second browser sees the same C state; stopping the service disables stale controls.
  await mobile.waitForFunction(() => document.querySelector('#people-total').textContent === '0');
  server.kill();
  await page.waitForFunction(() => document.querySelector('#connection').classList.contains('offline'), null, {timeout:10000});
  assert.ok(await page.locator('#stop-button').isDisabled());
  assert.ok(await page.locator('#add-button').isDisabled());
  // Expected network errors only after the intentional disconnect.
  assert.deepEqual(errors.filter(error => !/ERR_CONNECTION|Failed to fetch|Failed to load resource/.test(error)), []);
  console.log('PASS: real Edge desktop/phone UI, logo, form validation, escaped names, boarding/travel/arrival, moving interlocks, stop/resume, filters, demo, shared state, reset, offline controls, no horizontal overflow');
} finally {
  server.kill();
  if (browser) await browser.close();
}
