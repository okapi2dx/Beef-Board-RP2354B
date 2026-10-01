import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import ts from 'typescript';

const source = await readFile(new URL('./src/lib/types/chatter-events.ts', import.meta.url), 'utf8');
const code = ts.transpileModule(source, { compilerOptions: { target: ts.ScriptTarget.ESNext, module: ts.ModuleKind.ESNext } }).outputText;
const { diagnosticEventSlots } = await import(`data:text/javascript;base64,${Buffer.from(code).toString('base64')}`);
assert.deepEqual(diagnosticEventSlots(5, 5), { slots: [], lost: false });
assert.deepEqual(diagnosticEventSlots(5, 8), { slots: [0, 3, 6], lost: false });
assert.deepEqual(diagnosticEventSlots(65535, 1), { slots: [0, 3], lost: false });
assert.equal(diagnosticEventSlots(0, 9).lost, true);
// Simulate the actual firmware ring across wrap. Every returned record must
// belong to the requested sequence range, in order, without duplicates.
for (const start of [12, 65528, 65530, 65534, 65535]) {
  for (let count = 1; count <= 10; count++) {
    const ring = Array(6).fill(-1);
    for (let n = 1; n <= count; n++) ring[((start + n) % 65536) % 6] = n;
    const result = diagnosticEventSlots(start, (start + count) % 65536);
    const records = result.slots.map(slot => ring[slot / 3]);
    assert.ok(records.every(n => n > 0 && n <= count));
    assert.deepEqual(records, [...new Set(records)].sort((a, b) => a - b));
    assert.equal(records.at(-1), count);
    if (records.length < count) assert.equal(result.lost, true);
  }
}
console.log('PASS chatter event ring: normal, missing packets, sequence wrap and overwritten records');
