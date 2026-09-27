import assert from 'node:assert/strict';
import { readFile, writeFile, unlink } from 'node:fs/promises';
import ts from 'typescript';
import { compileModule } from 'svelte/compiler';

const generated = [];
async function moduleFile(name, source) {
  const js = ts.transpileModule(source, { compilerOptions: { target: ts.ScriptTarget.ESNext, module: ts.ModuleKind.ESNext } }).outputText;
  const output = compileModule(js, { filename: `${name}.svelte.js`, generate: 'client' }).js.code;
  const url = new URL(`./.bar-test-${name}.mjs`, import.meta.url);
  await writeFile(url, output);
  generated.push(url);
  return url;
}
try {
  const types = await moduleFile('types', await readFile(new URL('./src/lib/types/types.svelte.ts', import.meta.url), 'utf8'));
  const codes = await moduleFile('codes', await readFile(new URL('./src/lib/types/hid-codes.ts', import.meta.url), 'utf8'));
  const feature = await moduleFile('feature-report', await readFile(new URL('./src/lib/types/feature-report.ts', import.meta.url), 'utf8'));
  let source = await readFile(new URL('./src/lib/types/config.svelte.ts', import.meta.url), 'utf8');
  source = source.replace("import { ReportId } from '$lib/types/hid';", 'const ReportId = { Config: 1 };')
    .replace("import { appState } from '$lib/types/state.svelte';", 'const appState = globalThis.__barTestState;')
    .replace('$lib/types/types.svelte', types.href).replace('$lib/types/hid-codes', codes.href)
    .replace('$lib/types/feature-report', feature.href);
  let sent;
  globalThis.__barTestState = { device: { sendFeatureReport: async (_id, data) => { sent = data; } } };
  const { Config, readConfig, updateConfig, flushPendingConfigWrites } = await import((await moduleFile('config', source)).href);
  const { BarMode, barModeToNumber, ControllerType, controllerTypeToNumber, Hsv } = await import(types.href);
  const received = new Uint8Array(1024);
  received[0] = 33;
  received[1] = 1;
  for (const report of [received, Uint8Array.from([1, ...received])]) {
    globalThis.__barTestState.device.receiveFeatureReport = async () => new DataView(report.buffer);
    const config = await readConfig();
    assert.equal(config.version, 33);
    assert.equal(config.reverse_tt, 1);
  }
  assert.deepEqual(Object.values(ControllerType), ['Default', 'IIDX Entry', 'IIDX Premium']);
  assert.deepEqual(Object.values(ControllerType).map(mode => controllerTypeToNumber[mode]), [0, 1, 2]);
  assert.equal(barModeToNumber[BarMode.Static], 8);
  assert.equal(barModeToNumber[BarMode.Off], 7);
  for (const version of [16, 17, 19, 20, 21, 22, 23, 24]) {
    const bytes = new Uint8Array(1024);
    bytes[0] = version;
    bytes[80] = 24;
    bytes[4] = version >= 17 ? 8 : 4;
    bytes.set([21, 130, 240], 6); // independent turntable color
    bytes.set([61, 128, 200], 81);
    if (version >= 19) bytes[84] = 1;
    if (version >= 20) bytes[85] = 1;
    const config = new Config(new DataView(bytes.buffer));
    config.bar_static_hsv = new Hsv(85, 153, 255);
    if (version >= 19) config.link_bar_effect = true;
    if (version >= 20) config.digital_tt = true;
    await updateConfig(config);
    assert.deepEqual([...sent.slice(6, 9)], [21, 130, 240]);
    assert.deepEqual([...sent.slice(81, 84)], version >= 17 ? [85, 153, 255] : []);
    if (version >= 19) assert.equal(sent[84], 1);
    if (version >= 20) assert.equal(sent[85], 1);
    const restored = new Config(new DataView(sent.buffer));
    if (version === 17) {
      assert.equal(restored.bar_effect, BarMode.Static);
      assert.deepEqual(restored.bar_static_hsv.toHid(), [85, 153, 255]);
    }
    if (version >= 19) {
      assert.equal(restored.bar_effect, BarMode.Static);
      assert.deepEqual(restored.bar_static_hsv.toHid(), [85, 153, 255]);
      assert.equal(restored.link_bar_effect, true);
    }
    if (version >= 20) assert.equal(restored.digital_tt, true);
    if (version >= 23) {
      for (const [fade, brightness, inverted] of [[0, 0, false], [50, 50, true], [255, 100, false]]) {
        config.button_led_fade_ms = fade;
        config.button_led_brightness = brightness;
        config.button_led_invert = inverted;
        await updateConfig(config);
        assert.deepEqual([...sent.slice(87, 90)], [fade, brightness, Number(inverted)]);
        const roundTrip = new Config(new DataView(sent.buffer));
        assert.equal(roundTrip.button_led_fade_ms, fade);
        assert.equal(roundTrip.button_led_brightness, brightness);
        assert.equal(roundTrip.button_led_invert, inverted);
      }
      for (const [field, value] of [['button_led_fade_ms', 256], ['button_led_fade_ms', -1], ['button_led_brightness', 101], ['button_led_brightness', 0.5]]) {
        const previous = config[field];
        config[field] = value;
        await assert.rejects(() => updateConfig(config));
        config[field] = previous;
      }
    } else {
      assert.equal(restored.button_led_fade_ms, 0);
      assert.equal(restored.button_led_brightness, 100);
      assert.equal(restored.button_led_invert, false);
      assert.deepEqual([...sent.slice(87, 90)], []);
    }
    if (version >= 24) {
      config.button_led_individual = true;
      config.button_led_fade = Array.from({ length: 11 }, (_, i) => i);
      config.button_led_level = Array.from({ length: 11 }, (_, i) => 100 - i);
      config.button_led_inverted = Array.from({ length: 11 }, (_, i) => i % 2 === 0);
      config.tt_curve = 'Dynamic';
      await updateConfig(config);
      assert.equal(sent[90], 1);
      assert.deepEqual([...sent.slice(91, 102)], config.button_led_fade);
      assert.deepEqual([...sent.slice(102, 113)], config.button_led_level);
      assert.deepEqual([...sent.slice(113, 124)], config.button_led_inverted.map(Number));
      assert.equal(sent[124], 2);
    }
    if (version >= 21) {
      for (let delay = 0; delay <= 255; delay++) {
        config.tt_delay_ms = delay;
        await updateConfig(config);
        assert.equal(sent[86], delay);
        assert.equal(new Config(new DataView(sent.buffer)).tt_delay_ms, delay);
      }
      for (const delay of [-1, 256, 1.5, NaN]) {
        config.tt_delay_ms = delay;
        await assert.rejects(() => updateConfig(config));
      }
    } else assert.equal(restored.tt_delay_ms, 0);
    if (version >= 22) {
      config.tt_delay_ms = 0;
      for (const duration of [0, 50, 255]) {
        config.tt_deadzone = duration;
        await updateConfig(config);
        assert.equal(sent[3], duration);
        assert.equal(new Config(new DataView(sent.buffer)).tt_deadzone, duration);
      }
      for (const duration of [-1, 256, 1.5]) {
        config.tt_deadzone = duration;
        await assert.rejects(() => updateConfig(config));
      }
    }
  }
  const packet = new Uint8Array(139);
  packet[0] = 33; packet[80] = 24; packet[138] = 16;
  const edits = [];
  let release;
  const device = {
    receiveFeatureReport: async () => new DataView(packet.buffer),
    sendFeatureReport: async (_id, bytes) => {
      edits.push(bytes.slice());
      if (edits.length === 1) await new Promise(resolve => { release = resolve; });
    }
  };
  globalThis.__barTestState.device = device;
  const config = await readConfig();
  config.tt_delay_ms = 10;
  const firstWrite = updateConfig(config);
  await new Promise(resolve => setTimeout(resolve, 0));
  config.tt_delay_ms = 0; // Return to the previously saved value while B is in flight.
  const lastWrite = updateConfig(config);
  assert.equal(edits.length, 1);
  let flushed = false;
  const flush = flushPendingConfigWrites().then(() => { flushed = true; });
  await Promise.resolve();
  assert.equal(flushed, false);
  release();
  await Promise.all([firstWrite, lastWrite, flush]);
  assert.equal(flushed, true);
  assert.deepEqual(edits.map(bytes => bytes[86]), [10, 0]);
  assert(edits.every(bytes => bytes.length === 139));
  for (const count of [0, -1, 256, 1.5, NaN]) {
    config.tt_leds = count;
    await assert.rejects(() => updateConfig(config));
  }
  config.tt_leds = 24;
  let replacementWrites = 0;
  globalThis.__barTestState.device = {
    sendFeatureReport: async () => { replacementWrites++; if (replacementWrites === 1) throw new Error('Temporary USB failure'); }
  };
  await assert.rejects(() => updateConfig(config));
  await updateConfig(config);
  assert.equal(replacementWrites, 2, 'Failed writes must be retryable on another device');
  console.log('PASS: queued save/revert, exact RP2354B packet size, LED count validation, device-scoped cache and failed-write retry');
  console.log('PASS: static mode, independent color round-trip, and legacy version compatibility');
} finally {
  delete globalThis.__barTestState;
  await Promise.all(generated.map(url => unlink(url)));
}
