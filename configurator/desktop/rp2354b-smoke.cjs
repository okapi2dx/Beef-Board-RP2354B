const { chromium } = require('@playwright/test');
const http = require('node:http');
const fs = require('node:fs');
const path = require('node:path');
const assert = require('node:assert/strict');

(async () => {
  const root = path.resolve('../beef-config/build');
  const server = http.createServer((req, res) => {
    const entry = fs.existsSync(path.join(root, 'index.html')) ? 'index.html' : '404.html';
    const file = path.join(root, req.url === '/' ? entry : req.url.split('?')[0]);
    if (!file.startsWith(root + path.sep) || !fs.existsSync(file)) {
      res.writeHead(404);
      res.end();
      return;
    }
    res.setHeader('Content-Type', file.endsWith('.js') ? 'application/javascript' : file.endsWith('.css') ? 'text/css' : 'text/html');
    res.end(fs.readFileSync(file));
  });
  await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));

  let browser;
  try {
    browser = await chromium.launch({ channel: 'msedge', headless: true });
    const page = await browser.newPage();
    page.setDefaultTimeout(10000);
    const pageErrors = [];
    page.on('pageerror', error => pageErrors.push(String(error)));
    await page.addInitScript(() => {
      localStorage.setItem('beef-language', 'ja');

      const config = new Uint8Array(140);
      config[0] = 1;
      config[1] = 41; config[5] = 8; config[139] = 16;
      config[31] = 2;
      config[79] = 60;
      config[80] = 3;
      config[81] = 24;
      config[89] = 100;
      for (let i = 103; i < 114; i++) config[i] = 100;
      for (let i = 0; i < 11; i++) config[128 + i] = i;

      config[128] = 3; config[131] = 5;
      window.saved = config; window.writes = [];
      window.featureDiagnosticReads = 0;
      window.listenerAttached = false;
      const inputReportListeners = new Set();
      window.edgeSequence = 0;
      window.lastRawButtons = 0;
      window.edgeEvents = new Uint8Array(18).fill(255);
      window.mockEdges = new Uint16Array(7);
      window.mockDetected = new Uint16Array(7);
      window.mockSeen = new Uint8Array(7);
      window.mockPhaseRelease = new Uint8Array(7);
      window.mockPhaseElapsed = new Uint16Array(7);
      const device = {
        collections: [{ usagePage: 0xffeb, usage: 1 }],
        productName: 'BEEF BOARD RP2354B', vendorId: 0xfeed, productId: 3, opened: true,
        open: async () => {}, close: async () => {},
        receiveFeatureReport: async id => {
          if (id === 6) return new DataView(Uint8Array.from([66,69,69,70,1,3,2,0,63,0,4]).buffer);
          if (id === 3) return new DataView(Uint8Array.from([0,0,0,0,0,1,1]).buffer);
          if (id === 4) {
            window.featureDiagnosticReads++;
            const report = new Uint8Array(15);
            report[0] = 4;
            return new DataView(report.buffer);
          }
          return new DataView(config.buffer.slice(1));
        },
        sendFeatureReport: async (id, data) => {
          window.writes.push({id,data:[...data]});
          if (id === 1) config.set(data, 1);
        },
        addEventListener: (type, listener) => {
          if (type === 'inputreport') {
            inputReportListeners.add(listener);
            window.listenerAttached = true;
          }
        },
        removeEventListener: (type, listener) => {
          if (type === 'inputreport') inputReportListeners.delete(listener);
        }
      };
      window.emitCounts = () => {
        // Windows pads the 28-byte report to the longest input report.
        const report = new Uint8Array(60);
        const view = new DataView(report.buffer);
        for (let i = 0; i < 7; i++) {
          view.setUint16(i * 2, window.mockEdges[i], true);
          view.setUint16(14 + i * 2, window.mockDetected[i], true);
        }
        for (const listener of inputReportListeners)
          listener({ device, reportId: 7, data: view });
      };
      window.emitDiagnostics = (buttons, output, chatter = 0, interval = 30, finalButtons = buttons, firmwareDetected) => {
        if (!inputReportListeners.size) return false;
        const report = new Uint8Array(60);
        report[0] = finalButtons & 0xff;
        report[1] = (finalButtons >> 8) & 0xff;
        report[5] = output; report[13] = 37;
        new DataView(report.buffer).setUint16(24, chatter, true);
        new DataView(report.buffer).setUint16(38, buttons, true);
        const changed = window.lastRawButtons ^ buttons;
        for (let i = 0; i < 7; i++) if (changed & (1 << i)) {
          window.mockEdges[i]++;
          window.edgeSequence++;
          const slot = (window.edgeSequence % 6) * 3;
          const pressed = Boolean(buttons & (1 << i));
          const samePhase = Boolean(window.mockSeen[i]) && window.mockPhaseElapsed[i] + interval < 160;
          if (samePhase) window.mockPhaseElapsed[i] += interval;
          else { window.mockPhaseRelease[i] = pressed ? 0 : 1; window.mockPhaseElapsed[i] = 0; }
          const bounce = samePhase && (window.mockPhaseRelease[i] ? pressed : !pressed);
          window.mockSeen[i] = 1;
          window.edgeEvents[slot] = (pressed ? 128 : 0) | (bounce ? 64 : 0) | (window.mockPhaseRelease[i] ? 32 : 0) | (!samePhase ? 16 : 0) | i;
          const encoded = interval | (firmwareDetected === undefined ? 0 : 0x4000 | (firmwareDetected ? 0x8000 : 0));
          new DataView(window.edgeEvents.buffer).setUint16(slot + 1, encoded, true);
        }
        window.lastRawButtons = buttons;
        window.mockDetected[0] = Math.max(window.mockDetected[0], chatter);
        new DataView(report.buffer).setUint16(40, window.edgeSequence, true);
        report.set(window.edgeEvents, 42);
        for (const listener of inputReportListeners) {
          listener({ device, reportId: 5, data: new DataView(report.buffer) });
        }
        window.emitCounts();
        return true;
      };
      Object.defineProperty(navigator, 'hid', { value: {
        requestDevice: async () => [device], getDevices: async () => [device],
        addEventListener() {}, removeEventListener() {}
      }});
      if (!navigator.usb) Object.defineProperty(navigator, 'usb', { value: {} });
    });

    await page.goto(`http://127.0.0.1:${server.address().port}`);
    await page.getByRole('button', { name: '接続', exact: true }).click();
    await page.getByRole('button', { name: /^LED設定/ }).click();
    await page.locator('#tt-leds').fill('255'); await page.locator('#bar-leds').fill('255');
    await page.waitForFunction(() => window.saved[81] === 255 && window.saved[139] === 255);
    await page.getByText('ターンテーブルの発光効果', {exact:true}).locator('..').getByRole('button').click();
    await page.getByRole('option', {name:'呼吸',exact:true}).click();
    await page.waitForFunction(() => window.saved[3] === 7);
    await page.getByRole('button', { name: /^コントローラーモニター/ }).click();
    await page.getByRole('region', { name: 'コントローラーモニター' }).waitFor();

    await page.waitForTimeout(250);
    if (pageErrors.length) throw new Error(`page errors: ${pageErrors.join(' | ')}`);
    await page.waitForTimeout(500);
    const transport = await page.evaluate(() => ({ listenerAttached: window.listenerAttached, featureReads: window.featureDiagnosticReads }));
    assert.equal(transport.listenerAttached, true, `expected v30 interrupt listener, got ${JSON.stringify(transport)}`);
    await page.waitForFunction(() => window.emitDiagnostics(1, 177));
    await page.waitForFunction(() => document.querySelector('[data-testid="monitor-axis-value"]')?.textContent?.includes('177'));
    assert.equal(await page.evaluate(() => window.featureDiagnosticReads), 0,
      'v30 monitor must not poll diagnostics through blocking Feature Reports');
    // The monitor shows physical buttons; remapping affects the HID output.
    await page.getByRole('img',{name:'B1: 押下',exact:true}).waitFor();
    await page.getByRole('img',{name:'B6: 離している',exact:true}).waitFor();
    await page.getByRole('button', { name: /チャタリングテスト/ }).click();
    await page.evaluate(() => window.emitDiagnostics(1, 177));
    await page.locator('#chatter-press-0').fill('1.7');
    await page.waitForFunction(() => window.saved[49] === 17);
    await page.locator('#chatter-press-0').fill('10.1');
    await page.waitForFunction(() => window.saved[49] === 101);
    assert.equal(await page.locator('#chatter-press-0').inputValue(), '10.1');
    assert.ok(await page.locator('#chatter-press-0').evaluate(input => input.getBoundingClientRect().width >= 80));
    assert.equal(await page.locator('#chatter-press-0').getAttribute('max'), '16');
    assert.equal(await page.locator('#chatter-press-0').getAttribute('step'), '0.1');
    await page.waitForTimeout(70);
    const beforeSuppressedBounce = await page.locator('.waveform .wave-transition').count();
    await page.evaluate(() => {
      window.emitDiagnostics(0, 177, 0, 3, 1);
      window.emitDiagnostics(1, 177, 0, 3, 1);
    });
    assert.equal(await page.locator('.waveform .wave-transition').count(), beforeSuppressedBounce,
      'suppressed raw OFF/ON bounce must not interrupt the final output waveform');
    assert.ok(Number(await page.locator('.waveform .pressed-segment').first().getAttribute('width')) > 0);
    assert.equal(await page.locator('.wave-axis').count(), 0);
    assert.equal(await page.getByTestId('wave-history').count(), 1);
    await page.waitForTimeout(1100);
    await page.getByTestId('wave-history').evaluate(input => {
      input.value = '500'; input.dispatchEvent(new Event('input', { bubbles: true }));
    });
    const historicalTransition = await page.locator('.waveform .wave-transition').first().getAttribute('x1');
    await page.waitForTimeout(300);
    assert.equal(await page.locator('.waveform .wave-transition').first().getAttribute('x1'), historicalTransition,
      'historical waveform must stay fixed while a button is held');
    assert.ok(Number(await page.getByTestId('wave-history').inputValue()) >= 700);
    await page.getByTestId('wave-history').evaluate(input => {
      input.value = '0'; input.dispatchEvent(new Event('input', { bubbles: true }));
    });
    assert.equal(await page.getByTestId('wave-history').inputValue(), '0');
    await page.getByRole('button', { name: '解放時', exact: true }).click();
    await page.locator('#chatter-release-0').fill('0.1');
    await page.waitForFunction(() => window.saved[65] === 1);
    await page.getByRole('button', { name: '押下時', exact: true }).click();
    await page.evaluate(() => window.emitDiagnostics(1, 177, 3));
    await page.waitForFunction(() => document.querySelector('.summary.chatter strong')?.textContent === '3');
    assert.match(await page.locator('.slider-row').first().innerText(), /検知数\s*3/);
    await page.evaluate(() => window.emitDiagnostics(0, 177));
    await page.waitForFunction(() => document.querySelector('.waveform .wave-transition'));
    const outputSegment = page.locator('.waveform .wave-segment').first();
    assert.equal(await outputSegment.getAttribute('x'), '48',
      'the final output waveform must begin at the left edge of the time window');
    assert.ok(Number(await outputSegment.getAttribute('width')) > 0,
      'the final output state must extend up to its transition');
    await page.evaluate(() => window.emitDiagnostics(1, 177));
    await page.getByTestId('input-log-entry').first().waitFor();
    assert.match(await page.getByTestId('input-log-entry').first().innerText(), /3\.0\s*ms/);
    assert.ok((await page.getByTestId('input-log-entry').allInnerTexts()).every(text => /押下時/.test(text) && !/解放時/.test(text)),
      'physical OFF/ON bounce in the press phase should be labeled press');
    await page.getByRole('button', { name: '解放時', exact: true }).click();
    await page.locator('#chatter-release-0').fill('1.0');
    await page.getByRole('button', { name: '押下時', exact: true }).click();
    await page.evaluate(() => { window.emitDiagnostics(0, 177, 3, 200); window.emitDiagnostics(1, 177, 3, 5); });
    await page.getByRole('button', { name: '解放時', exact: true }).click();
    await page.waitForFunction(() => [...document.querySelectorAll('.waveform .chatter-marker')].some(marker => marker.getAttribute('y1') === '37'));
    assert.match(await page.getByTestId('input-log-entry').first().innerText(), /解放時[\s\S]*0\.5\s*ms[\s\S]*検知/,
      'physical ON bounce in the release phase should be labeled release');
    assert.ok((await page.getByTestId('input-log-entry').allInnerTexts()).every(text => /解放時/.test(text) && !/押下時/.test(text)),
      'release log must not mix in press labels');
    await page.locator('#chatter-release-0').fill('0.0');
    await page.waitForFunction(() => window.saved[65] === 0);
    await page.waitForFunction(() => ![...document.querySelectorAll('.waveform .chatter-marker')].some(marker => marker.getAttribute('y1') === '37'));
    assert.ok((await page.getByTestId('input-log-entry').allInnerTexts()).every(text => !/検知/.test(text)),
      '0.0 ms must clear release chatter highlighting from the visible history');
    await page.getByRole('switch', { name: '自動設定' }).click();
    await page.evaluate(() => { window.emitDiagnostics(0, 177, 3, 200); window.emitDiagnostics(1, 177, 3, 4); });
    await page.waitForFunction(() => window.saved[65] === 5);
    await page.getByRole('button', { name: '押下時', exact: true }).click();
    await page.evaluate(() => { window.emitDiagnostics(0, 177, 3, 200); window.emitDiagnostics(1, 177, 3, 200); window.emitDiagnostics(0, 177, 3, 4); window.emitDiagnostics(1, 177, 3, 4); });
    await page.waitForFunction(() => window.saved[49] >= 5);
    await page.getByRole('switch', { name: '自動設定' }).click();
    await page.evaluate(() => window.emitDiagnostics(0, 177, 3, 9999));
    await page.evaluate(() => window.emitDiagnostics(1, 177, 3, 9999));
    await page.waitForFunction(() => document.querySelector('[data-testid="input-log-entry"]')?.textContent?.includes('999.9ms'));
    await page.evaluate(() => window.emitDiagnostics(0, 177, 3, 10000));
    await page.evaluate(() => window.emitDiagnostics(1, 177, 3, 10000));
    await page.waitForFunction(() => document.querySelector('[data-testid="input-log-entry"]')?.textContent?.includes('999.9ms'));
    assert.ok(!(await page.getByTestId('input-log-entry').first().innerText()).includes('>'));
    await page.evaluate(() => { for (let i = 0; i < 12; i++) { window.emitDiagnostics(0, 177); window.emitDiagnostics(1, 177); } });
    await page.getByTestId('input-log-entry').nth(4).click();
    const clickedLogId = await page.locator('.selected-log-entry').getAttribute('data-log-id');
    assert.equal(await page.getByTestId('selected-wave-edge').count(), 1,
      'clicking a log row should highlight its edge in the waveform');
    assert.ok(Math.abs(Number(await page.getByTestId('selected-wave-edge').getAttribute('x1')) - 371) < 2,
      'clicked log edge should be centered in the waveform');
    await page.getByTestId('input-log').evaluate(viewport => { viewport.dispatchEvent(new WheelEvent('wheel', { bubbles: true })); viewport.scrollTop = viewport.scrollHeight; });
    await page.waitForFunction(id => document.querySelector('.selected-log-entry')?.getAttribute('data-log-id') !== id, clickedLogId);
    assert.equal(await page.getByTestId('selected-wave-edge').count(), 1,
      'scrolling the log should track the center row in the waveform');
    assert.ok(await page.getByTestId('input-log').evaluate(viewport => viewport.scrollTop > 0));
    await page.evaluate(() => window.emitDiagnostics(0, 177));
    await page.waitForFunction(() => document.querySelector('[data-testid="input-log"]')?.scrollTop === 0);
    assert.equal(await page.getByTestId('selected-wave-edge').count(), 1,
      'a new release should return the log to the top without moving the historical waveform');
    await page.evaluate(() => window.emitDiagnostics(1, 177));
    await page.waitForFunction(() => !document.querySelector('.selected-log-entry'));
    assert.equal(await page.getByTestId('wave-history').inputValue(), '0',
      'a new button press should return the waveform to live view');
    await page.evaluate(() => { window.mockEdges[1] = 4; window.mockDetected[1] = 1; window.emitCounts(); });
    await page.waitForFunction(() => document.querySelectorAll('.slider-row')[1]?.textContent?.includes('25.0%'));
    assert.match(await page.locator('.slider-row').nth(1).innerText(), /エッジ\s*4[\s\S]*検知数\s*1[\s\S]*25\.0%/);
    assert.ok(await page.evaluate(() => {
      const panel = document.querySelector('.settings-panel').getBoundingClientRect();
      const last = document.querySelectorAll('.slider-row')[6].getBoundingClientRect();
      return last.bottom <= panel.bottom + 1;
    }), 'all seven button metrics should remain inside the settings panel');
    await page.waitForFunction(() => document.querySelectorAll('[data-testid="input-log-entry"]').length >= 10);
    assert.ok(await page.evaluate(() => {
      const rows = document.querySelectorAll('[data-testid="input-log-entry"]');
      const viewport = document.querySelector('.log-list');
      return rows[9].getBoundingClientRect().bottom <= viewport.getBoundingClientRect().bottom + 1;
    }), 'ten input log entries should fit in the viewport');
    await page.locator('.log-panel').getByRole('button', { name: 'リセット' }).click();
    await page.waitForFunction(() => document.querySelector('.summary.total strong')?.textContent === '0');
    await page.evaluate(() => window.emitDiagnostics(0, 177));
    await page.waitForFunction(() => document.querySelector('.summary.total strong')?.textContent === '1');
    await page.locator('#chatter-press-0').fill('0.0');
    await page.getByRole('switch', { name: '自動設定' }).click();
    await page.evaluate(() => {
      window.emitDiagnostics(1, 177, 3, 200);
      window.emitDiagnostics(0, 177, 3, 0);
    });
    await page.waitForFunction(() => window.saved[49] === 1);
    await page.getByRole('switch', { name: '自動設定' }).click();
    // A reset or lost packet removes the phase origin. Do not classify a
    // later candidate using an invented elapsed time of zero.
    await page.locator('.log-panel').getByRole('button', { name: 'リセット' }).click();
    await page.evaluate(() => {
      window.emitDiagnostics(1, 177, 3, 0);
      window.emitDiagnostics(0, 177, 3, 0);
    });
    assert.ok((await page.getByTestId('input-log-entry').allInnerTexts()).every(text => !/検知/.test(text)));
    await page.locator('#chatter-press-0').fill('1.0');
    await page.evaluate(() => {
      window.emitDiagnostics(1, 177, 3, 200, 1, false);
      window.emitDiagnostics(0, 177, 3, 2, 0, false);
    });
    assert.match(await page.getByTestId('input-log-entry').first().innerText(), /通常/,
      'firmware decision must override a shorter rounded app interval');
    await page.evaluate(() => {
      window.emitDiagnostics(1, 177, 3, 2, 1, false);
      window.emitDiagnostics(0, 177, 4, 2, 1, true);
    });
    assert.match(await page.getByTestId('input-log-entry').first().innerText(), /検知/);
    await page.locator('.log-panel').getByRole('button', { name: 'リセット' }).click();
    await page.evaluate(() => {
      for (let i = 0; i < 1100; i++) window.emitDiagnostics(i % 2 ? 0 : 1, 177, 0, 200);
    });
    await page.waitForFunction(() => document.querySelector('[data-testid="input-log"]')?.scrollHeight >= 500 * 19);
    const retainedLog = await page.getByTestId('input-log').evaluate(viewport => ({
      rowsInDom: viewport.querySelectorAll('[data-testid="input-log-entry"]').length,
      scrollHeight: viewport.scrollHeight
    }));
    assert.equal(retainedLog.scrollHeight, 500 * 19, '1000 retained logs should contain 500 press entries');
    assert.ok(retainedLog.rowsInDom <= 25, 'the input log should render only nearby rows');
    await page.waitForFunction(() => document.querySelector('.key-light')?.style.opacity === '0.37');
    await page.getByRole('button',{name:/^ファームウェア/}).click();
    const href = await page.getByRole('link',{name:'RP2354B用UF2をダウンロード'}).getAttribute('href');
    const response = await page.request.get(new URL(href,page.url()).href);
    assert.equal(response.status(),200);
    assert.deepEqual(await response.body(),fs.readFileSync('../beef-config/static/firmware/Beef-Board-RP2354B.uf2'));
    await page.getByRole('button',{name:'BOOTSELモードへ',exact:true}).click();
    await page.waitForFunction(() => window.writes.some(w=>w.id===2&&w.data[0]===1));
    assert.deepEqual(pageErrors,[]);
    console.log('PASS RP2354B UI: unprefixed reports, 255 LEDs, effect save, single remap, actual PWM, UF2 download and BOOTSEL');
  } finally {
    if (browser) await browser.close();
    server.close();
  }
})().catch(err => {
  console.error(err);
  process.exitCode = 1;
});
