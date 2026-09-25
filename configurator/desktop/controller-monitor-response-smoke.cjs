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
    const pageErrors = [];
    page.on('pageerror', error => pageErrors.push(String(error)));
    await page.addInitScript(() => {
      localStorage.setItem('beef-language', 'ja');

      const config = new Uint8Array(139);
      config[0] = 1;
      config[1] = 30;
      config[31] = 2;
      config[79] = 60;
      config[80] = 3;
      config[81] = 24;
      config[89] = 100;
      for (let i = 103; i < 114; i++) config[i] = 100;
      for (let i = 0; i < 11; i++) config[128 + i] = i;

      window.featureDiagnosticReads = 0;
      window.listenerAttached = false;
      const inputReportListeners = new Set();
      const device = {
        productName: 'BEEF BOARD', vendorId: 0xfeed, productId: 0, opened: true,
        open: async () => {}, close: async () => {},
        receiveFeatureReport: async id => {
          if (id === 4) {
            window.featureDiagnosticReads++;
            const report = new Uint8Array(15);
            report[0] = 4;
            return new DataView(report.buffer);
          }
          return new DataView(config.buffer.slice(0));
        },
        sendFeatureReport: async (id, data) => {
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
        report[5] = output;
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
    await page.getByRole('button', { name: '接続 / Connect Device' }).click();
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
    console.log('PASS: v30 controller monitor uses interrupt input reports and performs zero diagnostic Feature Report polls');
  } finally {
    if (browser) await browser.close();
    server.close();
  }
})().catch(err => {
  console.error(err);
  process.exitCode = 1;
});
