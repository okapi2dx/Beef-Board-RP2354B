<script lang="ts">
	import ColorPicker from 'svelte-awesome-color-picker';
	import { onDestroy } from 'svelte';
	import { mode } from 'mode-watcher';

	import { Hsv } from '$lib/types/types.svelte';
	import { tr } from '$lib/types/locale.svelte';
	import SliderInput from '$lib/SliderInput.svelte';

	interface Props {
		hsv: Hsv;
		components?: 'h' | 'hs' | 'sv' | 'hsv';
	}

	let { hsv = $bindable(), components = 'hsv' }: Props = $props();

	const pickerId = $props.id();
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

{#if components !== 'hsv'}
	<div class="mb-4">
		{#if components.includes('h')}
			<label>{tr('色相', 'Hue')}<SliderInput bind:value={hsv.h} min={0} max={360} id={`${pickerId}-h`} /></label>
		{/if}
		{#if components.includes('s')}
			<label>{tr('彩度', 'Saturation')}<SliderInput bind:value={hsv.s} min={0} max={100} id={`${pickerId}-s`} /></label>
		{/if}
		{#if components.includes('v')}
			<label>{tr('明るさ', 'Brightness')}<SliderInput bind:value={hsv.v} min={0} max={100} id={`${pickerId}-v`} /></label>
		{/if}
	</div>
{:else}
<div
	class="mb-4"
	class:cp-dark={mode.current === 'dark'}
	role="none"
>
	<ColorPicker position="responsive" isAlpha={false} bind:hsv={hsva} onInput={colorChanged} />
</div>
{/if}

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
