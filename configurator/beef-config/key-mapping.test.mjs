import assert from 'node:assert/strict';
import { readFile, writeFile, unlink } from 'node:fs/promises';
import ts from 'typescript';
import { compileModule } from 'svelte/compiler';
import { effect_root, render_effect } from 'svelte/internal/client';
import { flushSync } from 'svelte';

// Exercise the real mapping classes with Svelte's reactive runtime. Class
// instances are not deeply proxied by the enclosing Config.$state field.
const source = await readFile(new URL('./src/lib/types/config.svelte.ts', import.meta.url), 'utf8');
const hid = await readFile(new URL('./src/lib/types/hid-codes.ts', import.meta.url), 'utf8');
const classes = source.slice(source.indexOf('export class IIDXKeyMapping'), source.indexOf('export class Config'));
const hidJs = ts.transpileModule(hid, { compilerOptions: { target: ts.ScriptTarget.ESNext, module: ts.ModuleKind.ESNext } }).outputText;
const input = `import * as HIDCodes from 'data:text/javascript;base64,${Buffer.from(hidJs).toString('base64')}';\n${classes}`;
const js = ts.transpileModule(input, { compilerOptions: { target: ts.ScriptTarget.ESNext, module: ts.ModuleKind.ESNext } }).outputText;
const compiled = compileModule(js, { filename: 'key-mapping.svelte.js', generate: 'client' }).js.code;
const temporary = new URL('./.key-mapping-test.generated.mjs', import.meta.url);
try {
  await writeFile(temporary, compiled);
  const { IIDXKeyMapping, SDVXKeyMapping } = await import(temporary.href);
  for (const [Mapping, changes] of [
    [IIDXKeyMapping, [['main_buttons', 0], ['function_buttons', 3], ['tt_ccw'], ['tt_cw']]],
    [SDVXKeyMapping, [['bt_buttons', 0], ['fx_buttons', 1], ['start']]]
  ]) {
    const mapping = new Mapping();
    let observed;
    let runs = 0;
    const dispose = effect_root(() => render_effect(() => {
      observed = changes.map(([field, index]) => index === undefined ? mapping[field] : mapping[field][index]);
      runs++;
    }));
    try {
      for (const [position, [field, index]] of changes.entries()) {
        const before = runs;
        if (index === undefined) mapping[field] = 4;
        else mapping[field][index] = 4;
        flushSync();
        assert.equal(runs, before + 1, `${Mapping.name}.${field} must notify auto-save`);
        assert.equal(observed[position], 4);
      }
    } finally { dispose(); }
  }
  console.log('PASS: all seven key mapping fields notify reactive auto-save');
} finally { await unlink(temporary).catch(() => {}); }
