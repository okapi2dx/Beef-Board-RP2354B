<script lang="ts">
	import { Button } from '$lib/components/ui/button';
	import { Label } from '$lib/components/ui/label';
	import * as AlertDialog from '$lib/components/ui/alert-dialog';
	import * as Select from '$lib/components/ui/select';
	import { Separator } from '$lib/components/ui/separator';
	import HelpText from '$lib/HelpText.svelte';
	import { ControllerType, InputMode } from '$lib/types/types.svelte';
	import { IIDXKeyMapping, type Config } from '$lib/types/config.svelte';
	import { getKeyCode, getKeyName } from '$lib/types/hid-codes';
	import { appState } from '$lib/types/state.svelte';
	import { tr } from '$lib/types/locale.svelte';

	interface Props {
		config: Config;
	}

	let { config = $bindable() }: Props = $props();

	let selectedButton: number | null = $state(null);
	let resetOpen = $state(false);

	// Button labels
	const IIDX_BUTTON_LABELS = {
		main_buttons: ['1', '2', '3', '4', '5', '6', '7'],
		function_buttons: ['E1', 'E2', 'E3', 'E4'],
		tt_ccw: 'TT-',
		tt_cw: 'TT+'
	};
	const BUTTON_LAYOUT_LABELS = ['B1', 'B2', 'B3', 'B4', 'B5', 'B6', 'B7', 'E1', 'E2', 'E3', 'E4'];

	function startKeyCapture(button: number) {
		selectedButton = button;
		window.addEventListener('keydown', handleKeyDown);
	}

	function handleKeyDown(event: KeyboardEvent) {
		if (selectedButton === null) return;

		event.preventDefault();
		window.removeEventListener('keydown', handleKeyDown);

		const hidKeyCode = getKeyCode(event.code);
		if (hidKeyCode) {
			switch (config.controller_type) {
				case ControllerType.Default:
				case ControllerType.IIDXEntry:
				case ControllerType.IIDXPremium:
					if (selectedButton < 7) {
						config.iidx_keys.main_buttons[selectedButton] = hidKeyCode;
					} else if (selectedButton < 11) {
						config.iidx_keys.function_buttons[selectedButton - 7] = hidKeyCode;
					} else if (selectedButton === 11) {
						config.iidx_keys.tt_ccw = hidKeyCode;
					} else if (selectedButton === 12) {
						config.iidx_keys.tt_cw = hidKeyCode;
					}
					break;
			}
		} else {
			appState.error = `Cannot bind button to ${event.code}`;
		}

		selectedButton = null;
	}

	function resetKeys() {
		config.iidx_keys = new IIDXKeyMapping();
		config.button_mapping = BUTTON_LAYOUT_LABELS.map((_, i) => i);
		selectedButton = null;
		window.removeEventListener('keydown', handleKeyDown);
		resetOpen = false;
	}

	function changeButtonMapping(physical: number, value: string | null | undefined) {
		if (value == null) return;
		const logical = Number(value);
		if (!Number.isInteger(logical) || logical < 0 || logical >= BUTTON_LAYOUT_LABELS.length) return;
		if (config.button_mapping[physical] === logical) return;

		// Multiple physical buttons may intentionally produce the same logical
		// button, so only update the selected physical input.
		const next = [...config.button_mapping];
		next[physical] = logical;
		config.button_mapping = next;
	}

	// Keep the remap layout aligned with ControllerMonitor exactly.
	// B1/B3/B5/B7 are the lower row; B2/B4/B6 are the upper row.
	const IIDX_REMAP_POSITIONS = [
		[45, 175],
		[75, 105],
		[105, 175],
		[135, 105],
		[165, 175],
		[195, 105],
		[225, 175],
		[45, 15],
		[105, 15],
		[165, 15],
		[225, 15]
	];

</script>

<div class="key-binding-root">
	<div class="key-binding-header flex items-center justify-between">
		<h3 class="text-xl font-bold">{tr('キー割り当て', 'Key Bindings')}</h3>
		{#if config.iidx_input_mode === InputMode.Keyboard || (config.iidx_input_mode === InputMode.Joystick && config.version >= 28)}
			<AlertDialog.Root bind:open={resetOpen}>
				<AlertDialog.Trigger>
					{#snippet child({ props })}
						<Button {...props} variant="outline" class="reset-action-button">{tr('キーを初期化', 'Reset Keys')}</Button>
					{/snippet}
				</AlertDialog.Trigger>
				<AlertDialog.Content>
					<AlertDialog.Header>
						<AlertDialog.Title>{tr('キー割り当てを初期化しますか？', 'Reset Key Bindings?')}</AlertDialog.Title>
						<AlertDialog.Description>
							{tr('すべてのキー割り当てを初期値に戻します。', 'This will reset all key bindings to their default values.')}
						</AlertDialog.Description>
					</AlertDialog.Header>
					<AlertDialog.Footer>
						<AlertDialog.Action class="reset-action-button" onclick={resetKeys}>{tr('初期化', 'Reset')}</AlertDialog.Action>
						<AlertDialog.Cancel>{tr('キャンセル', 'Cancel')}</AlertDialog.Cancel>
					</AlertDialog.Footer>
				</AlertDialog.Content>
			</AlertDialog.Root>
		{/if}
	</div>

	{#if Object.values(ControllerType).includes(config.controller_type)}
		{#if config.iidx_input_mode === InputMode.Joystick && config.version >= 28}
			<div class="remap-section">
				<div class="remap-heading">
					<h3 class="text-xl font-bold">{tr('ボタン配置入れ替え', 'Button Layout Remapping')}</h3>
					<HelpText label={tr('ボタン配置入れ替え', 'Button Layout Remapping')}>
						<p>{tr('各ボタンの入力先を変更できます。', 'Changes the input destination for each button.')}</p>
						<p>{tr('プルダウンから割り当て先を選択します。', 'Select the destination from the drop-down menu.')}</p>
						<p>{tr('同じ入力先を複数のボタンに設定できます。', 'Multiple buttons can use the same destination.')}</p>
					</HelpText>
				</div>
				<div class="remap-stage">
					<div class="remap-keyboard">
						{#each BUTTON_LAYOUT_LABELS as physicalLabel, i}
							<div
								class="remap-key"
								class:black={i < 7 && i % 2 === 1}
								class:function-key={i >= 7}
								class:changed={config.button_mapping[i] !== i}
								style:left={`${(IIDX_REMAP_POSITIONS[i][0] / 310) * 100}%`}
								style:top={`${(IIDX_REMAP_POSITIONS[i][1] / 260) * 100}%`}
							>
								<strong>{physicalLabel}</strong>
								<span class="remap-arrow">↓</span>
								<Select.Root
									type="single"
									value={String(config.button_mapping[i])}
									onValueChange={(value) => changeButtonMapping(i, value)}
								>
									<Select.Trigger class="remap-select">
										{BUTTON_LAYOUT_LABELS[config.button_mapping[i]]}
									</Select.Trigger>
									<Select.Content>
										<Select.Group>
											{#each BUTTON_LAYOUT_LABELS as logicalLabel, logical}
												<Select.Item value={String(logical)}>{logicalLabel}</Select.Item>
											{/each}
										</Select.Group>
									</Select.Content>
								</Select.Root>
							</div>
						{/each}
					</div>
				</div>
			</div>
		{/if}

		{#if config.iidx_input_mode === InputMode.Keyboard}
			<Separator class="mb-4" />
			<Label>{tr('メインボタン', 'Main Buttons')}</Label>
		<div class="mb-2 flex flex-col">
			<div class="grid grid-cols-4 gap-2">
				{#each IIDX_BUTTON_LABELS.main_buttons as label, i}
					<Button
						variant={selectedButton === i ? 'default' : 'outline'}
						onclick={() => startKeyCapture(i)}
					>
						{label}: {getKeyName(config.iidx_keys.main_buttons[i])}
					</Button>
				{/each}
			</div>
		</div>

		<Label>{tr('機能ボタン', 'Function Buttons')}</Label>
		<div class="mb-2 flex flex-col">
			<div class="grid grid-cols-4 gap-2">
				{#each IIDX_BUTTON_LABELS.function_buttons as label, i}
					<Button
						variant={selectedButton === i + 7 ? 'default' : 'outline'}
						onclick={() => startKeyCapture(i + 7)}
					>
						{label}: {getKeyName(config.iidx_keys.function_buttons[i])}
					</Button>
				{/each}
			</div>
		</div>

		<Label>{tr('ターンテーブル', 'Turntable')}</Label>
		<div class="mb-2 flex flex-col">
			<div class="grid grid-cols-2 gap-2">
				<Button
					variant={selectedButton === 11 ? 'default' : 'outline'}
					onclick={() => startKeyCapture(11)}
				>
					{IIDX_BUTTON_LABELS.tt_ccw}: {getKeyName(config.iidx_keys.tt_ccw)}
				</Button>
				<Button
					variant={selectedButton === 12 ? 'default' : 'outline'}
					onclick={() => startKeyCapture(12)}
				>
					{IIDX_BUTTON_LABELS.tt_cw}: {getKeyName(config.iidx_keys.tt_cw)}
				</Button>
			</div>
		</div>
		{/if}
	{/if}

	{#if selectedButton !== null}
		<div class="bg-muted mt-4 rounded-md p-4">{tr('割り当てるキーを押してください…', 'Press any key to bind...')}</div>
	{/if}
</div>

<style>
	.key-binding-root {
		margin: 0;
	}
	.key-binding-header {
		margin-bottom: 4px;
	}
	.remap-section {
		margin-bottom: 6px;
	}
	.remap-heading {
		display: flex;
		align-items: center;
		gap: 6px;
		margin-bottom: 6px;
	}
	.remap-heading h3 {
		margin: 0 !important;
		font-size: 1rem !important;
	}
	.remap-stage {
		width: min(100%, 480px);
		box-sizing: border-box;
		margin: 0 auto;
		background: #0f172a;
		border-radius: 10px;
		padding: 7px 10px 9px;
		color: #e2e8f0;
	}
	.remap-keyboard {
		position: relative;
		width: 100%;
		max-width: 454px;
		aspect-ratio: 310 / 260;
		margin: 0 auto;
		background: linear-gradient(145deg, #e8edf2, #c4ced8);
		border: 2px solid #718094;
		border-radius: 10px;
		box-sizing: border-box;
		box-shadow: inset 0 0 0 1px #f8fafc, 0 5px 14px rgba(2, 6, 23, 0.18);
		overflow: hidden;
	}
	.remap-key {
		position: absolute;
		width: 15%;
		height: 22%;
		border: 2px solid #7b8797;
		border-radius: 7px;
		background: #edf1f6;
		color: #172033;
		display: flex;
		flex-direction: column;
		align-items: center;
		justify-content: center;
		gap: 1px;
		padding: 4px;
		box-shadow: inset 0 -2px #bac4d0;
		transition: background 120ms ease, border-color 120ms ease, color 120ms ease, box-shadow 120ms ease, transform 120ms ease;
	}
	.remap-key:hover,
	.remap-key:focus-within {
		background: #dbeafe;
		border-color: #93c5fd;
		color: #1e3a8a;
		box-shadow: inset 0 -3px #bfdbfe, 0 0 0 2px rgba(96, 165, 250, 0.16);
		transform: translateY(-1px);
	}
	.remap-key.function-key {
		width: 14%;
		height: auto;
		aspect-ratio: 1 / 1;
		padding: 3px;
	}
	.remap-key.changed {
		border-color: #3b82f6;
		background: #eff6ff;
		box-shadow: inset 0 -2px #bfdbfe, 0 0 0 2px rgba(59, 130, 246, 0.16);
	}
	.remap-key.black {
		background: #202b3b;
		color: #fff;
	}
	.remap-key.black:hover,
	.remap-key.black:focus-within {
		background: #273b5f;
		border-color: #60a5fa;
		box-shadow: inset 0 -2px #1e3a5f, 0 0 0 2px rgba(96, 165, 250, 0.16);
	}
	.remap-key.black.changed {
		background: #243b5c;
		border-color: #60a5fa;
		box-shadow: inset 0 -2px #1e3a5f, 0 0 0 2px rgba(96, 165, 250, 0.18);
	}
	.remap-key strong {
		font-size: 13px;
		font-weight: 800;
		line-height: 1;
	}
	.remap-arrow {
		font-size: 10px;
		font-weight: 800;
		line-height: 1;
		opacity: 0.72;
	}
	:global(.remap-select) {
		height: 26px !important;
		min-height: 26px !important;
		width: 50px !important;
		padding: 0 5px !important;
		gap: 2px !important;
		font-size: 12px !important;
		font-weight: 800 !important;
		line-height: 1 !important;
		background: #fff !important;
		color: #172033 !important;
		border-color: #94a3b8 !important;
		cursor: pointer !important;
	}
	:global(.remap-select:hover),
	:global(.remap-select:focus-visible) {
		border-color: #93c5fd !important;
		box-shadow: 0 0 0 2px rgba(96, 165, 250, 0.12) !important;
	}
	@media (max-width: 760px) {
		.remap-stage {
			width: 100%;
			padding: 7px;
		}
		.remap-keyboard {
			width: 100%;
			max-width: 430px;
		}
		.remap-key {
			width: 15%;
		}
		.remap-key.function-key {
			width: 14%;
		}
	}
</style>
