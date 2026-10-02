import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import vm from 'node:vm';
import ts from 'typescript';

// Exercise the component's actual event handlers with a deterministic clock.
const source = await readFile(new URL('./src/lib/ColorPicker.svelte', import.meta.url), 'utf8');
assert.match(source, /onInput=\{colorChanged\}/);
const script = source.match(/<script lang="ts">([\s\S]*?)<\/script>/)[1]
  .replace(/^\s*import .*;\s*$/gm, '');
let destroy;
let nextId = 0;
const pending = new Map();
const saved = { h: 0, s: 0, v: 0 };
const props = () => ({ hsv: saved });
props.id = () => 'test-picker';
const context = vm.createContext({
  $props: props, $state: value => value, $bindable: () => undefined,
  onDestroy: callback => { destroy = callback; },
  setTimeout: callback => { pending.set(++nextId, callback); return nextId; },
  clearTimeout: id => pending.delete(id)
});
const compiled = ts.transpileModule(script, { compilerOptions: { target: ts.ScriptTarget.ESNext } }).outputText;
vm.runInContext(compiled, context);
const edit = (h, s, v) => vm.runInContext(`hsva = {h:${h},s:${s},v:${v},a:1}; colorChanged();`, context);
edit(120, 50, 60); // A keyboard/text event, without pointerdown or pointerup.
assert.equal(saved.h, 0);
edit(240, 75, 80);
assert.equal(pending.size, 1, 'Repeated edits must coalesce');
[...pending.values()][0]();
assert.deepEqual(saved, { h: 240, s: 75, v: 80 });
assert.equal(pending.size, 0);
edit(60, 20, 30);
destroy(); // Switching tabs must preserve the final edit.
assert.deepEqual(saved, { h: 60, s: 20, v: 30 });
assert.equal(pending.size, 0);
console.log('PASS color input: keyboard/text changes, coalesced writes and final edit on unmount');
