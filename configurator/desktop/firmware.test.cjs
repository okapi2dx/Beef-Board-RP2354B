const { test } = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const ts = require('../beef-config/node_modules/typescript');
const source = fs.readFileSync(require('node:path').join(__dirname, '../beef-config/src/lib/types/dfu.ts'), 'utf8');
const code = ts.transpile(source.slice(source.indexOf('function convertMapToFixedLength')), { target: ts.ScriptTarget.ES2022 });
const convert = new Function(code + '; return convertMapToFixedLength;')();
test('DFU chunks preserve sparse addresses, exact bytes and transfer size', () => {
  const bytes = new Uint8Array(1041).fill(0x5a);
  const result = convert(new Map([[0x2000, bytes], [0, new Uint8Array([1, 2, 3])]]), 1024);
  assert.deepEqual([...result.keys()], [0, 0x2000, 0x2400]);
  assert.deepEqual([...result.get(0)], [1, 2, 3]);
  assert.equal(result.get(0x2400).length, 17);
  assert.equal(result.get(0x2000).length, 1024);
});
test('unsupported address ranges fail before erase', () => {
  assert.throws(() => convert(new Map([[0xffff, new Uint8Array(2)]]), 1024));
  assert.ok(source.indexOf('const fixedFirmware =') < source.indexOf('await this.eraseFullChip()'));
});
