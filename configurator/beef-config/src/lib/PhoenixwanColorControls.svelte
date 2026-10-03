<script lang="ts">
	import { Button } from '$lib/components/ui/button';
	import { Command, sendCommand } from '$lib/types/hid';
	import { appState } from '$lib/types/state.svelte';
	import { tr } from '$lib/types/locale.svelte';

	interface Props { mode?: number; centerBar?: boolean; }
	let { mode = 1, centerBar = false }: Props = $props();
	let sending = $state(false);

	const supported = $derived(centerBar ? { hue: true, saturation: true, brightness: true } : {
		hue: mode === 1 || mode === 2 || mode === 6 || mode === 7,
		saturation: mode === 1 || (mode >= 3 && mode <= 7),
		brightness: mode === 1 || (mode >= 3 && mode <= 5)
	});
	const controls = {
		turntable: {
			hue: [Command.LegacyPhoenixwanHueDecrease, Command.LegacyPhoenixwanHueIncrease],
			saturation: [Command.LegacyPhoenixwanSaturationDecrease, Command.LegacyPhoenixwanSaturationIncrease],
			brightness: [Command.LegacyPhoenixwanBrightnessDecrease, Command.LegacyPhoenixwanBrightnessIncrease]
		},
		centerBar: {
			hue: [Command.LegacyPhoenixwanBarHueDecrease, Command.LegacyPhoenixwanBarHueIncrease],
			saturation: [Command.LegacyPhoenixwanBarSaturationDecrease, Command.LegacyPhoenixwanBarSaturationIncrease],
			brightness: [Command.LegacyPhoenixwanBarBrightnessDecrease, Command.LegacyPhoenixwanBarBrightnessIncrease]
		}
	} as const;

	async function adjust(command: Command): Promise<void> {
		if (sending || !appState.device) return;
		sending = true;
		try {
			await sendCommand(command);
		} catch (error) {
			appState.error = `${tr('Phoenixwanの色調整に失敗しました', 'Failed to adjust Phoenixwan color')}: ${error}`;
		} finally {
			sending = false;
		}
	}
</script>

<div class="mb-4 space-y-3 rounded-md border p-3">
	<p class="text-sm text-muted-foreground">
		{tr('選択中のモードで調整できる項目を表示します。「−／＋」で1段階ずつ相対調整します。', 'Controls available for the selected mode are shown. Use −/+ to make relative adjustments one step at a time.')}
	</p>
	{#each [
		{ key: 'hue', label: tr('色相', 'Hue') },
		{ key: 'saturation', label: tr('彩度', 'Saturation') },
		{ key: 'brightness', label: tr('明るさ', 'Brightness') }
	] as item}
		{#if supported[item.key as keyof typeof supported]}
			<div class="flex items-center justify-between gap-3">
				<span class="text-sm font-medium">{item.label}</span>
				<div class="flex gap-2">
					<Button id={`legacy-phoenixwan-${centerBar ? 'bar' : 'turntable'}-${item.key}`} variant="outline" size="sm" disabled={sending || !appState.device} aria-label={`${item.label} ${tr('を下げる', 'decrease')}`} onclick={() => adjust(controls[centerBar ? 'centerBar' : 'turntable'][item.key as keyof typeof controls.turntable][0])}>−</Button>
					<Button variant="outline" size="sm" disabled={sending || !appState.device} aria-label={`${item.label} ${tr('を上げる', 'increase')}`} onclick={() => adjust(controls[centerBar ? 'centerBar' : 'turntable'][item.key as keyof typeof controls.turntable][1])}>＋</Button>
				</div>
			</div>
		{/if}
	{/each}
</div>
