import assert from 'node:assert/strict';
import { featureReportPayload } from './src/lib/types/feature-report.ts';

function bytes(view) {
  return Array.from(new Uint8Array(view.buffer, view.byteOffset, view.byteLength));
}

const config = Uint8Array.from([33, 0, 8]);
assert.deepEqual(bytes(featureReportPayload(new DataView(config.buffer), 1)), [33, 0, 8]);
const configWithId = Uint8Array.from([1, ...config]);
assert.deepEqual(bytes(featureReportPayload(new DataView(configWithId.buffer), 1)), [33, 0, 8]);

const version = Uint8Array.from([0, 0, 0, 0, 0, 1, 0]);
for (const payload of [version, Uint8Array.from([3, ...version])]) {
  assert.deepEqual(bytes(featureReportPayload(new DataView(payload.buffer), 3, [7])), [...version]);
}

// A diagnostics payload can itself start with 4 (the report ID). Length must
// decide whether that byte is data or a prefix.
for (const size of [13, 24]) {
  const diagnostics = new Uint8Array(size);
  diagnostics[0] = 4;
  diagnostics[size - 1] = 99;
  for (const payload of [diagnostics, Uint8Array.from([4, ...diagnostics])]) {
    assert.deepEqual(bytes(featureReportPayload(new DataView(payload.buffer), 4, [13, 24])), [...diagnostics]);
  }
}

const backing = Uint8Array.from([255, 3, ...version, 255]);
assert.deepEqual(bytes(featureReportPayload(new DataView(backing.buffer, 1, 8), 3, [7])), [...version]);
assert.throws(() => featureReportPayload(new DataView(new Uint8Array([3, 0]).buffer), 3, [7]));
console.log('PASS: feature reports with and without report-ID prefixes');
