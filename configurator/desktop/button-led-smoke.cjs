const { _electron: electron } = require('@playwright/test');
const assert = require('node:assert/strict');
const path = require('node:path');
(async () => {
  const app = await electron.launch({ timeout: 15000, args: ['--disable-gpu'], executablePath: path.resolve('../../../outputs/build/win-unpacked/Beef Board Configurator.exe'),
    env: { ...process.env, BEEF_TEST_USER_DATA: path.resolve('../../../work/button-led-ui-test-' + Date.now()) } });
  try {
    const page = await app.firstWindow({ timeout: 10000 });
    page.setDefaultTimeout(10000);
    await page.getByRole('button', { name: '接続 / Connect Device' }).waitFor();
    await page.evaluate(() => {
      const bytes = new Uint8Array(1025);
      bytes[0] = 1; bytes[1] = 23; bytes[31] = 2; bytes[79] = 60;
      bytes[80] = 1; bytes[81] = 24; bytes[89] = 100;
      window.testWrites = [];
      const device = { productName: 'BEEF BOARD', vendorId: 0xfeed, productId: 0, opened: true,
        close: async () => {}, receiveFeatureReport: async () => new DataView(bytes.buffer),
        sendFeatureReport: async (id, data) => window.testWrites.push({ id, data: [...data] }) };
      Object.defineProperty(navigator.hid, 'requestDevice', { configurable: true, value: async () => [device] });
    });
    await page.getByRole('button', { name: '接続 / Connect Device' }).click();
    await page.locator('input#button-led-fade').fill('255');
    await page.locator('input#button-led-brightness').fill('50');
    await page.getByText('ボタンLEDを反転', { exact: true }).click();
    await page.waitForFunction(() => window.testWrites.some(x => x.id === 1 && x.data[87] === 255 && x.data[88] === 50 && x.data[89] === 1));
    assert.equal(await page.locator('input#button-led-fade').getAttribute('max'), '255');
    assert.equal(await page.locator('input#button-led-brightness').getAttribute('max'), '100');
    await page.screenshot({ path: path.resolve('../../../outputs/button-led-settings.png'), fullPage: true });
    console.log('PASS: packaged button LED controls auto-save 255ms, 50%, inversion via mock HID');
  } finally { await app.close(); }
})().catch(e => { console.error(e); process.exitCode = 1; });
