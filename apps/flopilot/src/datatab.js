// SPDX-License-Identifier: GPL-3.0-or-later
// DATA lives in the gear icon (Settings › DATA) instead of the swipe-up
// window. The simulator's DATA page (graph / text, drawn by its own code) is
// moved into the settings window while it's open and put back after, with a
// placeholder keeping the swipe-up pages in order.
export function initDataTab() {
  const pg = document.getElementById('pgData');
  const tab = document.querySelector('.tab[data-i="3"]');
  if (!pg || !tab) return;
  tab.style.display = 'none';
  const P = window.__simP ? window.__simP() : null;
  if (P && P.tab === 3) tab.parentElement.querySelector('.tab[data-i="0"]').click();
  const home = pg.parentElement, holder = document.createElement('div');
  holder.className = 'page';  // keeps setTab's page numbering
  home.insertBefore(holder, pg);
  holder.remove();
  let moved = false;
  const back = () => {
    if (!moved) return;
    moved = false;
    holder.replaceWith(pg);
    pg.classList.add('page');
    pg.classList.remove('on');
    pg.style.cssText = '';
  };
  const modal = document.getElementById('modal');
  if (modal) new MutationObserver(() => { if (!modal.classList.contains('on')) back(); }).observe(modal, { attributes: true, attributeFilter: ['class'] });

  function addTab() {
    const box = document.getElementById('modalBox');
    const tabs = box && box.querySelector('.tabs');
    if (!tabs || !box.querySelector('[data-pg="2"]') || box.querySelector('[data-tab="dt"]')) return;
    const t = document.createElement('div');
    t.className = 'pill'; t.dataset.tab = 'dt'; t.textContent = 'DATA';
    tabs.appendChild(t);
    const page = document.createElement('div');
    page.dataset.pg = 'dt';
    page.style.cssText = 'display:none;position:relative;height:52vh';
    box.querySelector('[data-pg="2"]').after(page);
    t.onclick = () => {
      box.querySelectorAll('[data-tab]').forEach(x => x.classList.toggle('on', x === t));
      box.querySelectorAll('[data-pg]').forEach(g => (g.style.display = g === page ? '' : 'none'));
      if (!moved) {
        moved = true;
        pg.replaceWith(holder);
        pg.classList.remove('page');
        pg.classList.add('on');  // the simulator draws the graph / text only while DATA is "on"
        pg.style.cssText = 'position:absolute;inset:0;display:block;overflow-y:auto';
        page.appendChild(pg);
      }
    };
    box.querySelectorAll('[data-tab]').forEach(x => x !== t && x.addEventListener('click', () => { page.style.display = 'none'; t.classList.remove('on'); }));
  }
  const btn = document.getElementById('btnSettings');
  if (btn) btn.addEventListener('click', () => setTimeout(addTab, 0));
}
