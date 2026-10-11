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
 const {waitForReconnection,readBoardInfo,readDiagnostics,readFirmwareInfo,parseDiagnostics,parseChatterCounts}=await import(await moduleFile('hid',source));
 const info=Uint8Array.from([0x42,0x45,0x45,0x46,1,3,2,0,63,0,4]);
 const device={vendorId:0x1ccf,productId:0x1018,productName:'BEEF BOARD',opened:true,
  collections:[{usagePage:0xffeb,usage:1}], receiveFeatureReport:async id=>new DataView((id===6?info:new Uint8Array(139)).buffer)};
 let requests=0,granted=false;
 Object.defineProperty(globalThis,'navigator',{configurable:true,value:{hid:{getDevices:async()=>granted?[device]:[],addEventListener(){},requestDevice:async()=>{
  throw new Error('Desktop reconnect must use a fresh user gesture');
 }}}});
 globalThis.window={beefNative:{authorizeHidIdentity:async identity=>{
  requests++;assert.deepEqual(identity,{vendorId:0x1ccf,productId:0x1018});
  if(requests===2)granted=true;
  return granted?1:0;
 }}};
 let now=0;Date.now=()=>now;
 globalThis.setTimeout=(callback,delay=0)=>originalTimer(()=>{now+=Number(delay)||0;callback();},0);
 await waitForReconnection(5000,{vendorId:0x1ccf,productId:0x1018});
 assert.equal(requests,2);assert.equal(state.device,device);

 // Resetting legacy AT90 configuration restores the default USB identity.
 // Desktop reconnection must authorize the post-reset VID/PID without a HID chooser.
 const defaultDevice={...device,vendorId:0xfeed,productId:0x0000,productName:'BEEF BOARD'};
 requests=0;granted=false;state.device=device;
 globalThis.window={beefNative:{authorizeHidIdentity:async identity=>{
  requests++;
  if(identity.vendorId===0xfeed&&identity.productId===0x0000)granted=true;
  return granted?1:0;
 }}};
 Object.defineProperty(globalThis,'navigator',{configurable:true,value:{hid:{getDevices:async()=>granted?[defaultDevice]:[],addEventListener(){},requestDevice:async()=>{
  throw new Error('Desktop reset reconnect must not require a HID chooser');
 }}}});
 await waitForReconnection(5000,[{vendorId:0x1ccf,productId:0x1018},{vendorId:0xfeed,productId:0x0000}]);
 assert.equal(requests,2);assert.equal(state.device,defaultDevice);

 // RP2354B also returns to its board-specific default identity after reset.
 // When Board Info is unavailable on an IIDX identity, reconnection must still
 // be able to authorize 0xFEED:0x0003 instead of assuming the legacy AT90 ID.
 const rpDefaultDevice={...device,vendorId:0xfeed,productId:0x0003,productName:'BEEF BOARD RP2354B'};
 requests=0;granted=false;state.device=device;
 globalThis.window={beefNative:{authorizeHidIdentity:async identity=>{
  requests++;
  if(identity.vendorId===0xfeed&&identity.productId===0x0003)granted=true;
  return granted?1:0;
 }}};
 Object.defineProperty(globalThis,'navigator',{configurable:true,value:{hid:{getDevices:async()=>granted?[rpDefaultDevice]:[],addEventListener(){},requestDevice:async()=>{
  throw new Error('Desktop RP reset reconnect must not require a HID chooser');
 }}}});
 await waitForReconnection(8000,[{vendorId:0x1ccf,productId:0x1018},{vendorId:0xfeed,productId:0x0000},{vendorId:0xfeed,productId:0x0003}]);
 assert.equal(requests,3);assert.equal(state.device,rpDefaultDevice);

 // A reset while already in default mode must wait for a new USB instance.
 // The old authorized handle can still answer reports before the reboot.
 const oldDefault={...defaultDevice};
 const newDefault={...defaultDevice};
 let polls=0;
 state.device=oldDefault;
 globalThis.window={beefNative:{authorizeHidIdentity:async()=>0}};
 Object.defineProperty(globalThis,'navigator',{configurable:true,value:{hid:{getDevices:async()=>++polls<3?[oldDefault]:[oldDefault,newDefault],addEventListener(){},requestDevice:async()=>[]}}});
 await waitForReconnection(5000,{vendorId:0xfeed,productId:0x0000},oldDefault);
 assert.ok(polls>=3);assert.equal(state.device,newDefault);

 // The RP config size identifies the board when Windows rejects Board Info
 // under the IIDX identity shared by both boards.
 const rpIidx={...device,receiveFeatureReport:async id=>{
  if(id===6)throw new Error('Board Info unavailable');
  return new DataView(new Uint8Array(139).buffer);
 }};
 assert.equal((await readBoardInfo(rpIidx)).boardType,'RP2354B');
 const at90Iidx={...device,receiveFeatureReport:async id=>{
  if(id===6)throw new Error('Board Info unavailable');
  return new DataView(Uint8Array.from([1,31,...new Uint8Array(137)]).buffer);
 }};
 assert.equal((await readBoardInfo(at90Iidx)).boardType,'AT90USB1286');

 // A stale HID handle may never settle after USB re-enumeration. Reconnect
 // must skip it instead of getting stuck forever before the overall timeout.
 const staleDevice={...defaultDevice,opened:true,receiveFeatureReport:async()=>new Promise(()=>{}),close:async()=>new Promise(()=>{})};
 granted=true;state.device=null;
 Object.defineProperty(globalThis,'navigator',{configurable:true,value:{hid:{getDevices:async()=>[staleDevice,defaultDevice],addEventListener(){},requestDevice:async()=>[]}}});
 await waitForReconnection(5000,[{vendorId:0xfeed,productId:0x0000}]);
 assert.equal(state.device,defaultDevice);

 // Windows/Electron can leave discovery or the HID chooser unresolved. The
 // connection attempt must end with an error instead of showing Connecting…
 // forever, so the user can retry.
 state.device=null;globalThis.window={beefNative:{authorizeHidIdentity:async()=>new Promise(()=>{})}};
 Object.defineProperty(globalThis,'navigator',{configurable:true,value:{hid:{getDevices:async()=>[],addEventListener(){},requestDevice:async()=>[]}}});
 await assert.rejects(waitForReconnection(5000,{vendorId:0x1ccf,productId:0x1018}),/timed out/);
 globalThis.window={};
 Object.defineProperty(globalThis,'navigator',{configurable:true,value:{hid:{getDevices:async()=>new Promise(()=>{}),addEventListener(){}}}});
 await assert.rejects(waitForReconnection(5000),/timed out/);

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
 const chatterDiagnostics=new Uint8Array(62);
 chatterDiagnostics[24]=0x34;chatterDiagnostics[25]=0x12;
 chatterDiagnostics[48]=5;chatterDiagnostics[50]=0x80;
 for(const prefix of [false,true]) {
  state.device={receiveFeatureReport:async()=>new DataView((prefix?Uint8Array.from([4,...chatterDiagnostics]):chatterDiagnostics).buffer)};
  const report=await readDiagnostics();
  assert.equal(report.chatterEdges.length,11);
  assert.equal(report.chatterEdges[0],0x1234);
  assert.equal(report.edgeSequence,5);
  assert.equal(report.edgeEvents[0],0x80);
 }
 const extendedDiagnostics=new Uint8Array(60);
 const extendedView=new DataView(extendedDiagnostics.buffer);
 extendedView.setUint16(24,0x1234,true);
 extendedView.setUint16(38,7,true);
 extendedView.setUint16(40,5,true);
 extendedDiagnostics[42]=0x80;
 extendedView.setUint16(43,9999,true);
 for(const prefix of [false,true]) {
  state.device={receiveFeatureReport:async()=>new DataView((prefix?Uint8Array.from([4,...extendedDiagnostics]):extendedDiagnostics).buffer)};
  const report=await readDiagnostics();
  assert.equal(report.chatterEdges.length,7);
  assert.equal(report.chatterEdges[0],0x1234);
  assert.equal(report.rawButtons,7);
  assert.equal(report.edgeSequence,5);
  assert.equal(report.edgeEvents[0],0x80);
  assert.equal(report.edgeEvents[1] | (report.edgeEvents[2]<<8),9999);
 }
 {
  const packet = new DataView(new ArrayBuffer(60));
  packet.setUint16(43,0xc002,true);
  packet.setUint16(46,0x4002,true);
  packet.setUint16(49,2,true);
  const parsed=parseDiagnostics(packet);
  assert.deepEqual(parsed.edgeDetected.slice(0,3),[true,false,undefined]);
  for(const slot of [0,3,6]) assert.equal(parsed.edgeEvents[slot+1] | parsed.edgeEvents[slot+2]<<8,2);
 }
 assert.deepEqual(parseDiagnostics(new DataView(new Uint8Array(24).buffer)).chatterEdges,[]);
 const rgbStream=new Uint8Array(63);
 rgbStream.set(extendedDiagnostics);
 rgbStream.set([12,34,56],60);
 const rgbReport=parseDiagnostics(new DataView(rgbStream.buffer));
 assert.deepEqual(rgbReport.ttRgb,[12,34,56]);
 assert.equal(rgbReport.chatterEdges[0],0x1234);
 assert.equal(rgbReport.rawButtons,7);
 assert.equal(rgbReport.edgeSequence,5);
 assert.equal(rgbReport.edgeEvents.length,18);
 assert.equal(parseDiagnostics(new DataView(extendedDiagnostics.buffer)).ttRgb,undefined);
 assert.equal(parseDiagnostics(new DataView(chatterDiagnostics.buffer)).ttRgb,undefined);
 const counterBytes=new Uint8Array(28);
 const counterView=new DataView(counterBytes.buffer);
 counterView.setUint16(0,1234,true);counterView.setUint16(14,12,true);
 assert.equal(parseChatterCounts(counterView).edges[0],1234);
 assert.equal(parseChatterCounts(counterView).detected[0],12);
 const paddedCounterBytes=new Uint8Array(60);
 paddedCounterBytes.set(counterBytes);
 assert.equal(parseChatterCounts(new DataView(paddedCounterBytes.buffer)).edges[0],1234);
 assert.equal(parseChatterCounts(new DataView(paddedCounterBytes.buffer)).detected[0],12);
 assert.throws(()=>parseChatterCounts(new DataView(new Uint8Array(27).buffer)));
 // Removal during the final board report must retry rather than return a
 // disconnected handle as a successful reconnection.
 let removalListener, boardReads=0;
 const disappearingDevice={...defaultDevice,receiveFeatureReport:async id=>{
  if(id===6 && ++boardReads===2) await removalListener({device:disappearingDevice});
  return new DataView((id===6?info:new Uint8Array(138)).buffer);
 }};
 const survivingDevice={...defaultDevice};
 Object.defineProperty(globalThis,'navigator',{configurable:true,value:{hid:{
  getDevices:async()=>boardReads>=2?[survivingDevice]:[disappearingDevice],
  addEventListener:(_,listener)=>{removalListener=listener;}
 }}});
 globalThis.window={};
 await waitForReconnection(5000, {vendorId:0xfeed,productId:0x0000});
 assert.equal(state.device,survivingDevice,'Reconnect must retry after removal during board detection');
 console.log('PASS: delayed USB identity permission/reconnect; diagnostic/version layouts; legacy LED telemetry');
} finally {
 Date.now=originalNow;globalThis.setTimeout=originalTimer;globalThis.window=originalWindow;
 if(navigatorDescriptor)Object.defineProperty(globalThis,'navigator',navigatorDescriptor);else delete globalThis.navigator;
 delete globalThis.__hidState;await Promise.all(generated.map(url=>unlink(url)));
}
