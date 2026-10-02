<script lang="ts">
	import ColorPicker from 'svelte-awesome-color-picker';
	import { onDestroy } from 'svelte';
	import { mode } from 'mode-watcher';

	import { Hsv } from '$lib/types/types.svelte';

	interface Props {
		hsv: Hsv;
	}

	let { hsv = $bindable() }: Props = $props();

	let hsva = $state({
		h: hsv.h,
		s: hsv.s,
		v: hsv.v,
		a: 1
	});

	let saveTimer: ReturnType<typeof setTimeout> | undefined;
	function commitColor(): void {
		if (saveTimer === undefined) return;
		clearTimeout(saveTimer);
		saveTimer = undefined;
		hsv.h = hsva.h;
		hsv.s = hsva.s;
		hsv.v = hsva.v;
	}
	// Mouse, touch, keyboard and text edits all produce this event.
	// Coalesce continuous edits before writing the board's EEPROM.
	function colorChanged(): void {
		clearTimeout(saveTimer);
		saveTimer = setTimeout(commitColor, 150);
	}
	onDestroy(commitColor);
</script>

<div
	class="mb-4"
	class:cp-dark={mode.current === 'dark'}
	role="none"
>
	<ColorPicker position="responsive" isAlpha={false} bind:hsv={hsva} onInput={colorChanged} />
</div>

<style>
	.cp-dark {
		/* Comes from the dark class in app.css */
		--cp-bg-color: hsl(222.2 84% 4.9%);
		--cp-border-color: hsl(217.2 32.6% 17.5%);
		--cp-text-color: 217.2 32.6% 17.5%;
		--cp-input-color: hsl(222.2 47.4% 11.2%);
		--cp-button-hover-color: hsl(217.2 32.6% 17.5%);
	}
</style>
