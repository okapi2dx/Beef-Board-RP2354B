import assert from 'node:assert/strict';
import {createLedEmulator,ledRgbToCss} from './src/lib/led-emulator.js';
const c={h:180,s:100,v:100};
const cfg={tt_effect:'Spin',bar_effect:'Static',link_bar_effect:true,rainbow_spin_speed:1,tt_spin_hsv:c,tt_shift_hsv:c,tt_rainbow_spin_hsv:c,tt_rainbow_static_hsv:c,tt_rainbow_react_hsv:c,tt_breathing_hsv:c,tt_react_hsv:c,tt_static_hsv:c,bar_static_hsv:c};
for(const mode of ['Spin','Shift','Rainbow Spin','Rainbow Static','Rainbow Reactive','Breathing','Reactive']){
 cfg.tt_effect=mode;const tick=createLedEmulator();
 for(let i=0;i<2000;i++){
  const f=tick(cfg,{direction:i%3-1,buttons:i%8},17);
  assert.equal(f.ring.length,24);assert.equal(f.bar.length,16);
  f.bar.forEach((v,j)=>assert.equal(v,f.ring[mode === 'Rainbow Spin' || mode === 'Rainbow Reactive' ? 23-Math.floor(j*24/16) : Math.floor(j*24/16)]));
 }
}
cfg.disable_leds=true;assert(createLedEmulator()(cfg,{},16).bar.every(v=>v==='#000000'));
console.log('PASS: all linked effects stay synchronized across 34 seconds per effect; 24/16 pixels, disabled LEDs');

cfg.disable_leds=false;cfg.tt_effect='Rainbow Reactive';cfg.rainbow_spin_speed=3;
const smooth=createLedEmulator();
const hue=f=>Number(f.ring[0].match(/hsl\(([^ ]+)/)[1]);
const first=smooth(cfg,{direction:1},16);
const gap=smooth(cfg,{direction:0},16);
assert.notEqual(hue(first),hue(gap),'Short missing sample must keep moving');
for(let i=0;i<12;i++) smooth(cfg,{direction:0},16);
const stopped=smooth(cfg,{direction:0},16);
assert.deepEqual(smooth(cfg,{direction:0},16),stopped,'Motion must stop after 150ms');
const before=hue(smooth(cfg,{direction:1},16));
const after=hue(smooth(cfg,{direction:-1},16));
assert(((after-before+540)%360)-180<0,'Reverse input must reverse immediately');
const fallback=createLedEmulator(), explicit=createLedEmulator();
assert.deepEqual(fallback({...cfg,rainbow_spin_speed:undefined},{direction:1},16),explicit(cfg,{direction:1},16),'Default speed must equal 3');
console.log('PASS: gap smoothing, bounded stop, immediate reversal, default speed 3');

function motionStep(from,to){const e=createLedEmulator();e(cfg,{output:from,direction:0},50);const a=hue(e(cfg,{output:to,direction:0},50));return a;}
assert.equal(motionStep(255,0),motionStep(100,101),'Axis wrap must not create a speed spike');
assert.equal(motionStep(0,255),motionStep(100,99),'Reverse wrap must not create a speed spike');
assert.notEqual(motionStep(100,101),motionStep(100,108),'Speed must reflect position movement');
const local=createLedEmulator();let packet={output:20,direction:1};local(cfg,packet,50);packet={output:23,direction:1};local(cfg,packet,50);
for(let i=0;i<12;i++)local(cfg,packet,16);
assert.deepEqual(local(cfg,packet,16),local(cfg,packet,16),'Old packets must not keep motion alive');
console.log('PASS: position-based speed, bidirectional axis wrapping and stale-input stop');

const levels=[0,10,20,30,40,50,60,70,80,90,100];
assert.deepEqual(createLedEmulator()(cfg,{ledLevels:levels},16,true).buttonLevels,levels);
const individual={...cfg,button_led_individual:true,button_led_level:Array(11).fill(37),button_led_inverted:Array(11).fill(false),button_led_fade:Array(11).fill(0)};
assert.equal(createLedEmulator()(individual,{buttons:1},16).buttonLevels[0],37);
console.log('PASS: actual PWM telemetry and per-output LED preview');

// AVR hue ticks are whole steps every 3ms at rest, with no lost remainder.
const rainbowCfg={...cfg,tt_effect:'Rainbow Spin',rainbow_spin_speed:1};
const avr=createLedEmulator();
const phase0=hue(avr(rainbowCfg,{direction:0},0));
assert.equal(hue(avr(rainbowCfg,{direction:0},2)),phase0);
assert.notEqual(hue(avr(rainbowCfg,{direction:0},1)),phase0);
const combined=createLedEmulator(), split=createLedEmulator();
const expected=combined(rainbowCfg,{direction:0},10003);
let actual;
for(let i=0;i<10003;i++) actual=split(rainbowCfg,{direction:0},1);
assert.deepEqual(actual,expected,'Long display gaps must retain every elapsed hue tick');
const spinCfg={...cfg,tt_effect:'Spin',tt_leds:12};
const spinTick=createLedEmulator();
const beforeTick=spinTick(spinCfg,{direction:0},99);
assert.notDeepEqual(spinTick(spinCfg,{direction:0},1),beforeTick,'12 LEDs use a 100ms AVR spin interval');
const denseSpin=createLedEmulator();
for(let i=0;i<24;i++) {
  assert(denseSpin({...spinCfg,tt_leds:48},{direction:0},25).ring.some(c=>c!=='#000000'),
    'Downsampling rings larger than the preview must not hide a moving LED');
}
console.log('PASS: AVR hue tick boundaries, elapsed-time catch-up and LED-count spin periods');

// Exercise the monitor's actual animation effect with tab changes and a
// throttled animation clock. No USB calls are involved in the animation.
const {readFile}=await import('node:fs/promises');
const {default:vm}=await import('node:vm');
const {default:ts}=await import('typescript');
const source=await readFile(new URL('./src/lib/ControllerMonitor.svelte',import.meta.url),'utf8');
const effect=source.slice(source.indexOf('\t$effect(() => {\n\t\t// Reset'),source.indexOf('</script>'));
let startEffect, draw, emulatorCount=0;
const elapsed=[];
const context=vm.createContext({
  $effect:callback=>{startEffect=callback;}, appState:{device:{},boardInfo:{}},
  config:cfg,data:null,displayedButtons:0,active:true,frame:null,
  performance:{now:()=>0}, isRp2354bBoard:()=>true,
  createLedEmulator:()=>{emulatorCount++;return (config,data,dt)=>{elapsed.push(dt);return {time:dt};};},
  requestAnimationFrame:callback=>{draw=callback;return 1;}, cancelAnimationFrame:()=>{}
});
vm.runInContext(ts.transpileModule(effect,{compilerOptions:{target:ts.ScriptTarget.ESNext}}).outputText,context);
const dispose=startEffect();
draw(16);const visible=context.frame;
context.active=false;draw(1016);
assert.equal(context.frame,visible,'Inactive tab must avoid publishing frames');
context.active=true;draw(2016);
assert.deepEqual(elapsed,[16,1000,1000],'Clock must advance off-tab without a 50ms cap');
assert.equal(emulatorCount,1,'Changing tabs must retain the animation phase');
dispose();startEffect();
assert.equal(emulatorCount,2,'Reconnection must start a new local phase');
console.log('PASS: monitor tab switching, throttled display catch-up and connection reset');

for(const mode of ['Shift','HID','Static','Reactive','Breathing']) {
  const live=createLedEmulator();
  const liveCfg={...cfg,tt_effect:mode,link_bar_effect:true};
  const packet={ttRgb:[12,34,56]};
  const frame=live(liveCfg,packet,10000);
  assert(frame.ring.every(c=>c==='rgb(61 102 129)'));
  assert(frame.bar.every(c=>c==='rgb(61 102 129)'));
  packet.ttRgb=[200,0,17];
  assert(live(liveCfg,packet,16).ring.every(c=>c==='rgb(229 0 73)'),
    'Physical color must replace the locally estimated phase immediately');
  assert(live({...liveCfg,disable_leds:true},packet,16).ring.every(c=>c==='#000000'));
}
assert(createLedEmulator()({...cfg,tt_effect:'Rainbow Spin'},{ttRgb:[12,34,56]},16).ring.some(c=>c!=='rgb(61 102 129)'),
  'One sampled pixel must not flatten non-uniform effects');
console.log('PASS: physical RGB for uniform effects, linked bar, disabled output and rainbow isolation');

assert.equal(ledRgbToCss([0,0,0]),'rgb(0 0 0)');
assert.equal(ledRgbToCss([255,255,255]),'rgb(255 255 255)');
assert.equal(ledRgbToCss([255,128,0]),'rgb(255 188 0)','Orange midpoint must use display-encoded intensity');
assert.equal(ledRgbToCss([0,128,0]),'rgb(0 188 0)','Green midpoint must not be interpreted as encoded dark green');
const raw=[255,128,0];ledRgbToCss(raw);assert.deepEqual(raw,[255,128,0]);
let previous=-1;
for(let i=0;i<=255;i++) {
  const displayed=Number(ledRgbToCss([i,0,0]).match(/rgb\((\d+)/)[1]);
  assert(displayed>=previous && displayed<=255);
  previous=displayed;
}
console.log('PASS: display gamma for orange/green, black/white endpoints, monotonic intensity and immutable telemetry');
