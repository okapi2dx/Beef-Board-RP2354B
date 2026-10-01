import assert from 'node:assert/strict';
import { WaveClock } from './src/lib/types/wave-history.ts';

const clock = new WaveClock(1000);
assert.equal(clock.advance(16000), 0, 'initial idle time must not enter the waveform');
assert.equal(clock.record(16000, 1, true), 0);
assert.equal(clock.advance(16100), 100, 'a held button records its full duration');
assert.equal(clock.record(16100, 0, true), 100);
assert.equal(clock.advance(31100), 350, 'a long released interval adds only the short context tail');
assert.equal(clock.record(31100, 1, true), 350, 'the next input resumes at the frozen wave time');
assert.equal(clock.advance(31200), 450);
assert.equal(clock.record(31200, 0, true), 450);
assert.equal(clock.advance(40000), 700);
console.log('PASS waveform activity clock');
