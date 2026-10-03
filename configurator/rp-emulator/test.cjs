const { _electron: electron } = require('../desktop/node_modules/@playwright/test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
(async () => {
  const data = fs.mkdtempSync(path.join(os.tmpdir(), 'beef-rp-emulator-'));
  const options = { executablePath: path.join(__dirname, 'dist/win-unpacked/Beef Board RP Emulator.exe'),
    args: [], timeout: 20000, env: { ...process.env, BEEF_RP_EMULATOR_USER_DATA: data } };
  const errors = [];
  let app = await electron.launch(options);
  try {
    let page = await app.firstWindow(); page.on('pageerror', e => errors.push(String(e)));
    await page.getByText('旧Phoenixwanを使用', { exact: true }).waitFor();
    assert.equal(await page.evaluate(() => window.rpEmulator.device.productId), 3);
    const picker = page.getByText('ターンテーブルの発光効果', { exact: true }).locator('..').locator('[data-slot=select-trigger]');
    const modes = ['1：単色','2：回転','3：シフト','4：固定レインボー','5：回転レインボー','6：トリガー','7：ブレス'];
    const supported = [[true,true,true],[true,false,false],[false,true,true],[false,true,true],[false,true,true],[true,true,false],[true,true,false]];
    for (let mode = 0; mode < modes.length; mode++) {
      await picker.click(); await page.getByRole('option', { name: modes[mode], exact: true }).click();
      for (const [channel, label] of ['色相','彩度','明るさ'].entries()) {
        assert.equal(await page.getByText(label, { exact: true }).count(), 2, 'Both lighting panels always show all three controls');
        for (const action of ['下げる','上げる']) {
          const buttons = page.getByRole('button', { name: new RegExp(`${label}.*${action}`) });
          assert.equal(await buttons.first().isEnabled(), supported[mode][channel]);
          assert.equal(await buttons.nth(1).isEnabled(), true, 'Center bar controls remain available');
        }
        const opacity = await page.locator(`#legacy-phoenixwan-turntable-${['hue','saturation','brightness'][channel]}`)
          .evaluate(el => Number(getComputedStyle(el.parentElement.parentElement).opacity));
        assert.equal(opacity, supported[mode][channel] ? 1 : 0.5, 'Unsupported rows are grayed out');
      }
      if (mode === 1) {
        await page.locator('#legacy-phoenixwan-turntable-brightness').scrollIntoViewIfNeeded();
        await page.screenshot({ path: path.join(__dirname, 'dist/rp-emulator-disabled-controls.png') });
      }
    }
    await picker.click(); await page.getByRole('option', { name: modes[0], exact: true }).click();
    await page.getByRole('button', { name: '入力パネル', exact: true }).click();
    await page.locator('#emu-panel').getByRole('button', { name: 'B3', exact: true }).click();
    const packet = await page.evaluate(async () => {
      const emu = window.rpEmulator, report = await emu.device.receiveFeatureReport(4), counts = await emu.device.receiveFeatureReport(7);
      return { bytes: [...new Uint8Array(report.buffer)], edges: counts.getUint16(4, true) };
    });
    assert.equal(packet.bytes.length, 60); assert.equal(packet.bytes[0], 4);
    assert.equal(packet.bytes[15], 255); assert.equal(packet.bytes[38], 4);
    assert.equal(packet.edges, 1); assert(packet.bytes.slice(24, 38).every(v => v === 0));
    await page.locator('#emu-axis').evaluate(el => { el.value = '220'; el.dispatchEvent(new Event('input', { bubbles: true })); });
    assert.deepEqual(await page.evaluate(() => [...window.rpEmulator.diagnostics().slice(3, 6)]), [220,220,220]);
    await page.locator('#emu-panel').getByRole('button', { name: '回転デモ', exact: true }).click();
    await page.waitForFunction(() => window.rpEmulator.state.transitions > 100);
    await page.locator('#emu-panel').getByRole('button', { name: '回転を停止', exact: true }).click();
    await page.locator('#emu-panel').getByRole('button', { name: '全ボタンを離す', exact: true }).click();
    assert.equal(await page.evaluate(() => window.rpEmulator.state.buttons), 0);
    await page.getByRole('button', { name: '入力パネル', exact: true }).click();
    await page.getByRole('button', { name: /色相.*上げる/ }).first().click();
    await page.waitForFunction(() => window.rpEmulator.state.commands.some(s => s.includes('HID 7')));
    await page.getByRole('button', { name: '点灯数を2灯減らす', exact: true }).click();
    await page.waitForFunction(() => window.rpEmulator.config[61] === 14);
    await page.getByRole('switch', { name: '旧Phoenixwanを使用', exact: true }).click();
    await page.waitForFunction(() => !window.rpEmulator.legacy);
    await page.getByRole('button', { name: '入力パネル', exact: true }).click();
    await page.getByRole('button', { name: '疑似USBを切断', exact: true }).click();
    await page.getByRole('button', { name: '接続', exact: true }).waitFor();
    await page.getByRole('button', { name: '疑似USBを接続', exact: true }).click();
    await page.waitForFunction(() => window.rpEmulator.device.opened);
    await page.getByRole('button', { name: '入力パネル', exact: true }).click();
    await page.getByRole('button', { name: /^入力設定/ }).click();
    await page.getByRole('button', { name: 'デフォルト', exact: true }).click();
    await page.getByRole('option', { name: 'IIDX Entry', exact: true }).click();
    await page.waitForFunction(() => window.rpEmulator.device.productId === 0x1018 && window.rpEmulator.device.opened);
    await page.getByText('IIDX Entry Model · RP2354B', { exact: true }).first().waitFor();
    assert.equal(await page.evaluate(async () => (await navigator.usb.getDevices()).length), 0);
    await app.close(); app = await electron.launch(options);
    page = await app.firstWindow(); page.on('pageerror', e => errors.push(String(e)));
    await page.getByText('旧Phoenixwanを使用', { exact: true }).waitFor();
    assert.deepEqual(await page.evaluate(() => ({ count: window.rpEmulator.config[61], legacy: window.rpEmulator.legacy,
      mode: window.rpEmulator.config[31], product: window.rpEmulator.device.productId })),
      { count: 14, legacy: false, mode: 1, product: 0x1018 });
    assert.deepEqual(errors, []);
    await page.screenshot({ path: path.join(__dirname, 'dist/rp-emulator-test.png') });
    console.log('PASS standalone emulator: seven-mode HSB visibility/disabled matrix, auto-connect, report layouts, buttons/PWM, rotation, HSB commands, setting saves, USB detach/attach, identity restart and persisted profile; no physical USB');
  } finally { await app.close(); }
})().catch(error => { console.error(error); process.exitCode = 1; });
