/****************************************
*
* Elevator Lab -- Published Documentation Freshness Checks
*
* SPDX-License-Identifier: MIT
*
******************************************/

// No rendering dependencies: suitable for CI and ordinary C builds.
import fs from 'node:fs/promises';
import path from 'node:path';
import assert from 'node:assert/strict';
import {root, loadStages, digest, normalize} from './evolution-lib.mjs';

// CHECK 1: Load source truth with no browser or rendering dependency
const stages = await loadStages();
const catalog = JSON.parse(await fs.readFile(path.join(root, 'web/evolution/catalog.json'), 'utf8'));
assert.equal(catalog.schemaVersion, 1);
assert.equal(catalog.stages.length, stages.length, 'Regenerate the web catalog after appending a stage.');
let count = 0;
// CHECK 2: Compare published stage order, metadata, notes and diagram coverage to source
for (let i = 0; i < stages.length; i++) {
  const stage = stages[i], published = catalog.stages[i];
  for (const field of ['id', 'slug', 'title', 'date', 'summary', 'sourceHash']) assert.equal(published[field], stage[field], `Stale stage metadata: ${stage.file}`);
  assert.equal(published.notes, `/evolution/stages/${stage.slug}.md`);
  assert.equal(normalize(await fs.readFile(path.join(root, 'web', published.notes.slice(1)), 'utf8')), stage.markdown);
  assert.equal(published.diagrams.length, stage.diagrams.length);
  // CHECK 3: Verify stable links, source hashes, SVG hashes and both generated mirrors
  for (let j = 0; j < stage.diagrams.length; j++) {
    const diagram = stage.diagrams[j], entry = published.diagrams[j];
    for (const field of ['name', 'title', 'type', 'sourceHash']) assert.equal(entry[field], diagram[field], `Stale diagram: ${diagram.name}`);
    assert.equal(entry.svg, `/evolution/diagrams/${diagram.name}.svg`);
    assert.equal(entry.mermaid, `/evolution/diagrams/${diagram.name}.mmd`);
    const svg = await fs.readFile(path.join(root, 'web', entry.svg.slice(1)), 'utf8');
    const source = await fs.readFile(path.join(root, 'web', entry.mermaid.slice(1)), 'utf8');
    assert.ok(svg.includes('<svg') && svg.includes('</svg>'), `Missing SVG: ${diagram.name}`);
    assert.equal(digest(svg), entry.svgHash, `Modified SVG: ${diagram.name}`);
    assert.equal(normalize(source), diagram.code, `Stale Mermaid source: ${diagram.name}`);
    assert.equal(normalize(await fs.readFile(path.join(root, 'docs/evolution/diagrams', `${diagram.name}.svg`), 'utf8')), normalize(svg));
    assert.equal(normalize(await fs.readFile(path.join(root, 'docs/evolution/diagrams', `${diagram.name}.mmd`), 'utf8')), diagram.code);
    count++;
  }
}
// CHECK 4: Ensure every documented stage also appears in the offline gallery
const gallery = await fs.readFile(path.join(root, 'docs/evolution/diagrams.html'), 'utf8');
for (const stage of stages) assert.ok(gallery.includes(`id="${stage.slug}"`), `Offline gallery is missing ${stage.slug}.`);
console.log(`PASS: ${stages.length} evolution stages, ${count} diagrams; architecture/sequence coverage, source hashes, web assets and offline gallery are current.`);
