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
  const board = await moduleFile('board', await readFile(new URL('./src/lib/types/board.ts', import.meta.url), 'utf8'));
  const feature = await moduleFile('feature-report', await readFile(new URL('./src/lib/types/feature-report.ts', import.meta.url), 'utf8'));
  let source = await readFile(new URL('./src/lib/types/config.svelte.ts', import.meta.url), 'utf8');
  source = source.replace("import { ReportId } from '$lib/types/hid';", 'const ReportId = { Config: 1 };')
    .replace("import { appState } from '$lib/types/state.svelte';", 'const appState = globalThis.__barTestState;')
    .replace('$lib/types/types.svelte', types.href).replace('$lib/types/hid-codes', codes.href)
    .replace('$lib/types/feature-report', feature.href).replace('$lib/types/board', board.href);
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
  assert.equal(config.supports_b8_remapping, true, '139-byte RP2354B config report must enable B8 remapping');
  assert.equal(config.supports_bar_led_count, true, '139-byte RP2354B config report must expose center bar LED count');
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
  for (const count of [-1, 256, 1.5, NaN]) {
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
  let mappingWrites = 0;
  globalThis.__barTestState.device = {
    sendFeatureReport: async () => { mappingWrites++; }
  };
  config.button_mapping = [0, 1, 2, 3, 4, 5, 6, 11, 8, 9, 10];
  globalThis.__barTestState.boardInfo = { boardType: 'AT90USB1286' };
  await updateConfig(config);
  assert.equal(mappingWrites, 1, 'RP2354B config packet must accept B8 even when boardInfo fell back to AT90');

  const at90Packet = new Uint8Array(138);
  at90Packet[0] = 42;
  at90Packet[80] = 24;
  const at90Config = new Config(new DataView(at90Packet.buffer));
  assert.equal(at90Config.supports_b8_remapping, true, 'AT90USB1286 v42+ must enable B8 remapping');
  assert.equal(at90Config.supports_bar_led_count, false, '138-byte AT90USB1286 config report must not expose center bar LED count');
  at90Config.button_mapping = [0, 1, 2, 3, 4, 5, 6, 11, 8, 9, 10];
  let at90WriteLength = 0;
  globalThis.__barTestState.device = {
    sendFeatureReport: async (_id, bytes) => { mappingWrites++; at90WriteLength = bytes.length; }
  };
  await updateConfig(at90Config);
  assert.equal(mappingWrites, 2, 'AT90USB1286 v42+ must save B8 remapping');
  assert.equal(at90WriteLength, 138, 'AT90USB1286 writes must preserve the 138-byte HID config report');

  const oldAt90Packet = new Uint8Array(138);
  oldAt90Packet[0] = 41;
  oldAt90Packet[80] = 24;
  const oldAt90Config = new Config(new DataView(oldAt90Packet.buffer));
  assert.equal(oldAt90Config.supports_b8_remapping, false, 'AT90USB1286 v41 must keep B8 remapping disabled');
  oldAt90Config.button_mapping = [0, 1, 2, 3, 4, 5, 6, 11, 8, 9, 10];
  await assert.rejects(() => updateConfig(oldAt90Config), /AT90USB1286 firmware v42\+/);
  assert.equal(mappingWrites, 2, 'AT90USB1286 v41 must reject B8 before sending a config report');
  const legacyPacket = new Uint8Array(139);
  legacyPacket[0] = 42; legacyPacket[61] = 16; legacyPacket[80] = 24; legacyPacket[138] = 16;
  const legacyConfig = new Config(new DataView(legacyPacket.buffer));
  assert.equal(legacyConfig.legacy_phoenixwan_bar_count, 16);
  globalThis.__barTestState.device = { sendFeatureReport: async (_id, bytes) => { sent = bytes; } };
  for (const count of [0,2,4,6,8,10,12,14,16]) {
    legacyConfig.legacy_phoenixwan_bar_count = count;
    await updateConfig(legacyConfig);
    assert.equal(sent.length, 139);
    assert.equal(sent[61], count);
    assert.equal(sent[138], 16);
    assert.equal(new Config(new DataView(sent.buffer)).legacy_phoenixwan_bar_count, count);
  }
  for (const count of [-1,1,3,17,18,2.5,NaN]) {
    legacyConfig.legacy_phoenixwan_bar_count = count;
    await assert.rejects(() => updateConfig(legacyConfig));
  }
  const at90LegacyPacket = new Uint8Array(138);
  at90LegacyPacket[0] = 42; at90LegacyPacket[61] = 9; at90LegacyPacket[80] = 24;
  assert.equal(new Config(new DataView(at90LegacyPacket.buffer)).legacy_phoenixwan_bar_count, 16,
    'AT90 padding must not be interpreted as a legacy bar count');
  const newPacket = new Uint8Array(138); newPacket[0]=43;newPacket[61]=16;newPacket[80]=24;
  const newAt90 = new Config(new DataView(newPacket.buffer));
  assert.equal(newAt90.supports_bar_led_count,true); assert.equal(newAt90.bar_leds,16);
  globalThis.__barTestState.device = { sendFeatureReport: async (_id, bytes) => { sent=bytes; } };
  for (const target of [newAt90,legacyConfig]) {
    target.legacy_phoenixwan_bar_count=16;
    for (const count of [0,1,24,255]) {
      target.tt_leds=count;target.bar_leds=count;await updateConfig(target);
      assert.equal(sent.length,target.bar_led_count_at_tail?139:138);
      const restored=new Config(new DataView(sent.buffer, sent.byteOffset, sent.byteLength));
      assert.equal(restored.tt_leds,count);assert.equal(restored.bar_leds,count);
    }
  }
  for (const [version, length, angleBased] of [[43,138,false],[44,138,true],[45,138,false],[33,139,true],[42,139,true],[43,139,false]]) {
    const bytes=new Uint8Array(length);bytes[0]=version;bytes[80]=24;bytes[61]=16;
    globalThis.__barTestState.boardInfo=undefined;
    const target=new Config(new DataView(bytes.buffer));
    assert.equal(target.supports_angle_deadzone,angleBased);
    const countBased=(length===139 && version>=43)||(length===138 && version>=45);
    assert.equal(target.supports_encoder_deadzone,countBased);
    for(const value of angleBased?[0,1,10,30]:countBased?[0,1,5,50]:[0,1,5,50,255]){
      target.tt_deadzone=value;await updateConfig(target);assert.equal(sent[3],value);
    }
    target.tt_deadzone=angleBased?31:countBased?51:256;await assert.rejects(()=>updateConfig(target));
  }
  for(const [version,length] of [[45,138],[43,139]]) {
    const bytes=new Uint8Array(length);bytes[0]=version;bytes[3]=200;bytes[30]=5;bytes[80]=24;bytes[61]=16;
    const loaded=new Config(new DataView(bytes.buffer));
    assert.equal(loaded.tt_deadzone,50,'Older count settings are normalized on read');
    assert.equal(loaded.tt_ratio,5,'Other settings stay intact');
    await updateConfig(loaded);assert.equal(sent[3],50);assert.equal(sent[30],5);
  }
  for(const [version,length] of [[47,138],[45,139]]) {
    const bytes=new Uint8Array(length);bytes[0]=version;bytes[30]=6;bytes[80]=24;bytes[61]=16;
    const target=new Config(new DataView(bytes.buffer));
    assert(target.supports_multiplier);
    for(const gain of [0,.001,.333,1,1.999,2]) {
      target.tt_multiplier=gain;await updateConfig(target);
      assert.equal(new DataView(sent.buffer,sent.byteOffset,sent.byteLength).getUint16(71,true),Math.round(gain*1000));
      assert.equal(new Config(new DataView(sent.buffer,sent.byteOffset,sent.byteLength)).tt_multiplier,gain);
      assert.equal(sent.length,length);
    }
    for(const gain of [-.001,2.001,NaN]) {target.tt_multiplier=gain;await assert.rejects(()=>updateConfig(target));}
  }
  console.log('PASS: fractional multiplier exact packet round-trip for AT90/RP, zero and invalid bounds');
  console.log('PASS: old count settings above 50 can be loaded and auto-saved without losing other settings');
  console.log('PASS: AT90 v43 time / v44 angle / v45 encoder and RP v43 encoder validation, including missing BoardInfo');
  console.log('PASS: legacy center-bar count round-trip, invalid values and AT90 padding isolation');
  console.log('PASS: queued save/revert, exact RP2354B packet size, LED count validation, device-scoped cache and failed-write retry');
  console.log('PASS: B8 remapping supports RP2354B and AT90USB1286 v42+ while rejecting older AT90 firmware');
  console.log('PASS: static mode, independent color round-trip, and legacy version compatibility');
} finally {
  delete globalThis.__barTestState;
  await Promise.all(generated.map(url => unlink(url)));
}
