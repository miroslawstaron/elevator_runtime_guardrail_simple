/****************************************
*
* Elevator Lab -- Documentation Diagram Generation
*
* SPDX-License-Identifier: MIT
*
******************************************/

// npm install --prefix build/browser-tools playwright --no-save
// npm install --prefix build/diagram-tools mermaid --no-save
// node tools/render-diagrams.mjs
import fs from 'node:fs/promises';
import path from 'node:path';
import {createRequire} from 'node:module';
import {root, loadStages, digest, escapeHtml as html} from './evolution-lib.mjs';

const require = createRequire(import.meta.url);
const {chromium} = require('../build/browser-tools/node_modules/playwright');
// GENERATION 1: Validate all source snapshots before preparing documentation directories
const stages = await loadStages();
const docsDirectory = path.join(root, 'docs/evolution/diagrams');
const webDirectory = path.join(root, 'web/evolution');
await Promise.all([
  fs.mkdir(docsDirectory, {recursive:true}),
  fs.mkdir(path.join(webDirectory, 'diagrams'), {recursive:true}),
  fs.mkdir(path.join(webDirectory, 'stages'), {recursive:true}),
  fs.mkdir(path.join(root, 'build'), {recursive:true})
]);
// RENDERER: Use a development-only browser; the deployed simulator does not depend on Node or Mermaid
const browser = await chromium.launch(process.env.EDGE_PATH ? {executablePath:process.env.EDGE_PATH, headless:true} : process.platform === 'win32' ? {channel:'msedge', headless:true} : {headless:true});
const catalog = {schemaVersion:1, title:'System evolution', stages:[]};
let count = 0;
try {
  const page = await browser.newPage({viewport:{width:1500, height:1000}});
  await page.setContent('<!doctype html><html><body></body></html>');
  await page.addScriptTag({path:path.join(root, 'build/diagram-tools/node_modules/mermaid/dist/mermaid.min.js')});
  // GENERATION 2: Initialize the original palette so historical SVG snapshots remain unchanged
  await page.evaluate(() => mermaid.initialize({
    startOnLoad:false, htmlLabels:false, theme:'base', securityLevel:'strict', fontFamily:'Segoe UI, Arial, sans-serif',
    themeVariables:{primaryColor:'#eef4df', primaryTextColor:'#173d45', primaryBorderColor:'#9eaf82', lineColor:'#53778a', secondaryColor:'#eaf1f2', tertiaryColor:'#f9f6ed', mainBkg:'#f2f6e9', nodeBorder:'#a2b28c', clusterBkg:'#f7f8f3', clusterBorder:'#d7dfcd', actorBkg:'#eaf1f2', actorBorder:'#91a5ac', actorTextColor:'#073e51', signalColor:'#456777', signalTextColor:'#173d45', noteBkgColor:'#f7f2dd', noteBorderColor:'#d8cda6', fontSize:'14px'},
    flowchart:{htmlLabels:false, useMaxWidth:true, wrappingWidth:220},
    sequence:{useMaxWidth:true, wrap:true, actorMargin:35, messageMargin:26}
  }));
  // GENERATION 3: Mirror each stage and render stable SVG/Mermaid pairs for web and offline use
  for (const stage of stages) {
    // Branding is versioned: preserve the existing SVG styling of historical stages.
    if (stage.diagramTheme === 'metriczone') await page.evaluate(() => mermaid.initialize({
      startOnLoad:false, htmlLabels:false, theme:'base', securityLevel:'strict', fontFamily:'Segoe UI, Arial, sans-serif',
      themeVariables:{primaryColor:'#e8f3f4', primaryTextColor:'#153e48', primaryBorderColor:'#80b4c0', lineColor:'#196575', secondaryColor:'#edf6f8', tertiaryColor:'#f8fbfc', mainBkg:'#e8f3f4', nodeBorder:'#80b4c0', clusterBkg:'#f4f9fa', clusterBorder:'#d5e4e7', actorBkg:'#e8f3f4', actorBorder:'#80b4c0', actorTextColor:'#003e4d', signalColor:'#196575', signalTextColor:'#153e48', noteBkgColor:'#edf6f8', noteBorderColor:'#a6cbd4', fontSize:'14px'},
      flowchart:{htmlLabels:false, useMaxWidth:true, wrappingWidth:220},
      sequence:{useMaxWidth:true, wrap:true, actorMargin:35, messageMargin:26}
    }));
    const entry = {id:stage.id, slug:stage.slug, title:stage.title, date:stage.date, summary:stage.summary, sourceHash:stage.sourceHash, notes:`/evolution/stages/${stage.slug}.md`, diagrams:[]};
    await fs.writeFile(path.join(webDirectory, 'stages', `${stage.slug}.md`), stage.markdown);
    // DIAGRAM EXPORT: Use deterministic IDs and hash both editable source and rendered SVG
    for (const diagram of stage.diagrams) {
      const svg = await page.evaluate(async ({code, id}) => (await mermaid.render(id, code)).svg, {code:diagram.code, id:`diagram${count}`});
      diagram.svgContent = svg;
      await Promise.all([
        fs.writeFile(path.join(docsDirectory, `${diagram.name}.svg`), svg),
        fs.writeFile(path.join(docsDirectory, `${diagram.name}.mmd`), diagram.code),
        fs.writeFile(path.join(webDirectory, 'diagrams', `${diagram.name}.svg`), svg),
        fs.writeFile(path.join(webDirectory, 'diagrams', `${diagram.name}.mmd`), diagram.code)
      ]);
      entry.diagrams.push({name:diagram.name, title:diagram.title, type:diagram.type, sourceHash:diagram.sourceHash, svgHash:digest(svg), svg:`/evolution/diagrams/${diagram.name}.svg`, mermaid:`/evolution/diagrams/${diagram.name}.mmd`});
      console.log('Rendered', diagram.name); count++;
    }
    catalog.stages.push(entry);
  }
  // GENERATION 4: Publish one catalog consumed by the web timeline and completeness checker
  await fs.writeFile(path.join(webDirectory, 'catalog.json'), JSON.stringify(catalog, null, 2) + '\n');
  // OFFLINE GALLERY: Embed rendered SVGs and escaped captions into one self-contained document
  const gallery = `<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Elevator Lab · System evolution</title>
<style>*{box-sizing:border-box}body{margin:0;background:#f3f7f8;color:#153e48;font-family:'Segoe UI',Arial,sans-serif}header,main,footer{max-width:1500px;margin:auto;padding:32px 42px}header{display:flex;justify-content:space-between;align-items:center;border-bottom:1px solid #dfe6d9;gap:24px}header img{width:110px}h1{font:700 40px Segoe UI,Arial,sans-serif;margin:12px 0}header p,section>p{color:#788775;line-height:1.7;font-size:14px}.eyebrow{font-size:11px;letter-spacing:2px;color:#6f835f}nav{display:flex;gap:10px;flex-wrap:wrap;margin-top:22px}a{color:#073e51}nav a{background:#e8f3f4;padding:10px 14px;border-radius:6px;text-decoration:none;font-size:12px}h2{font-size:25px;font-weight:400;margin:24px 0 8px}.diagram{background:white;border:1px solid #dfe6d9;border-radius:8px;margin:24px 0;padding:25px;break-inside:avoid}.diagram h3{font-size:17px;font-weight:500;margin:0 0 24px}.diagram svg{display:block;width:100%;height:auto;margin:auto}.links{border-top:1px solid #e5e9df;padding-top:16px;margin-top:20px;font-size:12px}.links a{margin-right:20px}section{scroll-margin-top:20px}footer{font-size:12px;color:#788775}@media(max-width:700px){header,main,footer{padding:20px}h1{font-size:30px}header img{width:75px}.diagram{padding:12px}}@media print{body{background:white}nav,.links{display:none}.diagram{border:0;padding:0;break-before:page}.diagram svg{max-height:230mm}}</style></head>
<body><header><div><div class="eyebrow">SYSTEM EVOLUTION / ${stages.length} STAGES / ${count} DIAGRAMS</div><h1>System architecture and evolution.</h1><p>Source modules, dependencies and interaction sequences at every implementation stage.<br>The elevator runtime and database remain in C.</p><nav>${stages.map(s => `<a href="#${s.slug}">${s.id} · ${html(s.title)}</a>`).join('')}</nav></div><img src="../../web/assets/metriczone-logo.png" alt="Metriczone"></header>
<main>${stages.map(stage => `<section id="${stage.slug}"><h2>${stage.id} · ${html(stage.title)}</h2><p>${html(stage.summary)}</p><p><a href="${stage.file}">Stage notes and Mermaid sources</a> · Recorded ${stage.date}</p>${stage.diagrams.map(d => `<article class="diagram"><h3>${html(d.title)}</h3>${d.svgContent}<div class="links"><a href="diagrams/${d.name}.svg" download>Download SVG</a><a href="diagrams/${d.name}.mmd" download>Editable Mermaid source</a></div></article>`).join('')}</section>`).join('')}</main><footer>Metriczone · Elevator Lab · Generated from stages.json and numbered stage snapshots. All diagrams work offline.</footer></body></html>`;
  await fs.writeFile(path.join(root, 'docs/evolution/diagrams.html'), gallery);
  await page.goto(new URL('../docs/evolution/diagrams.html', import.meta.url).href);
  await page.screenshot({path:path.join(root, 'build/diagram-gallery.png'), fullPage:true});
  console.log(`Exported ${stages.length} stages and ${count} diagrams to the offline gallery and web interface.`);
// CLEANUP: Always close the rendering browser, including after a failed diagram
} finally { await browser.close(); }
