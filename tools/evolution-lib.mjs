/****************************************
*
* Elevator Lab -- Evolution Registry and Source Validation
*
* SPDX-License-Identifier: MIT
*
******************************************/

import fs from 'node:fs/promises';
import path from 'node:path';
import crypto from 'node:crypto';
import {fileURLToPath} from 'node:url';

export const root = fileURLToPath(new URL('../', import.meta.url));
/**
 * Normalize source line endings before hashing or comparing generated documentation
 */
export const normalize = text => text.replace(/\r\n/g, '\n');
/**
 * Compute the SHA-256 digest of normalized documentation text
 */
export const digest = text => crypto.createHash('sha256').update(normalize(text)).digest('hex');
/**
 * Escape source metadata for safe inclusion in the generated offline gallery
 */
export const escapeHtml = text => String(text).replace(/[&<>"']/g, char => ({'&':'&amp;', '<':'&lt;', '>':'&gt;', '"':'&quot;', "'":'&#39;'}[char]));

/**
 * Load ordered stage snapshots and extract their supported Mermaid diagrams
 * Validate unique identities, constrained filenames, dates and architecture/sequence coverage
 * Return normalized sources and hashes for both generation and freshness checks
 */
export async function loadStages() {
  const registry = JSON.parse(await fs.readFile(path.join(root, 'docs/evolution/stages.json'), 'utf8'));
  if (registry.schemaVersion !== 1 || !Array.isArray(registry.stages) || !registry.stages.length) throw new Error('Invalid evolution registry.');
  const ids = new Set(), slugs = new Set(), files = new Set();
  let previous = 0;
  const stages = [];
  // REGISTRY 1: Reject reordered, duplicate or malformed stage metadata before reading files
  for (const item of registry.stages) {
    if (!/^\d{2,}$/.test(item.id) || Number(item.id) <= previous || ids.has(item.id) ||
        !/^[a-z0-9]+(?:-[a-z0-9]+)*$/.test(item.slug) || slugs.has(item.slug) ||
        !/^\d{2,}-[a-z0-9-]+\.md$/.test(item.file) || files.has(item.file) ||
        !item.title || !item.summary || !/^\d{4}-\d{2}-\d{2}$/.test(item.date) ||
        (item.diagramTheme !== undefined && !['legacy','metriczone'].includes(item.diagramTheme))) {
      throw new Error(`Invalid, duplicate or unordered stage: ${item.id}`);
    }
    previous = Number(item.id); ids.add(item.id); slugs.add(item.slug); files.add(item.file);
    // REGISTRY 2: Read the exact numbered snapshot and extract fenced Mermaid sources
    const markdown = normalize(await fs.readFile(path.join(root, 'docs/evolution', item.file), 'utf8'));
    const blocks = [...markdown.matchAll(/```mermaid[ \t]*\n([\s\S]*?)```/g)];
    // REGISTRY 3: Give each diagram a stable name and retain its nearest section heading
    const diagrams = blocks.map((block, index) => {
      const code = block[1].trim() + '\n';
      const type = /^(flowchart|graph)\s/.test(code) ? 'architecture' : /^sequenceDiagram\b/.test(code) ? 'sequence' : null;
      if (!type) throw new Error(`Unsupported diagram in ${item.file}: use flowchart or sequenceDiagram.`);
      const headings = [...markdown.slice(0, block.index).matchAll(/^##\s+(.+)$/gm)];
      const title = headings.at(-1)?.[1] || (type === 'architecture' ? 'Architecture' : 'Interaction sequence');
      const name = `${item.slug}-${String(index + 1).padStart(2, '0')}-${type}`;
      return {name, title, type, code, sourceHash:digest(code)};
    });
    // REGISTRY 4: Every implementation snapshot must explain both architecture and interaction sequence
    if (!diagrams.some(d => d.type === 'architecture') || !diagrams.some(d => d.type === 'sequence')) {
      throw new Error(`${item.file} needs at least one architecture and one sequence diagram.`);
    }
    stages.push({...item, markdown, sourceHash:digest(markdown), diagrams});
  }
  return stages;
}
