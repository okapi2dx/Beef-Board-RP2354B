const { chromium } = require('@playwright/test');
const http = require('node:http');
const fs = require('node:fs');
const path = require('node:path');
const assert = require('node:assert/strict');

(async () => {
  const root = path.resolve('../beef-config/build');
  const server = http.createServer((req, res) => {
    const file = path.join(root, req.url === '/' ? 'index.html' : req.url.split('?')[0]);
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
    await page.addInitScript(() => {
      localStorage.setItem('beef-language', 'ja');

      // Report id + version 29 config (same 138 config bytes as v28).
      const bytes = new Uint8Array(139);
      bytes[0] = 1;
      bytes[1] = 29;
      bytes[31] = 2;   // tt_ratio
      bytes[79] = 60;  // led_refresh
      bytes[80] = 3;   // rainbow_spin_speed
      bytes[81] = 24;  // tt_leds
      bytes[89] = 100; // button_led_brightness
      for (let i = 103; i < 114; i++) bytes[i] = 100; // per-button LED levels
      for (let i = 0; i < 11; i++) bytes[128 + i] = i; // button_mapping

      window.testWrites = [];
      const device = {
        productName: 'BEEF BOARD',
        vendorId: 0xfeed,
        productId: 0,
        opened: true,
        open: async () => {},
        close: async () => {},
        receiveFeatureReport: async () => new DataView(bytes.buffer.slice(0)),
        sendFeatureReport: async (id, data) => {
          window.testWrites.push({ id, data: [...data] });
          if (id === 1) bytes.set(data, 1);
        }
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
    await page.getByRole('button', { name: '接続 / Connect Device' }).click();
    await page.waitForTimeout(100);
    assert.equal(await page.evaluate(() => window.testWrites.length), 0, 'connecting must not rewrite config');
    await page.getByRole('button', { name: /^キー割り当て/ }).click();
    await page.getByText('ボタン配置入れ替え', { exact: true }).waitFor();

    const b1Label = page.getByText('B1', { exact: true }).first();
    const b1Card = b1Label.locator('..');
    await b1Card.getByRole('button').click();
    await page.getByRole('option', { name: 'B4', exact: true }).click();
    await page.waitForFunction(() => window.testWrites.some(w => w.id === 1 && w.data[127] === 3 && w.data[130] === 3));

    const write = await page.evaluate(() => window.testWrites.filter(w => w.id === 1).at(-1));
    assert.equal(write.data.length, 138);
    assert.equal(write.data[127], 3); // physical B1 -> logical B4
    assert.equal(write.data[130], 3); // physical B4 remains logical B4 (duplicate allowed)
    console.log('PASS: duplicate button remap serializes and sends an exact 138-byte v28 config report');
  } finally {
    if (browser) await browser.close();
    server.close();
  }
})().catch(err => {
  console.error(err);
  process.exitCode = 1;
});
