import assert from 'node:assert/strict';
import { measureWaveInterval } from './src/lib/types/wave-measure.ts';
const samples = [
  {timestamp:-1,pressed:false}, {timestamp:0,pressed:true},
  {timestamp:1.6,pressed:false}, {timestamp:2.7,pressed:true},
  {timestamp:52.7,pressed:false}
];
assert.deepEqual(measureWaveInterval(samples,0,1.6),{total:1.6,pressed:1.6,released:0});
const release=measureWaveInterval(samples,2.7,1.6);
assert(Math.abs(release.released-1.1)<1e-9);
assert.equal(release.pressed,0);
const mixed=measureWaveInterval(samples,52.7,0);
assert(Math.abs(mixed.pressed-51.6)<1e-9);
assert(Math.abs(mixed.released-1.1)<1e-9);
assert.equal(measureWaveInterval(samples,0,0),undefined);
assert.equal(measureWaveInterval(samples,0,2),undefined);
assert.equal(measureWaveInterval(samples,0,52.7,[{start:2,end:3}]),undefined);
assert.equal(measureWaveInterval(samples,0,1.6,[{start:1.6,end:3}]).total,1.6);
assert.equal(measureWaveInterval([{timestamp:-1,pressed:false},{timestamp:0,pressed:true},{timestamp:52.7,pressed:false}],0,2.7),undefined);
console.log('PASS waveform interval: press, release, mixed, reverse order, gaps and removed edges');
