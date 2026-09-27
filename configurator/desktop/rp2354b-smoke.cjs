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
      config[1] = 33; config[5] = 8; config[139] = 16;
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
      window.emitDiagnostics = (buttons, output) => {
        if (!inputReportListeners.size) return false;
        const report = new Uint8Array(24);
        report[0] = buttons & 0xff;
        report[1] = (buttons >> 8) & 0xff;
        report[5] = output; report[13] = 37;
        for (const listener of inputReportListeners) {
          listener({ device, reportId: 5, data: new DataView(report.buffer) });
        }
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
    await page.getByRole('img',{name:'B4: 押下',exact:true}).waitFor();
    await page.getByRole('img',{name:'B6: 離している',exact:true}).waitFor();
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
