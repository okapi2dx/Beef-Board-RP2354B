(() => {
  'use strict';
  const storageKey = 'beef-rp-emulator-settings-v1';
  const defaults = () => {
    const c = new Uint8Array(139);
    c[0] = 42; c[4] = 8; c[30] = 10; c[61] = 16; c[63] = 0x28;
    c[76] = 4; c[78] = 60; c[79] = 1; c[80] = 24; c[88] = 100; c[138] = 16;
    for (let i = 6; i <= 27; i += 3) { c[i + 1] = 255; c[i + 2] = 255; }
    c.set([0x16,0x07,0x09,0x2c,0x0d,0x0e,0x0f,0x1e,0x1f,0x20,0x21,0x51,0x52],35);
    c.set([0x07,0x09,0x0d,0x0e,0x06,0x10],55);
    c.set([0,255,255],81);
    for (let i = 0; i < 11; i++) { c[102 + i] = 100; c[127 + i] = i; }
    return c;
  };
  const valid = c => c.length === 139 && c[0] === 42 && c[2] <= 9 && c[4] <= 8 &&
    c[30] > 0 && c[80] > 0 && c[138] > 0 && c[61] <= 16 && c[61] % 2 === 0;
  let config = defaults(), legacy = true;
  try {
    const saved = JSON.parse(localStorage.getItem(storageKey));
    if (saved && Array.isArray(saved.config) && saved.config.every(v => Number.isInteger(v) && v >= 0 && v <= 255)) {
      const bytes = Uint8Array.from(saved.config);
      if (valid(bytes)) { config = bytes; legacy = saved.legacy !== false; }
    }
  } catch { /* Invalid local settings use defaults. */ }
  localStorage.setItem('beef-language', 'ja');
  const state = { attached: true, buttons: 0, axis: 128, direction: 0, transitions: 0, interval: 0,
    lastMotion: 0, running: false, speed: 5, sequence: 0, commands: [], edges: new Uint16Array(7),
    events: new Uint8Array(18).fill(255), lastEdge: new Float64Array(11), releaseAt: new Float64Array(11) };
  let device, panel, panelOpen = false;
  const save = () => localStorage.setItem(storageKey, JSON.stringify({ config: [...config], legacy }));
  const log = message => { state.commands.unshift(message); state.commands.length = Math.min(state.commands.length, 8); refreshPanel(); };
  const view = bytes => new DataView(bytes.slice().buffer);
  const hid = new EventTarget();
  function connectionEvent(type, old) {
    const event = new Event(type); Object.defineProperty(event, 'device', { value: old }); hid.dispatchEvent(event);
  }
  function button(index, pressed) {
    const bit = 1 << index;
    if (Boolean(state.buttons & bit) === pressed) return;
    const now = performance.now();
    state.buttons = pressed ? state.buttons | bit : state.buttons & ~bit;
    state.releaseAt[index] = pressed ? 0 : now;
    state.sequence = (state.sequence + 1) & 0xffff;
    if (index < 7) state.edges[index]++;
    const slot = (state.sequence % 6) * 3;
    const elapsed = Math.min(10000, Math.floor((now - state.lastEdge[index]) * 10));
    state.events[slot] = (pressed ? 0x80 : 0x20) | 0x10 | index;
    state.events[slot + 1] = elapsed & 255;
    state.events[slot + 2] = ((elapsed | 0x4000) >> 8) & 255;
    state.lastEdge[index] = now;
    refreshPanel(); emit();
  }
  function axis(value) {
    const position = ((Math.round(value) % 256) + 256) % 256;
    let delta = position - state.axis;
    if (delta > 127) delta -= 256;
    if (delta < -128) delta += 256;
    if (delta) {
      state.direction = Math.sign(delta); state.transitions += Math.abs(delta);
      state.interval = Math.max(1, Math.round(20 / Math.abs(delta))); state.lastMotion = performance.now();
    }
    state.axis = position; refreshPanel(); emit();
  }
  function diagnostics() {
    const bytes = new Uint8Array(60), dv = new DataView(bytes.buffer), now = performance.now();
    dv.setUint16(0, state.buttons, true); bytes[2] = [0,1,3,2][state.axis % 4];
    bytes[3] = bytes[4] = bytes[5] = state.axis;
    dv.setInt8(6, now - state.lastMotion < 150 ? state.direction : 0);
    dv.setUint32(7, state.transitions >>> 0, true); dv.setUint16(11, state.interval, true);
    for (let i = 0; i < 11; i++) {
      const active = Boolean(state.buttons & (1 << i));
      const brightness = config[90] ? config[102 + i] : config[88];
      const inverted = Boolean(config[90] ? config[113 + i] : config[89]);
      const fade = config[90] ? config[91 + i] : (config[125] | config[126] << 8);
      let level = active ? 1 : fade && state.releaseAt[i] ? Math.max(0, 1 - (now - state.releaseAt[i]) / fade) : 0;
      if (inverted) level = 1 - level;
      bytes[13 + i] = config[5] ? 0 : Math.round(level * brightness * 255 / 100);
    }
    // Physical button edges and chatter detections are separate report fields.
    dv.setUint16(38, state.buttons, true); dv.setUint16(40, state.sequence, true); bytes.set(state.events, 42);
    return bytes;
  }
  function chatterCounts() {
    const bytes = new Uint8Array(28), dv = new DataView(bytes.buffer);
    for (let i = 0; i < 7; i++) dv.setUint16(i * 2, state.edges[i], true);
    return bytes;
  }
  function emit() {
    if (!state.attached || !device?.opened) return;
    for (const [reportId, bytes] of [[5, diagnostics()], [7, chatterCounts()]]) {
      const event = new Event('inputreport');
      Object.defineProperties(event, { device: { value: device }, reportId: { value: reportId }, data: { value: view(bytes) } });
      device.dispatchEvent(event);
    }
  }
  function makeDevice() {
    const result = new EventTarget(), mode = config[31];
    Object.assign(result, { collections: [{ usagePage: 0xffeb, usage: 1 }],
      productName: mode === 1 ? 'beatmania IIDX controller entry model' : mode === 2 ? 'beatmania IIDX controller premium model' : 'BEEF BOARD RP2354B',
      vendorId: mode ? 0x1ccf : 0xfeed, productId: mode === 1 ? 0x1018 : mode === 2 ? 0x8048 : 3, opened: false,
      open: async () => { if (!state.attached || result !== device) throw new Error('疑似USBが切断されています'); result.opened = true; },
      close: async () => { result.opened = false; },
      receiveFeatureReport: async id => {
        if (!result.opened || result !== device || !state.attached) throw new Error('疑似USBが切断されています');
        if (id === 1) return view(config);
        if (id === 2) return view(Uint8Array.of(0));
        if (id === 3) return view(Uint8Array.of(0,0,0,0,1,0,0));
        if (id === 4) return view(diagnostics());
        if (id === 6) return view(Uint8Array.of(66,69,69,70,1,3,2,0,legacy ? 255 : 127,0,4));
        if (id === 7) return view(chatterCounts());
        throw new Error('未対応の疑似レポート');
      },
      sendFeatureReport: async (id, data) => {
        if (!result.opened || result !== device || !state.attached) throw new Error('疑似USBが切断されています');
        const bytes = new Uint8Array(data.buffer, data.byteOffset, data.byteLength);
        if (id === 1) {
          if (!valid(bytes)) throw new Error('不正な疑似設定パケット');
          config = bytes.slice(); save(); refreshPanel(); return;
        }
        if (id !== 2 || bytes.length !== 1) throw new Error('不正な疑似コマンド');
        const command = bytes[0];
        if (command === 1) { detach(); log('BOOTSEL切替を受信（ドライブは作成しません）'); }
        else if (command === 2) { config = defaults(); legacy = true; save(); reboot(); }
        else if (command === 3) reboot();
        else if (command === 4 || command === 5) { legacy = command === 4; save(); log(`旧Phoenixwan ${legacy ? 'ON' : 'OFF'}`); }
        else if (command >= 6 && command <= 17) {
          const bar = command >= 12, adjustment = command - (bar ? 12 : 6), channel = Math.floor(adjustment / 2);
          const supported = bar || Boolean(([7,1,6,6,6,6,3,3][config[2]] || 0) & (1 << channel));
          log(`${bar ? 'センターバー' : 'ターンテーブル'} ${['色相','彩度','明るさ'][channel]} ${adjustment % 2 ? '＋' : '−'} / HID ${command}${legacy && supported ? '' : '（無効）'}`);
        }
      }
    });
    return result;
  }
  function detach() {
    const old = device; state.attached = false; state.running = false;
    connectionEvent('disconnect', old); refreshPanel();
  }
  function attach(auto = true) {
    state.attached = true; device = makeDevice(); connectionEvent('connect', device); refreshPanel();
    if (auto) setTimeout(autoConnect, 100);
  }
  function reboot() {
    detach(); log('疑似基板を再起動'); setTimeout(() => attach(false), 400);
  }
  device = makeDevice();
  hid.requestDevice = async () => state.attached ? [device] : [];
  hid.getDevices = async () => state.attached ? [device] : [];
  Object.defineProperty(navigator, 'hid', { configurable: true, value: hid });
  const usb = new EventTarget(); usb.getDevices = async () => [];
  usb.requestDevice = async () => { throw new Error('エミュレーターでは実機への書き込みはできません'); };
  Object.defineProperty(navigator, 'usb', { configurable: true, value: usb });
  window.rpEmulator = { state, hid, get config() { return config.slice(); }, get legacy() { return legacy; },
    get device() { return device; }, button, axis, attach, detach, diagnostics, chatterCounts };
  function autoConnect() {
    if (!state.attached || device.opened) return;
    const connect = [...document.querySelectorAll('#app button')].find(b => b.textContent.trim() === '接続' || b.textContent.trim() === '再接続');
    if (connect && !connect.disabled) connect.click();
  }
  function refreshPanel() {
    if (!panel) return;
    panel.querySelector('#emu-usb').textContent = state.attached ? '疑似USBを切断' : '疑似USBを接続';
    panel.querySelector('#emu-run').textContent = state.running ? '回転を停止' : '回転デモ';
    panel.querySelector('#emu-axis').value = state.axis;
    panel.querySelector('#emu-axis-value').textContent = state.axis;
    panel.querySelector('#emu-state').textContent = `${state.attached ? '疑似USB接続' : '疑似USB切断'} / 旧Phoenixwan ${legacy ? 'ON' : 'OFF'} / ${config[61]}灯`;
    for (const b of panel.querySelectorAll('[data-button]')) { const active = Boolean(state.buttons & 1 << Number(b.dataset.button)); b.classList.toggle('active', active); b.setAttribute('aria-pressed', String(active)); }
    panel.querySelector('#emu-log').textContent = state.commands.join('\n') || '旧Phoenixwanの操作をすると、ここに表示します。';
  }
  document.addEventListener('DOMContentLoaded', () => {
    const style = document.createElement('style');
    style.textContent = `#app{height:calc(100vh - 34px)!important;margin-top:34px}#emu-banner{position:fixed;top:0;left:0;right:0;height:34px;z-index:99999;display:flex;align-items:center;justify-content:space-between;padding:0 16px;background:#174b55;color:#fff;font:13px system-ui}#emu-banner button,#emu-panel button{border:1px solid #687788;border-radius:6px;background:#263440;color:#fff;padding:6px 10px;cursor:pointer}#emu-panel{position:fixed;z-index:99999;right:18px;bottom:18px;width:330px;max-height:calc(100vh - 90px);overflow:auto;padding:16px;border:1px solid #5d8490;border-radius:12px;background:#17212d;color:#fff;box-shadow:0 8px 36px #0009;font:13px system-ui}#emu-panel[hidden]{display:none}#emu-panel h2{margin:0 0 8px;font-size:16px}#emu-panel p{margin:6px 0 12px;color:#b6c7d6}#emu-panel .buttons{display:grid;grid-template-columns:repeat(4,1fr);gap:6px;margin-bottom:12px}#emu-panel button.active{background:#087f70;border-color:#4dd8bd}#emu-panel .row{display:flex;align-items:center;justify-content:space-between;gap:8px;margin:12px 0}#emu-panel input{width:160px}#emu-panel pre{font:12px system-ui;white-space:pre-wrap;min-height:44px;background:#101822;padding:9px;border-radius:6px}#emu-panel small{display:block;color:#bdd3e1;line-height:1.5}`;
    document.head.append(style);
    const banner = document.createElement('div'); banner.id = 'emu-banner';
    banner.innerHTML = '<span>RP2354B 疑似接続エミュレーター V1.00 · 実機通信なし</span><button id="emu-open">入力パネル</button>';
    document.body.append(banner);
    panel = document.createElement('aside'); panel.id = 'emu-panel'; panel.hidden = true;
    panel.innerHTML = '<h2>RP2354B 入力パネル</h2><p id="emu-state"></p><div class="buttons"></div><label class="row">ターンテーブル<input id="emu-axis" type="range" min="0" max="255" value="128"><b id="emu-axis-value">128</b></label><label class="row">回転方向・速度<input id="emu-speed" type="range" min="-20" max="20" value="5"><b id="emu-speed-value">5</b></label><div class="row"><button id="emu-run">回転デモ</button><button id="emu-release">全ボタンを離す</button></div><div class="row"><button id="emu-usb">疑似USBを切断</button><button id="emu-reset">設定を初期化</button></div><h2>コマンド履歴（模擬）</h2><pre id="emu-log"></pre><small>設定はこのエミュレーター内に保存します。実機の入力遅延・デバウンス・UART発光は再現しません。</small>';
    document.body.append(panel);
    document.querySelector('#emu-open').onclick = () => { panelOpen = !panelOpen; panel.hidden = !panelOpen; };
    for (let i = 0; i < 11; i++) {
      const b = document.createElement('button'); b.textContent = i < 7 ? `B${i + 1}` : `E${i - 6}`; b.dataset.button = i;
      b.onclick = () => button(i, !(state.buttons & 1 << i)); panel.querySelector('.buttons').append(b);
    }
    panel.querySelector('#emu-axis').oninput = event => { state.running = false; axis(Number(event.target.value)); };
    panel.querySelector('#emu-speed').oninput = event => { state.speed = Number(event.target.value); panel.querySelector('#emu-speed-value').textContent = state.speed; };
    panel.querySelector('#emu-run').onclick = () => { state.running = !state.running; refreshPanel(); };
    panel.querySelector('#emu-release').onclick = () => { for (let i = 0; i < 11; i++) button(i, false); };
    panel.querySelector('#emu-usb').onclick = () => state.attached ? detach() : attach();
    panel.querySelector('#emu-reset').onclick = () => { if (confirm('エミュレーターの設定を初期化しますか？')) { config = defaults(); legacy = true; save(); location.reload(); } };
    refreshPanel();
    const startup = setInterval(() => {
      autoConnect();
      const led = [...document.querySelectorAll('#app button')].find(b => b.textContent.trim().startsWith('LED設定'));
      if (led && !led.disabled && device.opened) { led.click(); clearInterval(startup); }
    }, 100);
    setTimeout(() => clearInterval(startup), 10000);
  });
  setInterval(() => { if (state.attached && state.running) axis(state.axis + state.speed); else emit(); }, 20);
})();
