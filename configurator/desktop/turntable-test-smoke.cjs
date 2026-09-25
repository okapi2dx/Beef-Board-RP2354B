const { chromium } = require('@playwright/test');
const http = require('node:http');
const fs = require('node:fs');
const path = require('node:path');
const assert = require('node:assert/strict');

(async () => {
  const root = path.resolve('../beef-config/build');
  const server = http.createServer((req, res) => {
    const requestPath = req.url.split('?')[0];
    const entry = fs.existsSync(path.join(root, 'index.html')) ? 'index.html' : '404.html';
    const file = path.join(root, requestPath === '/' ? entry : requestPath);
    if (!file.startsWith(root + path.sep) || !fs.existsSync(file)) {
      res.writeHead(404);
      res.end();
      return;
    }
    res.setHeader(
      'Content-Type',
      file.endsWith('.js') ? 'application/javascript' : file.endsWith('.css') ? 'text/css' : 'text/html'
    );
    res.end(fs.readFileSync(file));
  });

  await new Promise((resolve) => server.listen(0, '127.0.0.1', resolve));
  let browser;
  try {
    browser = await chromium.launch({ channel: 'msedge', headless: true });
    const page = await browser.newPage({ viewport: { width: 980, height: 740 } });
    page.setDefaultTimeout(10000);
    const errors = [];
    page.on('pageerror', (e) => errors.push(String(e)));

    await page.addInitScript(() => {
      localStorage.setItem('beef-language', 'ja');

      // Report ID + config packet. Version 29 keeps diagnostics on the
      // receiveFeatureReport polling path while supporting the current
      // joystick key-reset UI.
      const bytes = new Uint8Array(1025);
      bytes[0] = 1;
      bytes[1] = 29;
      bytes[5] = 1;
      bytes[31] = 10;
      bytes[79] = 60;
      bytes[81] = 24;
      bytes[85] = 1;
      bytes[89] = 100;
      for (let i = 0; i < 11; i++) bytes[128 + i] = i;

      window.testRaw = 0;
      window.sensorAB = 0;
      window.reads = 0;
      window.writes = [];

      const device = {
        productName: 'BEEF BOARD',
        vendorId: 0xfeed,
        productId: 0,
        opened: true,
        collections: [{ usagePage: 0xffeb, usage: 0x01 }],
        close: async () => {},
        receiveFeatureReport: async (id) => {
          if (id === 4) {
            window.reads++;
            const d = new Uint8Array(25);
            d[0] = 4;
            d[3] = window.sensorAB;
            d[6] = window.testRaw;
            return new DataView(d.buffer);
          }
          return new DataView(bytes.buffer.slice(0));
        },
        sendFeatureReport: async (id, data) => window.writes.push({ id, data: [...data] })
      };

      Object.defineProperty(navigator, 'hid', {
        value: {
          requestDevice: async () => [device],
          getDevices: async () => [device],
          addEventListener() {},
          removeEventListener() {}
        }
      });
      if (!navigator.usb) Object.defineProperty(navigator, 'usb', { value: {} });
    });

    await page.goto(`http://127.0.0.1:${server.address().port}`);
    await page.getByRole('button', { name: '接続', exact: true }).click();

    const meter = page.getByRole('meter', { name: '設定反映後のX軸', exact: true });
    await meter.waitFor();

    const sensitivityHelp = page.getByRole('button', { name: 'ターンテーブル感度の説明', exact: true });
    assert.equal(await sensitivityHelp.count(), 1, 'Sensitivity help button must exist');
    await sensitivityHelp.focus();
    const sensitivityTooltip = page.locator('[data-slot="tooltip-content"]');
    await sensitivityTooltip.waitFor({ state: 'visible' });
    for (const line of [
      'ターンテーブルの回転に対する入力量を調整します。',
      '感度10では1カウント、感度5では2カウントです。',
      '感度1では10カウントです。'
    ]) {
      assert.equal(await sensitivityTooltip.getByText(line, { exact: true }).count(), 1, `Missing sensitivity help line: ${line}`);
    }
    const tooltipFits = await sensitivityTooltip.evaluate((el) => el.scrollWidth <= el.clientWidth + 1);
    assert(tooltipFits, 'Sensitivity tooltip content must stay inside its frame');
    await page.keyboard.press('Tab');

    const ledSubtitle = page.locator('.led-nav-subtitle');
    assert.equal(await ledSubtitle.count(), 1, 'LED sidebar subtitle must exist');
    const ledSubtitleFits = await ledSubtitle.evaluate((el) => {
      const style = getComputedStyle(el);
      return style.whiteSpace === 'nowrap' && el.scrollWidth <= el.clientWidth + 1 && el.scrollHeight <= el.clientHeight + 1;
    });
    assert(ledSubtitleFits, 'LED sidebar subtitle must fit on one line');

    for (const line of [
      'アナログ設定では、各設定を反映したX軸値を表示します。',
      '停止後は、設定した保持時間だけ最後の方向へ入力を続けます。',
      'デジタル設定では、X軸ではなくTT-/TT+をゲームへ送ります。'
    ]) {
      assert.equal(await page.getByText(line, { exact: true }).count(), 1, `Missing test help line: ${line}`);
    }

    for (const value of [0, 128, 255, 0]) {
      await page.evaluate((v) => { window.testRaw = v; }, value);
      await page.waitForFunction(
        (v) => document.querySelector('[role="meter"]')?.getAttribute('aria-valuenow') === String(v),
        value
      );
    }

    const phases = page.getByTestId('turntable-phases');
    for (const [sensorAB, expected] of [[0, '0/0'], [1, '0/1'], [3, '1/1'], [2, '1/0']]) {
      await page.evaluate((v) => { window.sensorAB = v; }, sensorAB);
      await page.waitForFunction(
        (v) => document.querySelector('[data-testid="turntable-phases"]')?.textContent?.replace(/\s+/g, ' ').trim().endsWith(v),
        expected
      );
      assert((await phases.textContent()).replace(/\s+/g, ' ').trim().endsWith(expected));
    }

    assert.equal(await page.getByText('回転方向', { exact: true }).count(), 0);
    assert.deepEqual(errors, []);

    console.log('PASS: sensitivity help, simplified turntable help, X-axis updates, A/B phase states, no direction label');
  } finally {
    if (browser) await browser.close();
    server.close();
  }
})().catch((e) => {
  console.error(e);
  process.exitCode = 1;
});
