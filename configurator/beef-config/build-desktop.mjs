import { build } from 'vite';
import { svelte } from '@sveltejs/vite-plugin-svelte';
import tailwindcss from '@tailwindcss/vite';
import path from 'node:path';
import fs from 'node:fs';
import { fileURLToPath } from 'node:url';
const root = path.dirname(fileURLToPath(import.meta.url));
// The desktop has one screen and needs only client assets, not a SvelteKit server build.
await build({
  configFile: false,
  root: path.join(root, 'desktop-web'),
  plugins: [svelte({ configFile: false }), tailwindcss()],
  resolve: { alias: { $lib: path.join(root, 'src/lib'), '@': path.join(root, 'src/lib') } },
  build: { outDir: path.join(root, 'build'), emptyOutDir: true }
});
fs.cpSync(path.join(root, 'static'), path.join(root, 'build'), { recursive: true });
