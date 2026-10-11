import assert from 'node:assert/strict';
import { readFile, writeFile, unlink } from 'node:fs/promises';
import ts from 'typescript';
import { compileModule } from 'svelte/compiler';

const temporary = new URL('./.connection-state.generated.mjs', import.meta.url);
const navigatorDescriptor = Object.getOwnPropertyDescriptor(globalThis, 'navigator');
const listeners = new Set();
let resolveInfo;
let info;
class Device { async close() {} }
const device = new Device();
globalThis.__connectionTest = {
  detectDevice: async () => device,
  readBoardInfo: () => info,
  restartAndWaitForReconnection: async () => {},
  waitForReconnection: async () => {}
};
Object.defineProperty(globalThis, 'navigator', { configurable: true, value: {
  hid: {
    addEventListener: (_, listener) => listeners.add(listener),
    removeEventListener: (_, listener) => listeners.delete(listener)
  }
} });
try {
  let source = await readFile(new URL('./src/lib/types/state.svelte.ts', import.meta.url), 'utf8');
  source = source.replace(/import \{ DfuDevice \}[^\n]+/, 'const DfuDevice = {};')
    .replace(/import \{ detectDevice[^\n]+/, 'const { detectDevice, readBoardInfo, restartAndWaitForReconnection, waitForReconnection } = globalThis.__connectionTest;');
  const js = ts.transpileModule(source, { compilerOptions: { target: ts.ScriptTarget.ESNext, module: ts.ModuleKind.ESNext } }).outputText;
  await writeFile(temporary, compileModule(js, { filename: 'state.svelte.js', generate: 'client' }).js.code);
  const { appState, connectDevice, onDisconnect } = await import(temporary.href);

  info = new Promise(resolve => { resolveInfo = resolve; });
  const connecting = connectDevice();
  await Promise.resolve();
  assert.equal(appState.device, device);
  assert.equal(listeners.size, 1, 'Disconnect must be watched while board detection is pending');
  await Promise.all([...listeners].map(listener => listener({ device })));
  resolveInfo({ mcu: 'AT90USB1286' });
  await connecting;
  assert.equal(appState.device, null);
  assert.equal(appState.boardInfo, null, 'A late board report must not restore disconnected information');
  assert.equal(listeners.size, 0);

  info = new Promise(resolve => { resolveInfo = resolve; });
  const anotherConnection = connectDevice();
  await Promise.resolve();
  await onDisconnect({ device });
  const replacement = new Device();
  appState.device = replacement;
  appState.boardInfo = { mcu: 'RP2354B' };
  resolveInfo({ mcu: 'AT90USB1286' });
  await anotherConnection;
  assert.equal(appState.device, replacement);
  assert.equal(appState.boardInfo.mcu, 'RP2354B', 'Old reports must not overwrite a replacement board');
  await onDisconnect();

  info = Promise.resolve({ mcu: 'AT90USB1286' });
  await connectDevice();
  assert.equal(appState.boardInfo.mcu, 'AT90USB1286');
  assert.equal(listeners.size, 1);
  await onDisconnect();
  console.log('PASS connection state: disconnect during detection, stale report, replacement board and normal connection');
} finally {
  if (navigatorDescriptor) Object.defineProperty(globalThis, 'navigator', navigatorDescriptor);
  else delete globalThis.navigator;
  delete globalThis.__connectionTest;
  await unlink(temporary).catch(() => {});
}
