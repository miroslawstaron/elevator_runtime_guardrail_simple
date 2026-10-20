// Optional documentation export tools:
// npm install --prefix build/browser-tools playwright --no-save
// npm install --prefix build/diagram-tools mermaid --no-save
// node tools/render-diagrams.mjs
import fs from 'node:fs/promises';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {createRequire} from 'node:module';
const require = createRequire(import.meta.url);
const {chromium} = require('../build/browser-tools/node_modules/playwright');
const root = fileURLToPath(new URL('../', import.meta.url));
const destination = path.join(root, 'docs/evolution/diagrams');
await fs.mkdir(destination, {recursive:true});
const browser = await chromium.launch(process.env.EDGE_PATH ? {executablePath:process.env.EDGE_PATH,headless:true} : process.platform === 'win32' ? {channel:'msedge',headless:true} : {headless:true});
const entries = [];
try {
  const page = await browser.newPage({viewport:{width:1600,height:1000}});
  await page.setContent('<!doctype html><html><head></head><body></body></html>');
  await page.addScriptTag({path:path.join(root,'build/diagram-tools/node_modules/mermaid/dist/mermaid.min.js')});
  await page.evaluate(() => mermaid.initialize({startOnLoad:false,htmlLabels:false,theme:'base',securityLevel:'strict',fontFamily:'Segoe UI, Arial, sans-serif',themeVariables:{primaryColor:'#eef4df',primaryTextColor:'#173d45',primaryBorderColor:'#9eaf82',lineColor:'#53778a',secondaryColor:'#eaf1f2',tertiaryColor:'#f9f6ed',mainBkg:'#f2f6e9',nodeBorder:'#a2b28c',clusterBkg:'#f7f8f3',clusterBorder:'#d7dfcd',actorBkg:'#eaf1f2',actorBorder:'#91a5ac',actorTextColor:'#073e51',signalColor:'#456777',signalTextColor:'#173d45',noteBkgColor:'#f7f2dd',noteBorderColor:'#d8cda6',fontSize:'14px'},flowchart:{htmlLabels:false,useMaxWidth:true,wrappingWidth:220},sequence:{useMaxWidth:true,wrap:true,actorMargin:35,messageMargin:26}}));
  for (const [source,phase] of [['01-before.md','before'],['02-after.md','after'],['03-public-access.md','public']]) {
    const text = await fs.readFile(path.join(root,'docs/evolution',source),'utf8');
    const blocks = [...text.matchAll(/## ([^\r\n]+)[\s\S]*?```mermaid\r?\n([\s\S]*?)```/g)];
    for (let i = 0; i < blocks.length; i++) {
      const title = blocks[i][1];
      const code = blocks[i][2].trim() + '\n';
      const name = `${phase}-${String(i+1).padStart(2,'0')}-${i === 0 ? 'architecture' : 'sequence'}`;
      const svg = await page.evaluate(async ({code,id}) => (await mermaid.render(id, code)).svg, {code,id:'diagram'+entries.length});
      await fs.writeFile(path.join(destination,name+'.mmd'),code);
      await fs.writeFile(path.join(destination,name+'.svg'),svg);
      entries.push({title,name,phase,svg});
      console.log('Rendered',name);
    }
  }
  const html = `<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Elevator Lab · Architecture evolution</title><style>*{box-sizing:border-box}body{margin:0;background:#f6f7f3;color:#173d45;font-family:'Segoe UI',Arial,sans-serif}header,main,footer{max-width:1500px;margin:auto;padding:32px 42px}header{display:flex;justify-content:space-between;align-items:center;border-bottom:1px solid #dfe6d9}header img{width:110px}h1{font-family:Georgia,serif;font-weight:400;font-size:40px;margin:12px 0}header p,section>p{color:#788775;line-height:1.7;font-size:14px}.eyebrow{font-size:11px;letter-spacing:2px;color:#6f835f}nav{display:flex;gap:12px;margin-top:22px}a{color:#073e51}nav a{background:#e8efdb;padding:10px 16px;border-radius:6px;text-decoration:none}h2{font-size:25px;font-weight:400;margin:20px 0 8px}.diagram{background:white;border:1px solid #dfe6d9;border-radius:8px;margin:24px 0;padding:25px;page-break-inside:avoid}.diagram h3{font-size:17px;font-weight:500;margin:0 0 24px}.diagram svg{display:block;width:100%;height:auto;margin:auto;max-height:none}.links{border-top:1px solid #e5e9df;padding-top:16px;margin-top:20px;font-size:12px}.links a{margin-right:20px}section{scroll-margin-top:20px}footer{font-size:12px;color:#788775}@media(max-width:700px){header,main,footer{padding:20px}h1{font-size:30px}header img{width:75px}.diagram{padding:12px}}@media print{body{background:white}nav,.links{display:none}.diagram{border:0;padding:0;break-before:page}.diagram svg{max-height:230mm}header{padding-bottom:0}}</style></head><body><header><div><div class="eyebrow">SYSTEM EVOLUTION / 20 OCTOBER 2026</div><h1>From terminal to Elevator Lab.</h1><p>Architecture and interaction sequences before and after the web interface.<br>The original elevator state machine and runtime guardrails remain in C.</p><nav><a href="#before">01 · Before</a><a href="#after">02 · After</a><a href="#public">03 · Internet access</a></nav></div><img src="../../web/assets/agenticai-logo.png" alt="agenticAI"></header><main>${['before','after','public'].map(phase=>`<section id="${phase}"><h2>${phase==='before'?'01 · Before: the terminal simulator':phase==='after'?'02 · After: web interface, C runtime':'03 · Internet access: HTTPS tunnel to port 8099'}</h2><p>${phase==='before'?'Baseline captured before implementation changes.':'Authoritative C clock, passenger dispatch and local HTTP API. Browser controls and visualizes shared state.'}</p>${entries.filter(e=>e.phase===phase).map(e=>`<article class="diagram"><h3>${e.title}</h3>${e.svg}<div class="links"><a href="diagrams/${e.name}.svg">Download SVG</a><a href="diagrams/${e.name}.mmd">Editable Mermaid source</a></div></article>`).join('')}</section>`).join('')}</main><footer>agenticAI · Elevator Lab · Generated from 01-before.md, 02-after.md and 03-public-access.md. All diagrams work offline.</footer></body></html>`;
  await fs.writeFile(path.join(root,'docs/evolution/diagrams.html'),html);
  await page.goto(new URL('../docs/evolution/diagrams.html',import.meta.url).href);
  await page.screenshot({path:path.join(root,'build/diagram-gallery.png'),fullPage:true});
  console.log(`Exported ${entries.length} SVG diagrams, Mermaid sources, and offline gallery.`);
} finally { await browser.close(); }
