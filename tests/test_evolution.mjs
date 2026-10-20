/****************************************
*
* Elevator Lab -- Evolution Diagram Browser Verification
*
* SPDX-License-Identifier: MIT
*
******************************************/

// Real C HTTP and browser checks for the published, catalog-driven evolution viewer.
import assert from 'node:assert/strict';
import {spawn} from 'node:child_process';
import {createRequire} from 'node:module';
import fs from 'node:fs/promises';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
const require = createRequire(import.meta.url);
const {chromium} = require('../build/browser-tools/node_modules/playwright');
const root = fileURLToPath(new URL('../', import.meta.url));
const port = 18094, base = `http://localhost:${port}`;
const binary = path.resolve(process.argv[2] || path.join(root, 'build/elevator_web.exe'));
const server = spawn(binary, ['--port', String(port), '--database', ':memory:', '--web-root', path.join(root, 'web')], {windowsHide:true});
let startup = ''; server.stdout.on('data', data => { startup += data; }); server.stderr.on('data', data => { startup += data; });
server.on('error', error => { startup += error.message; });
let browser;
// SCENARIO 1: Verify every catalog stage and its served SVG, source and notes
try {
  for (let i = 0; i < 100 && !startup.includes('Telemetry:'); i++) await new Promise(resolve => setTimeout(resolve, 30));
  assert.match(startup, /Telemetry:/);
  const catalogResponse = await fetch(base + '/evolution/catalog.json');
  assert.equal(catalogResponse.status, 200);
  const catalog = await catalogResponse.json();
  assert.ok(catalog.stages.length >= 6);
  let diagramCount = 0;
  for (const stage of catalog.stages) {
    assert.ok(stage.diagrams.some(d => d.type === 'architecture'));
    assert.ok(stage.diagrams.some(d => d.type === 'sequence'));
    const notes = await fetch(base + stage.notes); assert.equal(notes.status, 200);
    assert.match(await notes.text(), /```mermaid/);
    for (const diagram of stage.diagrams) {
      const svg = await fetch(base + diagram.svg); assert.equal(svg.status, 200);
      assert.ok(svg.headers.get('content-type').startsWith('image/svg+xml'));
      assert.ok(svg.headers.get('content-security-policy').includes("default-src 'none'"));
      assert.ok(svg.headers.get('content-security-policy').includes('sandbox'));
      assert.match(await svg.text(), /<svg/);
      const source = await fetch(base + diagram.mermaid); assert.equal(source.status, 200);
      assert.match(await source.text(), /flowchart|sequenceDiagram/);
      diagramCount++;
    }
  }
  // SCENARIO 2: Reject traversal, unsupported names and accidental database/source exposure
  for (const endpoint of ['/evolution/diagrams/%2e%2e%2fsrc%2fweb_server.c.svg', '/evolution/diagrams/.hidden.svg', '/evolution/diagrams/a..svg', '/evolution/diagrams/before-01-architecture.exe', '/evolution/stages/telemetry.sqlite3', '/evolution/stages/AGENTS.md', '/docs/evolution/01-before.md', '/data/elevator.sqlite3']) {
    assert.equal((await fetch(base + endpoint)).status, 404, endpoint);
  }
  browser = await chromium.launch(process.platform === 'win32' ? {channel:'msedge', headless:true} : {headless:true});
  const page = await browser.newPage({viewport:{width:1440, height:1050}, acceptDownloads:true});
  const errors = []; page.on('pageerror', error => errors.push(error.message));
  page.on('console', message => { if (message.type() === 'error') errors.push(message.text()); });
  await page.goto(base + '/#evolution');
  await page.waitForFunction(() => document.querySelectorAll('.evolution-step').length >= 6);
  assert.equal(await page.locator('.evolution-step').count(), catalog.stages.length);
  // SCENARIO 3: Browse every stage and exercise architecture/sequence filters
  for (const stage of catalog.stages) {
    await page.locator(`[data-stage="${stage.slug}"]`).click();
    assert.equal(await page.locator('#evolution-stage-title').textContent(), stage.title);
    assert.equal(await page.locator('.evolution-diagram').count(), stage.diagrams.length);
    await page.waitForFunction(() => [...document.querySelectorAll('.evolution-preview img')].every(img => img.complete && img.naturalWidth > 0));
    await page.locator('[data-diagram-filter="architecture"]').click();
    assert.equal(await page.locator('.evolution-diagram').count(), stage.diagrams.filter(d => d.type === 'architecture').length);
    await page.locator('[data-diagram-filter="sequence"]').click();
    assert.equal(await page.locator('.evolution-diagram').count(), stage.diagrams.filter(d => d.type === 'sequence').length);
    await page.locator('[data-diagram-filter="all"]').click();
  }
  // SCENARIO 4: Open, zoom and fit diagrams, then validate actual source and SVG downloads
  await page.locator('.evolution-preview').first().click();
  await page.waitForFunction(() => document.querySelector('#diagram-full-image').naturalWidth > 0 && document.querySelector('#diagram-full-image').style.width);
  assert.ok(await page.locator('#diagram-dialog').isVisible());
  const initialWidth = await page.locator('#diagram-full-image').evaluate(img => img.getBoundingClientRect().width);
  await page.locator('#diagram-zoom-in').click();
  assert.ok(await page.locator('#diagram-full-image').evaluate(img => img.getBoundingClientRect().width) > initialWidth);
  assert.equal(await page.locator('#diagram-zoom-level').textContent(), '125%');
  await page.locator('#diagram-fit').click();
  assert.equal(await page.locator('#diagram-zoom-level').textContent(), '100%');
  await page.keyboard.press('Escape');
  assert.equal(await page.locator('#diagram-dialog').isVisible(), false);
  const downloadPromise = page.waitForEvent('download');
  await page.locator('.evolution-diagram-footer a').filter({hasText:'Mermaid source'}).first().click();
  const download = await downloadPromise;
  assert.match(await fs.readFile(await download.path(), 'utf8'), /flowchart/);
  const svgDownloadPromise = page.waitForEvent('download');
  await page.locator('.evolution-diagram-footer a').filter({hasText:'SVG'}).first().click();
  const svgDownload = await svgDownloadPromise;
  assert.match(await fs.readFile(await svgDownload.path(), 'utf8'), /<svg/);
  const notesDownloadPromise = page.waitForEvent('download');
  await page.locator('#evolution-stage-notes').click();
  const notesDownload = await notesDownloadPromise;
  assert.match(await fs.readFile(await notesDownload.path(), 'utf8'), /sequenceDiagram/);
  await page.locator('#evolution').screenshot({path:path.join(root, 'build/evolution-desktop.png')});
  assert.equal(await page.evaluate(() => document.documentElement.scrollWidth > innerWidth), false);
  // Bookmarking selects a historical stage rather than silently returning to the latest.
  await page.goto(base + '/?stage=telemetry#evolution');
  await page.waitForFunction(() => document.querySelector('[data-stage="telemetry"]')?.getAttribute('aria-current') === 'step');
  // SCENARIO 5: Keep stage selection and modal viewing usable on phones
  const mobile = await browser.newPage({viewport:{width:390, height:844}, isMobile:true, hasTouch:true});
  await mobile.goto(base + '/?stage=evolution#evolution');
  await mobile.waitForFunction(() => document.querySelectorAll('.evolution-preview img').length > 0);
  await mobile.locator('[data-stage="before"]').click();
  await mobile.waitForFunction(() => [...document.querySelectorAll('.evolution-preview img')].every(img => img.complete && img.naturalWidth > 0));
  assert.equal(await mobile.evaluate(() => document.documentElement.scrollWidth > innerWidth), false);
  await mobile.locator('#evolution').screenshot({path:path.join(root, 'build/evolution-mobile.png')});
  await mobile.locator('.evolution-preview').first().click();
  await mobile.waitForFunction(() => document.querySelector('#diagram-full-image').style.width);
  await mobile.locator('#diagram-dialog').screenshot({path:path.join(root, 'build/evolution-zoom-mobile.png')});
  await mobile.locator('#close-diagram').click();
  assert.deepEqual(errors, []);
  console.log(`PASS: ${catalog.stages.length} stages / ${diagramCount} served SVGs and sources, restricted paths/CSP, stage selection, architecture/sequence filters, loaded images, zoom, downloads, bookmarks and desktop/phone layouts.`);
// CLEANUP: Release both isolated server and browser after success or failure
} finally { server.kill(); if (browser) await browser.close(); }
