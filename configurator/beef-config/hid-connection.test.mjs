import assert from 'node:assert/strict';
import {readFile, writeFile, unlink} from 'node:fs/promises';
import ts from 'typescript';
const generated=[];
async function moduleFile(name, source) {
 const url=new URL(`./.hid-test-${name}.mjs`,import.meta.url);
 await writeFile(url,ts.transpileModule(source,{compilerOptions:{target:ts.ScriptTarget.ESNext,module:ts.ModuleKind.ESNext}}).outputText);
 generated.push(url);return url.href;
}
const originalNow=Date.now, originalTimer=globalThis.setTimeout;
const navigatorDescriptor=Object.getOwnPropertyDescriptor(globalThis,'navigator');
const originalWindow=globalThis.window;
try {
 const board=await moduleFile('board',await readFile(new URL('./src/lib/types/board.ts',import.meta.url),'utf8'));
 const feature=await moduleFile('feature',await readFile(new URL('./src/lib/types/feature-report.ts',import.meta.url),'utf8'));
 const state={device:null,boardInfo:null};globalThis.__hidState=state;
 let source=await readFile(new URL('./src/lib/types/hid.ts',import.meta.url),'utf8');
 source=source.replace("import { appState, onDisconnect } from '$lib/types/state.svelte';",'const appState=globalThis.__hidState; const onDisconnect=async()=>{appState.device=null;};')
  .replace('$lib/types/feature-report',feature).replace('$lib/types/board',board);
 const {waitForReconnection,readDiagnostics,readFirmwareInfo,parseDiagnostics}=await import(await moduleFile('hid',source));
 const info=Uint8Array.from([0x42,0x45,0x45,0x46,1,3,2,0,63,0,4]);
 const device={vendorId:0x1ccf,productId:0x1018,productName:'BEEF BOARD',opened:true,
  collections:[{usagePage:0xffeb,usage:1}], receiveFeatureReport:async id=>new DataView((id===6?info:new Uint8Array(139)).buffer)};
 let requests=0;
 Object.defineProperty(globalThis,'navigator',{configurable:true,value:{hid:{getDevices:async()=>[],addEventListener(){},requestDevice:async({filters})=>{
  requests++;assert.equal(filters[0].productId,0x1018);return [device];
 }}}});
 globalThis.window={beefNative:{}};
 let now=0;Date.now=()=>now;
 globalThis.setTimeout=callback=>{now+=250;queueMicrotask(callback);return 0;};
 await waitForReconnection(5000,{vendorId:0x1ccf,productId:0x1018});
 assert.equal(requests,1);assert.equal(state.device,device);
 const diagnostics=new Uint8Array(24);diagnostics[0]=4;diagnostics[13]=80;
 const version=Uint8Array.from([0,0,0,0,0,1,1]);
 for(const prefix of [false,true]) {
  state.device={receiveFeatureReport:async id=>{
   const bytes=id===4?diagnostics:version;
   return new DataView((prefix?Uint8Array.from([id,...bytes]):bytes).buffer);
  }};
  assert.equal((await readDiagnostics()).buttons,4);
  assert.equal((await readDiagnostics()).ledLevels[0],80);
  assert.equal((await readFirmwareInfo()).version,'0.1.1');
 }
 for (const bytes of [Uint8Array.from([3,0,0,0]), Uint8Array.from([3,3,0,0,0])]) {
  state.device={receiveFeatureReport:async()=>new DataView(bytes.buffer)};
  assert.equal((await readFirmwareInfo()).version,null);
 }
 assert.deepEqual(parseDiagnostics(new DataView(new Uint8Array(13).buffer)).ledLevels,[]);
 console.log('PASS: changed USB identity permission/reconnect; diagnostic/version layouts; legacy LED telemetry');
} finally {
 Date.now=originalNow;globalThis.setTimeout=originalTimer;globalThis.window=originalWindow;
 if(navigatorDescriptor)Object.defineProperty(globalThis,'navigator',navigatorDescriptor);else delete globalThis.navigator;
 delete globalThis.__hidState;await Promise.all(generated.map(url=>unlink(url)));
}
