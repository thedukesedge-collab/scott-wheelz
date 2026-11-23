// SPDX-License-Identifier: GPL-3.0-or-later
// SEARCH FOR BOARD window (Neon Future). Scans like VESC Tool: Bluetooth LE
// devices advertising the VESC Express UART service (6e400001-...); SHOW ALL
// lists every named device, for boards whose advertising leaves the service out.
import { BleClient } from '@capacitor-community/bluetooth-le';

const NAMES = 'fpNames';  // your own names for boards, by device id
const SCAN_MS = 10000;

const css = `
#fpScan { position: fixed; inset: 0; z-index: 80; background: rgba(4,2,10,.78); display: flex; align-items: center; justify-content: center; }
#fpScan .box { width: min(92vw, 400px); max-height: 78vh; display: flex; flex-direction: column; background: #0b0718;
  border: 2px solid #b14cff; border-radius: 14px; box-shadow: 0 0 22px rgba(177,76,255,.55); padding: 14px; color: #ead6ff;
  font-family: -apple-system, system-ui, sans-serif; }
#fpScan h3 { margin: 0 0 4px; font: 700 15px Orbitron, monospace; letter-spacing: 2px; color: #39ff7a; text-shadow: 0 0 8px rgba(57,255,122,.7); text-align: center; }
#fpScan .sub { text-align: center; font: 700 10px Orbitron, monospace; letter-spacing: 1px; color: #b98cff; margin-bottom: 10px; min-height: 14px; }
#fpScan .list { overflow: auto; flex: 1; min-height: 120px; }
#fpScan .dev { display: flex; align-items: center; gap: 10px; padding: 11px 12px; margin: 7px 0; border: 1.5px solid #7a3fc0;
  border-radius: 10px; background: rgba(177,76,255,.08); }
#fpScan .dev:active { background: rgba(57,255,122,.18); border-color: #39ff7a; }
#fpScan .dev .n { flex: 1; font: 700 13px Orbitron, monospace; color: #39ff7a; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
#fpScan .dev small { display: block; font: 600 9px -apple-system, system-ui, sans-serif; color: #b98cff; letter-spacing: .5px; margin-top: 2px; }
#fpScan .dev .tag { font: 700 8px Orbitron, monospace; color: #0b0718; background: #39ff7a; border-radius: 6px; padding: 2px 5px; margin-left: 6px; }
#fpScan .bars { display: flex; align-items: flex-end; gap: 2px; height: 16px; }
#fpScan .bars i { width: 4px; background: #3a2566; border-radius: 1px; }
#fpScan .bars i.on { background: #39ff7a; box-shadow: 0 0 5px #39ff7a; }
#fpScan .ren { font: 700 9px Orbitron, monospace; color: #b98cff; border: 1px solid #7a3fc0; border-radius: 999px; padding: 4px 8px; }
#fpScan .empty { text-align: center; color: #7d68ab; font-size: 12px; padding: 26px 8px; line-height: 1.5; }
#fpScan .row { display: flex; gap: 8px; margin-top: 10px; }
#fpScan .btn { flex: 1; text-align: center; padding: 10px 6px; border-radius: 999px; font: 700 11px Orbitron, monospace; letter-spacing: 1px;
  border: 2px solid #b14cff; color: #ead6ff; }
#fpScan .btn.go { border-color: #39ff7a; color: #39ff7a; box-shadow: 0 0 10px rgba(57,255,122,.35); }
#fpScan .btn.on { background: rgba(57,255,122,.15); }
#fpScan .radar { width: 10px; height: 10px; border-radius: 50%; background: #39ff7a; display: inline-block; margin-right: 6px;
  box-shadow: 0 0 0 0 rgba(57,255,122,.7); animation: fpPing 1.2s infinite; vertical-align: middle; }
@keyframes fpPing { to { box-shadow: 0 0 0 12px rgba(57,255,122,0); } }`;

function names() { try { return JSON.parse(localStorage.getItem(NAMES) || '{}'); } catch (e) { return {}; } }
export function boardName(id, fallback) { return names()[id] || fallback || 'VESC BOARD'; }

function bars(rssi) {
  const n = rssi >= -60 ? 4 : rssi >= -70 ? 3 : rssi >= -80 ? 2 : rssi >= -90 ? 1 : 0;
  return '<div class="bars">' + [6, 9, 12, 16].map((h, i) => '<i class="' + (i < n ? 'on' : '') + '" style="height:' + h + 'px"></i>').join('') + '</div>';
}

// resolves { deviceId, name } or null (closed)
export function searchBoard({ lastId, toast }) {
  return new Promise(resolve => {
    if (!document.getElementById('fpScanCss')) {
      const st = document.createElement('style'); st.id = 'fpScanCss'; st.textContent = css; document.head.appendChild(st);
    }
    const wrap = document.createElement('div');
    wrap.id = 'fpScan';
    wrap.innerHTML = '<div class="box"><h3>SEARCH FOR BOARD</h3><div class="sub" id="fsSub"></div><div class="list" id="fsList"></div>'
      + '<div class="row"><div class="btn" id="fsAll">SHOW ALL</div><div class="btn go" id="fsAgain">SCAN AGAIN</div></div>'
      + '<div class="row"><div class="btn" id="fsClose">CLOSE</div></div></div>';
    document.body.appendChild(wrap);
    const $ = id => wrap.querySelector('#' + id);
    const found = new Map();  // id -> { name, rssi, nus, seen }
    let all = false, scanning = false, timer = null, done = false;

    const draw = () => {
      const list = [...found.entries()].filter(([, d]) => all ? d.name || d.nus : d.nus)
        .sort((a, b) => (b[0] === lastId) - (a[0] === lastId) || b[1].rssi - a[1].rssi);
      $('fsSub').innerHTML = scanning ? '<span class="radar"></span>SCANNING…' : (list.length ? 'TAP YOUR BOARD' : 'SCAN FINISHED');
      $('fsAll').classList.toggle('on', all);
      if (!list.length) {
        $('fsList').innerHTML = '<div class="empty">' + (scanning ? 'Looking for VESC Express boards…' :
          'No board found.<br>Power the board on, keep the phone close, and make sure VESC Tool isn\'t connected to it.') + '</div>';
        return;
      }
      const nm = names();
      $('fsList').innerHTML = list.map(([id, d]) => '<div class="dev" data-id="' + id + '">' + bars(d.rssi)
        + '<div class="n">' + escape(nm[id] || d.name || 'VESC BOARD') + (id === lastId ? '<span class="tag">LAST USED</span>' : '')
        + '<small>' + escape(nm[id] && d.name ? d.name + ' · ' : '') + (d.nus ? 'VESC EXPRESS' : 'OTHER DEVICE') + ' · ' + d.rssi + ' dBm</small></div>'
        + '<span class="ren" data-ren="' + id + '">RENAME</span></div>').join('');
      wrap.querySelectorAll('[data-id]').forEach(x => x.onclick = e => {
        if (e.target.dataset.ren) return;
        const id = x.dataset.id;
        finish({ deviceId: id, name: names()[id] || found.get(id).name || 'VESC BOARD' });
      });
      wrap.querySelectorAll('[data-ren]').forEach(x => x.onclick = () => {
        const id = x.dataset.ren, nm2 = names();
        const v = prompt('Board name', nm2[id] || found.get(id).name || '');
        if (v === null) return;
        if (v.trim()) nm2[id] = v.trim().slice(0, 24); else delete nm2[id];
        try { localStorage.setItem(NAMES, JSON.stringify(nm2)); } catch (e2) {}
        draw();
      });
    };
    const escape = s => String(s).replace(/[&<>"]/g, c => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]));

    const stop = async () => {
      clearTimeout(timer);
      if (scanning) { scanning = false; try { await BleClient.stopLEScan(); } catch (e) {} }
    };
    const scan = async () => {
      await stop();
      try {
        await BleClient.initialize();
        scanning = true; draw();
        // no service filter, so SHOW ALL works; VESC Express boards are picked out by their UART service
        await BleClient.requestLEScan({ allowDuplicates: true }, r => {
          const id = r.device.deviceId;
          const nus = (r.uuids || []).some(u => u.toLowerCase().startsWith('6e400001'));
          const old = found.get(id);
          found.set(id, { name: r.localName || r.device.name || (old && old.name) || '', rssi: r.rssi ?? -99, nus: nus || !!(old && old.nus) });
          drawSoon();
        });
        timer = setTimeout(() => { stop().then(draw); }, SCAN_MS);
      } catch (e) {
        scanning = false; draw();
        toast('Bluetooth: ' + (e && e.message || e));
      }
    };
    let pending = false;
    const drawSoon = () => { if (!pending) { pending = true; setTimeout(() => { pending = false; if (!done) draw(); }, 250); } };
    const finish = async v => { if (done) return; done = true; await stop(); wrap.remove(); resolve(v); };

    $('fsAll').onclick = () => { all = !all; draw(); };
    $('fsAgain').onclick = () => { found.clear(); scan(); };
    $('fsClose').onclick = () => finish(null);
    wrap.onclick = e => { if (e.target === wrap) finish(null); };
    draw();
    scan();
  });
}
