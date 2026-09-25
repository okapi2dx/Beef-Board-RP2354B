const { _electron: electron } = require('@playwright/test');
const assert = require('node:assert/strict');
const path = require('node:path');
(async () => {
  const testData = path.join(__dirname, '../../../work/test-user-data-firmware');
  const app = await electron.launch(process.env.BEEF_TEST_EXE ? { executablePath: process.env.BEEF_TEST_EXE, args: [], timeout: 20000, env: { ...process.env, BEEF_TEST_USER_DATA: testData } } : { args: [__dirname], timeout: 20000, env: { ...process.env, BEEF_TEST_USER_DATA: testData } });
  try {
    const page = await app.firstWindow();
    page.setDefaultTimeout(10000);
    const errors = [];
    page.on('pageerror', e => errors.push(String(e)));
    await page.getByRole('button', { name: '日本語', exact: true }).click();
    await page.getByRole('option', { name: 'English', exact: true }).click();
    await page.getByRole('button', { name: 'English', exact: true }).click();
    await page.getByRole('option', { name: '日本語', exact: true }).click();
    await page.getByRole('button', { name: /^ファームウェア/ }).click();
    const input = page.locator('input[type=file]');
    const write = page.getByRole('button', { name: '書き込み開始', exact: true });
    assert.equal(await write.isDisabled(), true);
    await page.evaluate(() => {
      window.usbWrites = []; window.failWrite = false;
      const device = { open: async () => {}, close: async () => {}, selectConfiguration: async () => {}, claimInterface: async () => {},
        controlTransferOut: async (request, bytes) => {
          await new Promise(resolve => setTimeout(resolve, 50));
          if (window.failWrite) throw new Error('simulated USB failure');
          window.usbWrites.push({ request, bytes: bytes ? [...bytes] : [] }); return { status: 'ok' };
        },
        controlTransferIn: async () => ({ status: 'ok', data: new DataView(new Uint8Array([0, 0, 0, 0, 2, 0]).buffer) }) };
      Object.defineProperty(navigator.usb, 'requestDevice', { value: async () => device });
    });
    const valid = { name: 'test-beef.hex', mimeType: 'text/plain', buffer: Buffer.from(':0400000001020304F2\n:00000001FF\n') };
    await input.setInputFiles(valid);
    await page.getByTestId('selected-firmware').waitFor();
    assert.equal(await write.isDisabled(), true);
    await page.getByRole('button', { name: '書き込みモードの機器に接続', exact: true }).click();
    await page.getByText('接続済み: AT90USB1286 DFU', { exact: true }).waitFor();
    await input.setInputFiles({ name: 'invalid.hex', mimeType: 'text/plain', buffer: Buffer.from('invalid') });
    await page.getByText(/HEXファイルを読み込めません/).waitFor();
    assert.equal(await write.isDisabled(), true);
    assert.equal(await page.evaluate(() => window.usbWrites.length), 0);
    await input.setInputFiles(valid);
    await page.getByTestId('selected-firmware').waitFor();
    await write.click();
    await page.getByRole('button', { name: 'キャンセル', exact: true }).click();
    assert.equal(await page.evaluate(() => window.usbWrites.length), 0);
    await page.evaluate(() => window.failWrite = true);
    await write.click();
    await page.getByRole('button', { name: 'このファイルを書き込む', exact: true }).click();
    await page.getByText(/書き込みに失敗しました/).waitFor();
    assert.equal(await write.isEnabled(), true);
    await page.evaluate(() => window.failWrite = false);
    await write.click();
    await page.getByRole('button', { name: 'このファイルを書き込む', exact: true }).click();
    assert.equal(await input.isDisabled(), true);
    await page.getByText('書き込みが完了しました。', { exact: true }).waitFor();
    const writes = await page.evaluate(() => window.usbWrites);
    assert.ok(writes.some(x => x.bytes.join(',') === '4,0,255'), 'erase');
    assert.ok(writes.some(x => x.bytes.slice(32, 36).join(',') === '1,2,3,4'), 'selected file payload');
    await page.screenshot({ path: path.join(__dirname, '../../../outputs/firmware-screen.png'), fullPage: true });
    await page.getByRole('button', { name: '通常モードで再起動', exact: true }).click();
    await page.getByText('通常モードで再起動しました。Configタブから接続してください。', { exact: true }).waitFor();
    assert.deepEqual(errors, []);
    console.log('PASS: standalone firmware selection, invalid HEX blocked, cancel sends nothing, USB failure/retry, exact payload, progress/completion/restart; mock USB only');
  } finally { await app.close(); }
})().catch(error => { console.error(error); process.exitCode = 1; });
