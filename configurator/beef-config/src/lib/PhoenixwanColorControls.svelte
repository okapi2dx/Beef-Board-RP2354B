<script lang="ts">
	import HelpText from '$lib/HelpText.svelte';
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

<div class="phoenixwan-color-controls">
  <div class="phoenixwan-color-help"><span>{tr('色の調整', 'Color adjustments')}</span><HelpText wrap>
    <p>{tr('「−／＋」で1段階ずつ調整します。', 'Use −/+ to adjust one step at a time.')}</p>
    <p>{tr('調整できない項目はグレーアウトします。', 'Unavailable controls are grayed out.')}</p>
  </HelpText></div>
	{#each [
		{ key: 'hue', label: tr('色相', 'Hue') },
		{ key: 'saturation', label: tr('彩度', 'Saturation') },
		{ key: 'brightness', label: tr('明るさ', 'Brightness') }
	] as item}
		{@const canAdjust = supported[item.key as keyof typeof supported]}
			<div class="flex items-center justify-between gap-3" class:opacity-50={!canAdjust}>
				<span class="text-sm font-medium">{item.label}</span>
				<div class="flex gap-2">
					<Button id={`legacy-phoenixwan-${centerBar ? 'bar' : 'turntable'}-${item.key}`} variant="outline" size="sm" disabled={!canAdjust || sending || !appState.device} aria-label={`${item.label} ${tr('を下げる', 'decrease')}`} onclick={() => adjust(controls[centerBar ? 'centerBar' : 'turntable'][item.key as keyof typeof controls.turntable][0])}>−</Button>
					<Button variant="outline" size="sm" disabled={!canAdjust || sending || !appState.device} aria-label={`${item.label} ${tr('を上げる', 'increase')}`} onclick={() => adjust(controls[centerBar ? 'centerBar' : 'turntable'][item.key as keyof typeof controls.turntable][1])}>＋</Button>
				</div>
			</div>
	{/each}
</div>

<style>
 .phoenixwan-color-controls { display: grid; gap: 8px; margin-bottom: 12px; }
 .phoenixwan-color-help { display: flex; align-items: center; gap: 6px; font-size: 12px; color: var(--muted-foreground); }
 @media (max-height: 650px) {
  .phoenixwan-color-controls { gap: 5px; margin-bottom: 8px; }
 }
</style>
