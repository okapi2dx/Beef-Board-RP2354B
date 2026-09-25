const { test } = require('node:test');
const assert = require('node:assert/strict');
const { EventEmitter } = require('node:events');
const fs = require('node:fs/promises');
const path = require('node:path');
const { createFlasher, validateHex } = require('./native-flash.cjs');
const good = ':0400000001020304F2\n:00000001FF\n';
test('HEX rejects corrupt/empty/trailing data and accepts AVR upper addresses', () => {
  assert.equal(validateHex(good), 4);
  for (const text of ['', ':00000001FF', good.replace('F2', 'F1'), good + good]) assert.throws(() => validateHex(text));
  assert.equal(validateHex(':020000040001F9\n' + good), 4);
  assert.throws(() => validateHex(':020000040002F8\n' + good));
});
test('native writer flashes first, then runs start_app only after successful verification', async () => {
  const folder = await fs.mkdtemp(path.join(__dirname, 'test-flash-'));
  const filename = path.join(folder, '日本語 file.hex');
  const calls = [];
  const children = [];
  const flasher = createFlasher('C:/tools', (exe, argv, opts) => {
    const child = new EventEmitter();
    child.stdout = new EventEmitter();
    child.stderr = new EventEmitter();
    calls.push({ exe, argv, opts });
    children.push(child);
    return child;
  });
  try {
    await fs.writeFile(filename, good);
    assert.equal((await flasher.select(filename)).bytes, 4);
    await fs.writeFile(filename, 'invalid after selection');

    const resultPromise = flasher.flash(() => {});
    await assert.rejects(flasher.flash(() => {}));
    while (children.length < 1) await new Promise(r => setTimeout(r, 5));

    assert.equal(await fs.readFile(path.join(calls[0].opts.cwd, 'firmware.hex'), 'utf8'), good);
    assert.equal(path.basename(calls[0].exe), 'avrdude.exe');
    assert.deepEqual(calls[0].argv.slice(-6), ['-c', 'flip1', '-p', 'usb1286', '-U', 'flash:w:firmware.hex:i']);
    assert.equal(calls[0].opts.shell, false);
    assert.equal(calls[0].opts.windowsHide, true);
    assert.equal(calls[0].argv.includes('-V'), false);
    assert.equal(calls[0].argv.includes('-F'), false);

    children[0].emit('close', 0);
    while (children.length < 2) await new Promise(r => setTimeout(r, 5));
    assert.deepEqual(calls[1].argv.slice(-7), ['-c', 'flip1', '-p', 'usb1286', '-F', '-x', 'start_app']);
    children[1].emit('close', 0);

    assert.deepEqual(await resultPromise, { success: true, exitCode: 0, restartSuccess: true, restartExitCode: 0 });
    assert.equal(flasher.busy, false);
    await assert.rejects(fs.stat(calls[0].opts.cwd));
  } finally {
    await fs.unlink(filename).catch(() => {});
    await fs.rmdir(folder).catch(() => {});
  }
});

test('native DFU reconnect starts the normal application without selected firmware', async () => {
  const calls = [];
  const children = [];
  const flasher = createFlasher('C:/tools', (exe, argv, opts) => {
    const child = new EventEmitter();
    child.stdout = new EventEmitter();
    child.stderr = new EventEmitter();
    calls.push({ exe, argv, opts });
    children.push(child);
    return child;
  });

  const resultPromise = flasher.restartDfu(() => {});
  while (!children.length) await new Promise(r => setTimeout(r, 5));
  assert.equal(path.basename(calls[0].exe), 'avrdude.exe');
  assert.deepEqual(calls[0].argv.slice(-7), ['-c', 'flip1', '-p', 'usb1286', '-F', '-x', 'start_app']);
  assert.equal(calls[0].opts.shell, false);
  assert.equal(calls[0].opts.windowsHide, true);
  children[0].emit('close', 0);
  assert.deepEqual(await resultPromise, { success: true, exitCode: 0 });
  assert.equal(flasher.busy, false);
});

test('native writer never runs start_app after a failed flash', async () => {
  const folder = await fs.mkdtemp(path.join(__dirname, 'test-flash-fail-'));
  const filename = path.join(folder, 'firmware.hex');
  const children = [];
  const flasher = createFlasher('C:/tools', (_exe, _argv, _opts) => {
    const child = new EventEmitter();
    child.stdout = new EventEmitter();
    child.stderr = new EventEmitter();
    children.push(child);
    return child;
  });
  try {
    await fs.writeFile(filename, good);
    await flasher.select(filename);
    const resultPromise = flasher.flash(() => {});
    while (!children.length) await new Promise(r => setTimeout(r, 5));
    children[0].emit('close', 1);
    assert.deepEqual(await resultPromise, { success: false, exitCode: 1, restartSuccess: false, restartExitCode: null });
    assert.equal(children.length, 1);
  } finally {
    await fs.unlink(filename).catch(() => {});
    await fs.rmdir(folder).catch(() => {});
  }
});
