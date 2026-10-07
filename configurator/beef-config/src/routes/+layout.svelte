<script lang="ts">
	import '../app.css';
	import '../equipment-theme.css';
	import '../simple-theme.css';
	import '../panel-theme.css';
	import '../blue-line-theme.css';
	import { onMount } from 'svelte';
	import { ModeWatcher, setMode } from 'mode-watcher';

	interface Props {
		children?: import('svelte').Snippet;
	}

	let { children }: Props = $props();
	const design = import.meta.env.VITE_BEEF_DESIGN === 'simple' ? 'simple' : import.meta.env.VITE_BEEF_DESIGN === 'panel' ? 'panel' : import.meta.env.VITE_BEEF_DESIGN === 'equipment' ? 'equipment' : 'blue-line';
	onMount(() => {
		document.documentElement.dataset.design = design;
		const preferenceKey = `beef-${design}-design`;
		if (!localStorage.getItem(preferenceKey) || localStorage.getItem('beef-active-design') !== design) {
			setMode(design === 'simple' ? 'light' : 'dark');
			localStorage.setItem(preferenceKey, '1');
		}
		localStorage.setItem('beef-active-design', design);
	});
</script>

<ModeWatcher />
{@render children?.()}
