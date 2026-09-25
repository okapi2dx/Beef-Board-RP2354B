<script lang="ts">
	import { Input } from '$lib/components/ui/input';
	import { Slider } from '$lib/components/ui/slider';

	interface Props {
		value: number;
		min: number;
		max: number;
		id: string;
		reversed?: boolean;
	}

	let { value = $bindable(), min, max, id, reversed = false }: Props = $props();

	let displayedValue = $derived(reversed ? min + max - value : value);
	function change(next: number) {
		if (!Number.isFinite(next)) return;
		const bounded = Math.max(min, Math.min(max, Math.round(next)));
		value = reversed ? min + max - bounded : bounded;
	}
</script>

<div class="mt-2 grid grid-cols-[minmax(0,1fr)_5.5rem] items-center gap-5">
	<Slider
		type="single"
		id={`${id}-slider`}
		{min}
		{max}
		value={displayedValue}
		onValueCommit={change}
	/>
	<Input class="w-full text-right tabular-nums" {id} {min} {max} type="number" value={displayedValue} oninput={(event) => change(event.currentTarget.valueAsNumber)} />
</div>
