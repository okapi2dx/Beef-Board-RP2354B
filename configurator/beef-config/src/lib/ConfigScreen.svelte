<script lang="ts">
	import { onMount } from 'svelte';
	import * as Tabs from '$lib/components/ui/tabs';

	import { Input } from '$lib/components/ui/input';
	import { Label } from '$lib/components/ui/label/index.js';
	import * as Select from '$lib/components/ui/select';
	import { Separator } from '$lib/components/ui/separator/index.js';

	import InputModes from '$lib/InputModes.svelte';
	import KeyBinding from '$lib/KeyBinding.svelte';
	import LightEffectSelect from '$lib/LightEffectSelect.svelte';
	import SliderInput from '$lib/SliderInput.svelte';
	import Switch from '$lib/Switch.svelte';
	import ToolTipLabel from '$lib/ToolTipLabel.svelte';
	import TurntableTest from '$lib/TurntableTest.svelte';
	import ControllerMonitor from '$lib/ControllerMonitor.svelte';

import { readConfig, updateConfig, type Config } from '$lib/types/config.svelte';
import { Command, sendCommand, waitForReconnection } from '$lib/types/hid';
import { defaultUsbIdentity } from '$lib/types/board';
	import { appState } from '$lib/types/state.svelte';
	import { TurntableMode, BarMode, ControllerType, InputMode } from '$lib/types/types.svelte';
	import WarningAlert from '$lib/WarningAlert.svelte';
	import ColorPicker from '$lib/ColorPicker.svelte';
	import { tr } from '$lib/types/locale.svelte';

	let config: Config | undefined = $state();
	let {
		active = true,
		settingsTab = $bindable('input'),
		showTabs = true
	}: { active?: boolean; settingsTab?: string; showTabs?: boolean } = $props();
	let controllerRestarting = $state(false);
	function controllerModeLabel(mode: ControllerType): string {
		if (mode === ControllerType.Default) return tr('デフォルト', 'Default');
		if (mode === ControllerType.IIDXEntry) return 'IIDX Entry';
		return 'IIDX Premium';
	}

	async function changeControllerMode(mode: ControllerType): Promise<void> {
		if (!config || mode === config.controller_type || controllerRestarting) return;
		const expectedIdentity = mode === ControllerType.Default
			? defaultUsbIdentity(appState.boardInfo)
			: { vendorId: 0x1ccf, productId: mode === ControllerType.IIDXEntry ? 0x1018 : 0x8048 };
		controllerRestarting = true;
		appState.connecting = true;
		appState.error = undefined;
		try {
			config.controller_type = mode;
			await updateConfig(config);

			// Changing controller mode changes the USB identity. The device can
			// disappear before Chromium resolves sendFeatureReport(), so a send
			// error can still mean that the restart command succeeded.
			try {
				await sendCommand(Command.Restart);
			} catch {
				// Always continue into USB re-enumeration/reconnection.
			}

			// The old USB identity can still answer HID requests before the board
			// processes Restart. Wait specifically for the identity selected above.
			await waitForReconnection(20000, expectedIdentity);
			config = await readConfig();
		} catch (err) {
			appState.error = `${err}`;
		} finally {
			appState.connecting = false;
			controllerRestarting = false;
		}
	}

	onMount(async () => {
		try {
			config = await readConfig();
		} catch (err) {
			appState.error = `${err}`;
		}
	});

	$effect(() => {
		if (config && !controllerRestarting) {
			try {
				void updateConfig(config).catch((err) => { appState.error = `${err}`; });
			} catch (err) {
				appState.error = `${err}`;
			}
		}
	});
</script>

{#if config}
	{#if config.version < 33}
		<WarningAlert
			title={tr('ファームウェアが古いです', 'Outdated Firmware')}
			description={tr('角度式デッドゾーンを使うには、最新のファームウェアへ更新してください。', 'Update the firmware to use the angle-based deadzone.')}
		/>
	{/if}

<Tabs.Root bind:value={settingsTab}>
  {#if showTabs}
  <Tabs.List class="mb-5 grid h-auto w-full grid-cols-4" aria-label={tr('設定カテゴリ', 'Settings categories')}>
    <Tabs.Trigger value="input">{tr('入力設定', 'Input')}</Tabs.Trigger>
    <Tabs.Trigger value="led">{tr('LED設定', 'LEDs')}</Tabs.Trigger>
    <Tabs.Trigger value="keys">{tr('キー割り当て', 'Key Bindings')}</Tabs.Trigger>
    <Tabs.Trigger value="monitor">{tr('モニター', 'Monitor')}</Tabs.Trigger>
  </Tabs.List>
  {/if}
  <Tabs.Content value="input" class="settings-card">
	<div class="mb-4">
		<Label for="controller-type">{tr('コントローラーモード', 'Controller Mode')}</Label>
		<Select.Root
			type="single"
			value={config.controller_type}
			disabled={controllerRestarting}
			onValueChange={(value) => void changeControllerMode(value as ControllerType)}
		>
			<Select.Trigger class="w-[180px]">{controllerModeLabel(config.controller_type)}</Select.Trigger>
			<Select.Content>
				<Select.Group>
					{#each Object.values(ControllerType) as value}
						<Select.Item {value}>{controllerModeLabel(value)}</Select.Item>
					{/each}
				</Select.Group>
			</Select.Content>
		</Select.Root>
	</div>

	{#if controllerRestarting}
		<WarningAlert
			title={tr('USBを再認識しています', 'Reconnecting USB')}
			description={tr('コントローラーを自動再起動しています。そのままお待ちください。', 'The controller is restarting automatically. Please wait.')}
		/>
	{/if}

	<Separator class="mb-4" />

	{#if Object.values(ControllerType).includes(config.controller_type)}
		<div>
			<h2 class="mb-2 text-xl font-bold">{tr('IIDX設定', 'IIDX Configuration')}</h2>
			<InputModes bind:inputMode={config.iidx_input_mode} />

			<h3 class="mb-2 text-xl font-bold">{tr('ターンテーブル', 'Turntable')}</h3>

			<Switch label={tr('回転方向を反転', 'Reverse Turntable')} bind:checked={config.reverse_tt} />
			{#if config.version >= 20}
				<Switch
					label={config.digital_tt ? tr('ターンテーブル入力：デジタル', 'Turntable Input: Digital') : tr('ターンテーブル入力：アナログ', 'Turntable Input: Analog')}
					bind:checked={config.digital_tt}
				/>
				{#if !config.digital_tt && config.iidx_input_mode === InputMode.Keyboard}
					<p class="mb-4 text-sm">{tr('アナログ入力を使う場合は、入力モードをジョイスティックにしてください。', 'Select Joystick input mode to use analog turntable input.')}</p>
				{/if}
			{/if}
			<div class="mb-4">
				<ToolTipLabel
					forId="tt-deadzone"
					label={config.version >= 33
						? tr('ターンテーブル デッドゾーン（°）', 'Turntable Deadzone (°)')
						: config.version >= 22
							? tr('ターンテーブル デッドゾーン（ms）', 'Turntable Deadzone (ms)')
							: tr('ターンテーブル デッドゾーン', 'Turntable Deadzone')}
				>
					{#if config.version >= 33}
						<p>{tr('X軸の回転量を角度で判定します。', 'Uses the X-axis rotation angle for the deadzone.')}</p>
						<p>{tr('設定角度に達するまで入力を無視します。', 'Input is ignored until the configured angle is reached.')}</p>
						<p>{tr('0°で無効になり、すぐに入力します。', 'Set 0° to disable it and accept input immediately.')}</p>
					{:else}
						<p>{tr('短い回転や誤入力を無視する時間です。', 'Ignores brief turntable movement and false input.')}</p>
						<p>{tr('0msで無効になります。', 'Set 0 ms to disable it.')}</p>
					{/if}
				</ToolTipLabel>
				<SliderInput
					bind:value={config.tt_deadzone}
					min={config.version >= 22 ? 0 : 1}
					max={config.version >= 33 ? 30 : config.version >= 22 ? 255 : 6}
					id="tt-deadzone"
				/>
			</div>

			{#if config.version >= 12}
				<div class="mb-4">
					<ToolTipLabel forId="tt-sustain-ms" label={tr('ターンテーブル保持時間（ms）', 'Turntable Hold Time (ms)')}>
						<p>{tr('回転が止まった後の入力保持時間です。', 'Sets how long input is held after the turntable stops.')}</p>
						<p>{tr('値を大きくすると入力が途切れにくくなります。', 'Higher values make the input less likely to drop out.')}</p>
						<p>{tr('0msでは保持しません。', 'Set 0 ms for no hold time.')}</p>
					</ToolTipLabel>
					<SliderInput bind:value={config.tt_sustain_ms} min={0} max={255} id="tt-sustain-ms" />
				</div>
			{/if}

			{#if config.version >= 21}
				<div class="mb-4">
					<ToolTipLabel forId="tt-delay-ms" label={tr('ターンテーブル入力ディレイ（ms）', 'Turntable Input Delay (ms)')}>
						<p>{tr('回転してから入力するまでの遅延時間です。', 'Sets the delay before turntable input is sent.')}</p>
						<p>{tr('値を大きくすると入力が遅れて反映されます。', 'Higher values make the input take longer to appear.')}</p>
						<p>{tr('0msで遅延なしになります。', 'Set 0 ms for no delay.')}</p>
					</ToolTipLabel>
					<SliderInput bind:value={config.tt_delay_ms} min={0} max={255} id="tt-delay-ms" />
					
				</div>
			{/if}
			<div class="mb-4">
				<ToolTipLabel forId="tt-ratio" label={tr('ターンテーブル感度', 'Turntable Sensitivity')}>
					<p>{tr('ターンテーブルの回転に対する入力量を調整します。', 'Adjusts how much input is produced by turntable rotation.')}</p>
					<p>{tr('感度10では1カウント、感度5では2カウントです。', 'Sensitivity 10 uses 1 count; sensitivity 5 uses 2 counts.')}</p>
					<p>{tr('感度1では10カウントです。', 'Sensitivity 1 uses 10 counts.')}</p>
				</ToolTipLabel>
				<SliderInput bind:value={config.tt_ratio} min={1} max={config.version >= 26 ? 10 : 6} id="tt-ratio" />
			</div>
			<TurntableTest
				active={active && settingsTab === 'input'}
				supported={config.version >= 24}
				digital={Boolean(config.digital_tt)}
				streaming={config.version >= 30}
			/>

			{#if config.version >= 15}
				<Separator class="mb-4" />

				<h3 class="mb-2 text-xl font-bold">{tr('チャタリング防止', 'Debouncing')}</h3>
				<div class="mb-4">
					<ToolTipLabel forId="iidx-button-debounce" label={tr('ボタンのデバウンス（ms）', 'Button Debounce (ms)')}>
						<p>{tr('ボタンのチャタリングを防止します。', 'Prevents button switch chatter.')}</p>
						<p>{tr('値を大きくすると連続入力を抑えます。', 'Higher values suppress repeated input more strongly.')}</p>
						<p>{tr('0msで無効になります。', 'Set 0 ms to disable it.')}</p>
					</ToolTipLabel>
					<SliderInput
						bind:value={config.iidx_buttons_debounce}
						min={0}
						max={50}
						id="iidx-button-debounce"
					/>
				</div>

				
			{/if}

		</div>
	{/if}
</Tabs.Content>
  <Tabs.Content value="led" class="settings-card">
	{#if Object.values(ControllerType).includes(config.controller_type)}
		<div>
			<h3 class="mb-2 text-xl font-bold">{tr('ライティング', 'Lights')}</h3>

			<Switch label={tr('LEDを無効にする', 'Disable LEDs')} bind:checked={config.disable_leds} />

			{#if !config.disable_leds}
				<h3 class="mb-2 text-xl font-bold">{tr('ボタンLED', 'Button LEDs')}</h3>
				{#if config.version >= 23}
					<div class="mb-4">
						<ToolTipLabel forId="button-led-fade" label={tr('フェードアウト時間（ms）', 'Fade-out Time (ms)')}>
							<p>{tr('ボタンLEDが消えるまでの時間です。', 'Sets how long a button LED takes to turn off.')}</p>
							<p>{tr('値を大きくするとゆっくり消灯します。', 'Higher values make the LED fade out more slowly.')}</p>
							<p>{tr('0msではすぐに消灯します。', 'At 0 ms, it turns off immediately.')}</p>
						</ToolTipLabel>
						<SliderInput bind:value={config.button_led_fade_ms} min={0} max={config.version >= 27 ? 1000 : 255} id="button-led-fade" />
					</div>
					<div class="mb-4">
						<ToolTipLabel forId="button-led-brightness" label={tr('ボタンLEDの明るさ（％）', 'Button LED Brightness (%)')}>
							<p>{tr('ボタンLEDの最大明るさを設定します。', 'Sets the maximum button LED brightness.')}</p>
							<p>{tr('値を小さくするとLEDが暗くなります。', 'Lower values make the LED dimmer.')}</p>
							<p>{tr('0％で消灯、100％で最大です。', '0% turns it off; 100% is maximum brightness.')}</p>
						</ToolTipLabel>
						<SliderInput bind:value={config.button_led_brightness} min={0} max={100} id="button-led-brightness" />
					</div>
					<Switch label={tr('ボタンLEDを反転', 'Invert Button LEDs')} bind:checked={config.button_led_invert}>
						<p>{tr('ボタンLEDの点灯動作を反転します。', 'Reverses the button LED lighting behavior.')}</p>
						<p>{tr('OFFでは押している間に点灯します。', 'OFF lights the LED while pressed.')}</p>
						<p>{tr('ONでは離している間に点灯します。', 'ON lights the LED while released.')}</p>
					</Switch>
					
				{:else}
					<p class="mb-4">{tr('ボタンLEDの調整にはファームウェアV1.00以降へ更新してください。', 'Update to firmware V1.00 or later to adjust button LEDs.')}</p>
				{/if}
				{#if config.version >= 19}
					<Switch label={tr('ターンテーブルとセンターバーの発光効果を連動', 'Link Turntable and Light Bar Effects')} bind:checked={config.link_bar_effect} />
				{/if}
				{#if config.version >= 33}
					<p class="mb-4 text-sm">{tr('PHOENIXWAN UART接続も同じ発光設定に追従します。色は相対調整のため電源投入時の完全一致はできず、センターバーの動きとターンテーブルの消灯もUARTの公開仕様では再現できません。', 'Phoenixwan UART follows these lighting settings too. Color adjustments are relative, so power-on colors may differ; the published UART protocol also cannot reproduce moving bar patterns or turntable off.')}</p>
				{/if}
				{@const ttModeMapping = Object.values(TurntableMode)}
				<div class="grid gap-x-6 gap-y-2 md:grid-cols-2">
					<div class="min-w-0">
						<LightEffectSelect
							label={tr('ターンテーブルの発光効果', 'Turntable Effect')}
							bind:effect={config.tt_effect}
							modeMapping={ttModeMapping}
						/>
						{#if config.tt_effect === TurntableMode.Static}
							<ColorPicker bind:hsv={config.tt_static_hsv} />
						{:else if config.tt_effect === TurntableMode.Spin}
							<ColorPicker bind:hsv={config.tt_spin_hsv} />
						{:else if config.tt_effect === TurntableMode.Shift}
							<ColorPicker bind:hsv={config.tt_shift_hsv} />
						{:else if config.tt_effect === TurntableMode.RainbowStatic}
							<ColorPicker bind:hsv={config.tt_rainbow_static_hsv} />
						{:else if config.tt_effect === TurntableMode.RainbowReactive}
							<ColorPicker bind:hsv={config.tt_rainbow_react_hsv} />
						{:else if config.tt_effect === TurntableMode.RainbowSpin}
							<ColorPicker bind:hsv={config.tt_rainbow_spin_hsv} />
						{:else if config.tt_effect === TurntableMode.Reactive}
							<ColorPicker bind:hsv={config.tt_react_hsv} />
						{:else if config.tt_effect === TurntableMode.Breathing}
							<ColorPicker bind:hsv={config.tt_breathing_hsv} />
						{/if}
					</div>

					<div class="min-w-0">
						{#if config.version >= 19 && config.link_bar_effect}
							<p class="mb-4 text-sm">{tr('センターバーにターンテーブルと同じ発光効果と色を反映します。', 'The light bar follows the turntable effect and color.')}</p>
						{:else}
							{@const barModeMapping = config.version >= 17 ? Object.values(BarMode) : Object.values(BarMode).filter((mode) => mode !== BarMode.Static)}
							<LightEffectSelect
								label={tr('センターバーの発光効果', 'Light Bar Effect')}
								bind:effect={config.bar_effect}
								modeMapping={barModeMapping}
							/>
							{#if config.version >= 17 && config.bar_effect === BarMode.Static}
								<ColorPicker bind:hsv={config.bar_static_hsv} />
							{:else if config.version < 17}
								<p class="mb-4 text-sm">{tr('センターバーの固定色を使うには、対応ファームウェアへ更新してください。', 'Update the firmware to use a static light bar color.')}</p>
							{/if}
						{/if}
					</div>
				</div>

				{#if config.version >= 16}
					<Separator class="mb-4" />
					<div class="mb-4">
						<ToolTipLabel forId="led-refresh" label={tr('RGB LED更新頻度', 'RGB LED Refresh Rate')}>
							<p>{tr('RGB LEDを更新する頻度です。', 'Sets the RGB LED refresh rate.')}</p>
							<p>{tr('値を高くすると動きが滑らかになります。', 'Higher values make animations smoother.')}</p>
							<p>{tr('高すぎると処理負荷が増えます。', 'Very high values increase processing load.')}</p>
						</ToolTipLabel>
						<SliderInput bind:value={config.led_refresh} min={1} max={60} id="led-refresh" />
					</div>

					<div class="mb-4">
						<ToolTipLabel
							forId="rainbow-spin-speed"
							label={tr('レインボー効果の回転速度', 'Rainbow Effect Spin Speed')}
						>
							<p>{tr('レインボーの色が動く速さです。', 'Sets how fast the rainbow colors move.')}</p>
							<p>{tr('値を大きくすると動きが速くなります。', 'Higher values make the movement faster.')}</p>
							<p>{tr('好みに合わせて調整してください。', 'Adjust it to your preference.')}</p>
						</ToolTipLabel>
						<SliderInput
							bind:value={config.rainbow_spin_speed}
							min={1}
							max={5}
							id="rainbow-spin-speed"
						/>
					</div>

					<div class="mb-4 flex items-center justify-between gap-4">
						<ToolTipLabel forId="tt-leds" label={tr('ターンテーブルLED数', 'Turntable LEDs')}>
							<p>{tr('ターンテーブルのRGB LED数を設定します。', 'Sets the number of RGB LEDs on the turntable.')}</p>
							<p>{tr('実際に接続しているLED数に合わせてください。', 'Match this to the number of LEDs actually connected.')}</p>
							<p>{tr('初期値は24。RP2354Bでは変更がすぐに反映されます。', 'The default is 24. Changes apply immediately on RP2354B.')}</p>
						</ToolTipLabel>
						<Input
							class="w-24 shrink-0"
							id="tt-leds"
							min={1}
							max={255}
							type="number"
							bind:value={config.tt_leds}
						/>
					</div>

					{#if config.version >= 32}
						<div class="mb-4 flex items-center justify-between gap-4">
							<ToolTipLabel forId="bar-leds" label={tr('センターバーLED数', 'Center Bar LEDs')}>
								<p>{tr('センターバーのRGB LED数を設定します。', 'Sets the number of RGB LEDs on the center bar.')}</p>
								<p>{tr('実際に接続しているLED数に合わせてください。', 'Match this to the number of LEDs actually connected.')}</p>
								<p>{tr('初期値は16。RP2354Bでは変更がすぐに反映されます。', 'The default is 16. Changes apply immediately on RP2354B.')}</p>
							</ToolTipLabel>
							<Input
								class="w-24 shrink-0"
								id="bar-leds"
								min={1}
								max={255}
								type="number"
								bind:value={config.bar_leds}
							/>
						</div>
					{:else}
						<p class="mb-4 text-sm">{tr('センターバーLED数の変更には、対応ファームウェアへ更新してください。', 'Update to compatible firmware to change the center bar LED count.')}</p>
					{/if}
				{/if}
			{/if}
		</div>
	{/if}
</Tabs.Content>
  <Tabs.Content value="keys" class="settings-card keys-card">
	{#if config.version >= 13}
		<KeyBinding bind:config />
	{/if}
</Tabs.Content>
  <Tabs.Content value="monitor" class="settings-card"><ControllerMonitor {config} active={active && settingsTab === 'monitor'} /></Tabs.Content>
</Tabs.Root>

{/if}

<style>
  :global(.settings-card) { border: 1px solid var(--border); border-radius: 1rem; padding: 1.25rem; background: var(--card); }
  :global(.settings-card .mb-4) { margin-bottom: 0.75rem; }
  :global(.settings-card h2), :global(.settings-card h3) { font-size: 1.125rem; margin-top: 0.75rem; margin-bottom: 0.75rem; }
  :global(.settings-card label) { line-height: 1.5; }
  :global(.keys-card) { padding-top: 0.75rem; padding-bottom: 0.9rem; }
</style>

