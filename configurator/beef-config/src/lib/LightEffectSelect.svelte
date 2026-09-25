<script lang="ts" generics="T extends TurntableMode | BarMode">
	import { Label } from '$lib/components/ui/label';
	import * as Select from '$lib/components/ui/select';

	import type { BarMode, TurntableMode } from '$lib/types/types.svelte';
	import { tr } from '$lib/types/locale.svelte';

	interface Props {
		label: string;
		effect: T;
		modeMapping: T[];
	}

	let { label, effect = $bindable(), modeMapping }: Props = $props();
	const labels: Record<string, string> = {
		Static: '固定', Spin: '回転', Shift: 'シフト', 'Rainbow Static': 'レインボー（固定）',
		'Rainbow Reactive': 'レインボー（反応）', 'Rainbow Spin': 'レインボー（回転）',
		Reactive: '反応', Breathing: '呼吸', HID: 'HID連動', Off: 'オフ',
		'Key Spectrum (P1)': 'キー・スペクトラム（P1）', 'Key Spectrum (P2)': 'キー・スペクトラム（P2）',
		'Tape LED (P1)': 'テープLED（P1）', 'Tape LED (P2)': 'テープLED（P2）'
	};
	function effectLabel(value: T): string { return tr(labels[value] ?? value, value); }
</script>

<div class="mb-4">
	<Label>{label}</Label>
	<Select.Root
		type="single"
		bind:value={effect}
	>
		<Select.Trigger class="w-[180px]">{effectLabel(effect)}</Select.Trigger>
		<Select.Content>
			<Select.Group>
				{#each modeMapping as value}
					<Select.Item {value}>{effectLabel(value)}</Select.Item>
				{/each}
			</Select.Group>
		</Select.Content>
	</Select.Root>
</div>
