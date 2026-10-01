<script lang="ts">
	import { Input } from '$lib/components/ui/input';
	import { Slider } from '$lib/components/ui/slider';

	interface Props {
		value: number;
		min: number;
		max: number;
		id: string;
		reversed?: boolean;
		onValueChange?: (value: number) => void;
		compact?: boolean;
		step?: number;
	}

	let { value = $bindable(), min, max, id, reversed = false, onValueChange, compact = false, step = 1 }: Props = $props();

	let displayedValue = $derived(reversed ? min + max - value : value);
	function change(next: number) {
		if (!Number.isFinite(next)) return;
		const bounded = Math.max(min, Math.min(max, Math.round((next - min) / step) * step + min));
		value = Math.round((reversed ? min + max - bounded : bounded) * 1000) / 1000;
		onValueChange?.(value);
	}
</script>

<div class={compact ? 'grid grid-cols-[minmax(0,1fr)_5.25rem] items-center gap-2' : 'mt-2 grid grid-cols-[minmax(0,1fr)_5.5rem] items-center gap-5'}>
	<Slider
		type="single"
		id={`${id}-slider`}
		{min}
		{max}
		{step}
		value={displayedValue}
		onValueCommit={change}
	/>
	<Input class={compact ? 'h-7 w-full px-2 text-right tabular-nums' : 'w-full text-right tabular-nums'} {id} {min} {max} {step} type="number" value={step < 1 ? displayedValue.toFixed(1) : displayedValue} oninput={(event) => change(event.currentTarget.valueAsNumber)} />
</div>
