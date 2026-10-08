<script lang="ts">
	import '../app.css';
	import '../equipment-theme.css';
	import '../simple-theme.css';
	import '../panel-theme.css';
	import '../blue-line-theme.css';
	import '../paper-theme.css';
	import '../instrument-theme.css';
	import { onMount } from 'svelte';
	import { ModeWatcher, setMode } from 'mode-watcher';

	interface Props {
		children?: import('svelte').Snippet;
	}

	let { children }: Props = $props();
	const requestedDesign = import.meta.env.VITE_BEEF_DESIGN;
	const design = ['simple', 'panel', 'equipment', 'blue-line', 'paper', 'instrument'].includes(requestedDesign ?? '') ? requestedDesign! : 'blue-line';
	onMount(() => {
		document.documentElement.dataset.design = design;
		const preferenceKey = `beef-${design}-design`;
		if (!localStorage.getItem(preferenceKey) || localStorage.getItem('beef-active-design') !== design) {
			setMode(design === 'simple' || design === 'paper' || design === 'instrument' ? 'light' : 'dark');
			localStorage.setItem(preferenceKey, '1');
		}
		localStorage.setItem('beef-active-design', design);
	});
</script>

<ModeWatcher />
{@render children?.()}
