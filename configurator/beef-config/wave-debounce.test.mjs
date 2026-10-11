import assert from 'node:assert/strict';
import { replayDebounce } from './src/lib/types/wave-debounce.ts';
import { ChatterAutoTuner } from './src/lib/types/chatter-auto.ts';
const s = (timestamp, pressed, buttonIndex = 0) => ({timestamp, pressed, buttonIndex});
const settings = units => Array(7).fill(units);
const raw = [s(-1,false),s(0,true),s(2,false),s(3,true),s(1003,false)];
const original = structuredClone(raw);
assert.deepEqual(replayDebounce(raw,settings(0),settings(0)),raw);
for(const release of [0,12,50,160]) assert.deepEqual(replayDebounce(raw,settings(50),settings(release)),[raw[0],raw[1],raw[4]],'No delay on final release');
assert.deepEqual(raw,original);
for(let n=0;n<100000;n++) {
  const base=n/1000;
  const input=[s(base-1,false),s(base,true),s(base+1.6,false),s(base+2.7,true),s(base+52.7,false),s(base+54.7,true),s(base+57.7,false)];
  const out=replayDebounce(input,settings(0),settings(12));
  assert.deepEqual(out.map(edge=>edge.pressed),[false,true,false,true,false,true,false]);
  assert.equal(out[2].timestamp,base+1.6,'First OFF immediate');
  assert(Math.abs(out[3].timestamp-(base+2.8))<1e-9,'Sustained ON at lockout expiry');
  assert.equal(out[4].timestamp,base+52.7); assert.equal(out[6].timestamp,base+57.7);
}
for(let button=0;button<7;button++) for(const base of [.001,.1,50,1000,123456.789]) for(const gap of [1.1,1.2,1.3]) {
  const input=[s(base-1,false,button),s(base,true,button),s(base+1.6,false,button),s(base+1.6+gap,true,button),s(base+51.6+gap,false,button)];
  const out=replayDebounce(input,settings(0),settings(12));
  assert.equal(out[2].timestamp,input[2].timestamp);
  assert(Math.abs(out[3].timestamp-(base+1.6+Math.max(gap,1.2)))<1e-9);
  assert.equal(out.at(-1).timestamp,input.at(-1).timestamp);
}
assert.deepEqual(replayDebounce([s(-1,false),s(0,true),s(1,false)],settings(50),settings(12)),[s(-1,false),s(0,true),s(5,false)],'Only reverse OFF during press lockout is deferred');
assert.deepEqual(replayDebounce([s(-1,true),s(0,false),s(1.1,true),s(101.1,false)],settings(0),settings(12)),[s(-1,true),s(0,false),s(1.2,true),s(101.1,false)]);
assert.deepEqual(replayDebounce([s(-1,false),s(0,true),s(3,false)],settings(50),settings(12),[{start:1,end:2}]),[s(-1,false),s(0,true),s(3,false)]);
assert.deepEqual(replayDebounce([s(-1,false),s(0,true),s(10,false),s(11,true),s(11.2,false)],settings(0),settings(12)),[s(-1,false),s(0,true),s(10,false)],'Bounce ending at expiry never emits a phantom ON');
console.log('PASS immediate edges: 100000 offsets, B1-B7 boundaries, exact 1003ms, sustained ON, zero, gaps, immutable input');
for (let button=0;button<7;button++) for (const pressed of [false,true]) {
  const tuner=new ChatterAutoTuner();
  const input=[s(-1,!pressed,button),s(0,pressed,button),s(1.3,!pressed,button),s(3.7,pressed,button),s(100,!pressed,button)];
  assert.equal(tuner.observe(button,true,false,0,0),undefined);
  assert.equal(tuner.observe(button,false,true,13,0),14);
  const recommendation=tuner.observe(button,false,false,37,14);
  assert.equal(recommendation,38);
  assert.deepEqual(replayDebounce(input,settings(pressed?recommendation:0),settings(pressed?0:recommendation)),[input[0],input[1],input[4]]);
  assert.equal(tuner.observe(button,true,false,0,recommendation),undefined);
  assert.equal(tuner.observe(button,false,false,13,recommendation),undefined);
}
const tuner=new ChatterAutoTuner();
assert.equal(tuner.observe(0,false,true,13,50),50);
assert.equal(tuner.observe(1,false,false,26,0),undefined);
assert.equal(tuner.observe(0,false,false,159,50),160);
tuner.reset();
assert.equal(tuner.observe(0,false,false,26,0),undefined);
console.log('PASS automatic tuning: press/release recovery, B1-B7, normal phases, gaps and bounds');
