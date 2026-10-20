/****************************************
*
* Elevator Lab -- Architecture and Sequence History Viewer
*
* SPDX-License-Identifier: MIT
*
******************************************/

'use strict';
(() => {
  /**
   * Look up a history viewer or diagram dialog element
   */
  const el = id => document.getElementById(id);
  /**
   * Escape stage text before including it in generated timeline or diagram markup
   */
  const escape = text => String(text).replace(/[&<>"']/g, c => ({'&':'&amp;', '<':'&lt;', '>':'&gt;', '"':'&quot;', "'":'&#39;'}[c]));
  let catalog = null, selected = null, filter = 'all', activeDiagram = null, zoom = 1, fittedWidth = 0;
  /**
   * Reapply the shared button policy after inserting read-only diagram controls
   */
  function localControls() { window.ElevatorLab?.updateControls(); }
  /**
   * Show or clear an evolution catalog error without changing the simulation controls
   */
  function error(message) { el('evolution-error').textContent = message; el('evolution-error').hidden = !message; }
  /**
   * Validate the catalog path against the same constrained documentation naming scheme
   * Accept only the expected directory and extension for each asset kind
   */
  function validAsset(value, kind) {
    const prefix = kind === 'notes' ? '/evolution/stages/' : '/evolution/diagrams/';
    const suffix = kind === 'notes' ? '.md' : kind === 'svg' ? '.svg' : '.mmd';
    return typeof value === 'string' && value.startsWith(prefix) && value.endsWith(suffix) && /^[a-z0-9-]+$/.test(value.slice(prefix.length, -suffix.length));
  }
  /**
   * Select an existing snapshot, update its metadata and render the active diagram filter
   * Optionally replace the bookmark URL without reloading the live simulator
   */
  function selectStage(slug, updateUrl = false) {
    const stage = catalog.stages.find(s => s.slug === slug);
    if (!stage) return;
    selected = stage;
    document.querySelectorAll('.evolution-step').forEach(button => {
      if (button.dataset.stage === slug) button.setAttribute('aria-current', 'step'); else button.removeAttribute('aria-current');
    });
    el('evolution-stage-label').textContent = `STAGE ${stage.id}${stage === catalog.stages.at(-1) ? ' · LATEST' : ''}`;
    el('evolution-stage-title').textContent = stage.title;
    el('evolution-stage-summary').textContent = stage.summary;
    el('evolution-stage-date').textContent = stage.date;
    el('evolution-stage-notes').href = stage.notes;
    if (updateUrl) {
      const url = new URL(location.href); url.searchParams.set('stage', slug); url.hash = 'evolution';
      history.replaceState(null, '', url);
    }
    renderDiagrams();
  }
  /**
   * Render only the selected stage diagrams matching the active kind
   * Use escaped captions and validated local links for previews and downloads
   */
  function renderDiagrams() {
    const diagrams = selected.diagrams.filter(diagram => filter === 'all' || diagram.type === filter);
    el('evolution-diagrams').innerHTML = diagrams.map(diagram => `<article class="evolution-diagram" data-diagram-type="${diagram.type}"><div class="evolution-diagram-heading"><h4>${escape(diagram.title)}</h4><span class="evolution-kind ${diagram.type}">${diagram.type === 'architecture' ? 'Architecture' : 'Sequence'}</span></div><button type="button" class="evolution-preview" data-local-control="true" data-open-diagram="${escape(diagram.name)}" aria-label="Enlarge ${escape(diagram.title)}"><img src="${diagram.svg}" alt="${escape(selected.title)}: ${escape(diagram.title)}"></button><div class="evolution-diagram-footer"><button type="button" data-local-control="true" data-open-diagram="${escape(diagram.name)}">Enlarge & zoom ↗</button><div><a href="${diagram.svg}" download>SVG ↓</a><a href="${diagram.mermaid}" download>Mermaid source ↓</a></div></div></article>`).join('');
    document.querySelectorAll('[data-diagram-filter]').forEach(button => button.setAttribute('aria-pressed', String(button.dataset.diagramFilter === filter)));
    localControls();
  }
  /**
   * Scale the full-size image relative to its fitted width
   * Clamp zoom to 15-400 percent and wait until image dimensions are known
   */
  function setZoom(next) {
    const image = el('diagram-full-image');
    if (!image.naturalWidth) return;
    zoom = Math.max(0.15, Math.min(4, next));
    image.style.width = `${Math.round(fittedWidth * zoom)}px`;
    el('diagram-zoom-level').textContent = `${Math.round(zoom * 100)}%`;
  }
  /**
   * Fit a loaded diagram to the current scroll viewport and reset zoom to 100 percent
   */
  function fit() { const image = el('diagram-full-image'); if (image.naturalWidth) { fittedWidth = Math.max(160, el('diagram-viewport').clientWidth - 45); setZoom(1); } }
  /**
   * Open a diagram belonging to the selected stage in the native modal
   * Wait for image loading before calculating fit and reset the diagram scroll position
   */
  function openDiagram(name) {
    activeDiagram = selected.diagrams.find(d => d.name === name);
    if (!activeDiagram) return;
    el('diagram-dialog-title').textContent = activeDiagram.title;
    el('diagram-dialog-stage').textContent = `STAGE ${selected.id} · ${selected.title.toUpperCase()}`;
    const image = el('diagram-full-image'); image.alt = `${selected.title}: ${activeDiagram.title}`;
    image.style.removeProperty('width'); image.onload = fit;
    el('diagram-dialog').showModal(); image.src = activeDiagram.svg;
    if (image.complete) fit();
    el('diagram-viewport').scrollTo(0, 0);
  }
  // HISTORY INPUT: Delegate stage selection, type filtering and enlarged diagram actions
  document.addEventListener('click', event => {
    const button = event.target.closest('button');
    if (!button || button.disabled) return;
    if (button.dataset.stage) selectStage(button.dataset.stage, true);
    else if (button.dataset.diagramFilter && selected) { filter = button.dataset.diagramFilter; renderDiagrams(); }
    else if (button.dataset.openDiagram) openDiagram(button.dataset.openDiagram);
  });
  el('close-diagram').addEventListener('click', () => el('diagram-dialog').close());
  el('diagram-zoom-in').addEventListener('click', () => setZoom(zoom * 1.25));
  el('diagram-zoom-out').addEventListener('click', () => setZoom(zoom / 1.25));
  el('diagram-fit').addEventListener('click', fit);
  // BOOKMARKS: Restore a valid requested stage when browser history changes
  window.addEventListener('popstate', () => { if (catalog) selectStage(new URLSearchParams(location.search).get('stage') || catalog.stages.at(-1).slug); });
  // CATALOG LOAD: Validate generated local links before rendering any stage previews
  (async () => {
    try {
      const response = await fetch('/evolution/catalog.json', {signal:AbortSignal.timeout(10000)});
      if (!response.ok) throw new Error('System evolution could not be loaded. Refresh to retry.');
      catalog = await response.json();
      if (catalog.schemaVersion !== 1 || !catalog.stages?.length) throw new Error('The evolution catalog is unavailable.');
      for (const stage of catalog.stages) {
        if (!/^[a-z0-9-]+$/.test(stage.slug) || !validAsset(stage.notes, 'notes') || !stage.diagrams?.length || stage.diagrams.some(d => !['architecture','sequence'].includes(d.type) || !validAsset(d.svg, 'svg') || !validAsset(d.mermaid, 'mermaid'))) throw new Error('An evolution stage has invalid diagram links.');
      }
      const count = catalog.stages.reduce((total, stage) => total + stage.diagrams.length, 0);
      el('evolution-count').textContent = `${catalog.stages.length} stages · ${count} diagrams`;
      el('evolution-stages').innerHTML = catalog.stages.map((stage, index) => `<li><button type="button" class="evolution-step" data-local-control="true" data-stage="${stage.slug}"><span class="evolution-step-number">${escape(stage.id)}</span><span class="evolution-step-text">${escape(stage.title)}<small>${stage.diagrams.length} diagrams${index === catalog.stages.length - 1 ? ' · Latest' : ''}</small></span></button></li>`).join('');
      // INITIAL STAGE: Honor a known bookmark or show the latest documented implementation
      const requested = new URLSearchParams(location.search).get('stage');
      selectStage(catalog.stages.some(stage => stage.slug === requested) ? requested : catalog.stages.at(-1).slug);
      error('');
    } catch (problem) { error(problem.message); el('evolution-count').textContent = 'Timeline unavailable'; }
  })();
})();
