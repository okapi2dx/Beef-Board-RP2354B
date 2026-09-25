import assert from 'node:assert/strict';
import {createLedEmulator} from './src/lib/led-emulator.js';
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
