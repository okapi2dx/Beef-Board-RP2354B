<script lang="ts">
	import HeadingIcon from '$lib/HeadingIcon.svelte';
	import { onMount } from 'svelte';
	import * as Tabs from '$lib/components/ui/tabs';

	import { Input } from '$lib/components/ui/input';
	import { Button } from '$lib/components/ui/button';
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
	import ChatterTest from '$lib/ChatterTest.svelte';

import { readConfig, updateConfig, type Config } from '$lib/types/config.svelte';
import { Command, restartAndWaitForReconnection, sendCommand } from '$lib/types/hid';
import { BoardCapability, BoardType, defaultUsbIdentity, isRp2354bBoard } from '$lib/types/board';
	import { appState } from '$lib/types/state.svelte';
	import { TurntableMode, BarMode, ControllerType, InputMode } from '$lib/types/types.svelte';
	import WarningAlert from '$lib/WarningAlert.svelte';
	import ColorPicker from '$lib/ColorPicker.svelte';
	import PhoenixwanColorControls from '$lib/PhoenixwanColorControls.svelte';
	import { tr } from '$lib/types/locale.svelte';

	let config: Config | undefined = $state();
	const legacyPhoenixwanTtModes = [
		TurntableMode.Static,
		TurntableMode.Spin,
		TurntableMode.Shift,
		TurntableMode.RainbowStatic,
		TurntableMode.RainbowReactive,
		TurntableMode.Reactive,
		TurntableMode.Breathing
	];
	let {
		active = true,
		settingsTab = $bindable('input'),
		showTabs = true
	}: { active?: boolean; settingsTab?: string; showTabs?: boolean } = $props();
	let controllerRestarting = $state(false);
	let legacyPhoenixwanBusy = $state(false);
	async function setLegacyPhoenixwan(enabled: boolean): Promise<void> {
		if (legacyPhoenixwanBusy || !appState.boardInfo || !config) return;
		legacyPhoenixwanBusy = true;
		const device = appState.device;
		const currentConfig = config;
		try {
			await sendCommand(enabled ? Command.EnableLegacyPhoenixwan : Command.DisableLegacyPhoenixwan);
			if (device !== appState.device || !appState.boardInfo) return;
			const stateBit = BoardCapability.LegacyPhoenixwanEnabled;
			appState.boardInfo = {
				...appState.boardInfo,
				capabilities: enabled
					? appState.boardInfo.capabilities | stateBit
					: appState.boardInfo.capabilities & ~stateBit
			};
		} catch (error) {
			if (device === appState.device && config === currentConfig) config.legacy_phoenixwan_enabled = !enabled;
			appState.error = `${tr('旧Phoenixwanの設定を変更できませんでした', 'Could not change the legacy Phoenixwan setting')}: ${error}`;
		} finally {
			legacyPhoenixwanBusy = false;
		}
	}
	function controllerModeLabel(mode: ControllerType): string {
		if (mode === ControllerType.Default) return tr('デフォルト', 'Default');
		if (mode === ControllerType.IIDXEntry) return 'IIDX Entry';
		return 'IIDX Premium';
	}

	async function changeControllerMode(mode: ControllerType): Promise<void> {
		if (!config || mode === config.controller_type || controllerRestarting) return;
		const preferredDefaultIdentity = defaultUsbIdentity(appState.boardInfo);
		const expectedIdentity = mode === ControllerType.Default
			? [preferredDefaultIdentity, { vendorId: 0xfeed, productId: preferredDefaultIdentity.productId === 0x0003 ? 0x0000 : 0x0003 }]
			: { vendorId: 0x1ccf, productId: mode === ControllerType.IIDXEntry ? 0x1018 : 0x8048 };
		controllerRestarting = true;
		appState.connecting = true;
		appState.error = undefined;
		try {
			config.controller_type = mode;
			await updateConfig(config);
			// The AT90 acknowledges SET_REPORT before its EEPROM update finishes.
			// Windows rejects GET_REPORT during that interval, so wait for a
			// successful readback before requesting the USB identity change.
			let saved: Config | undefined;
			let readError: unknown;
			const saveDeadline = Date.now() + 5000;
			while (Date.now() < saveDeadline) {
				try {
					saved = await readConfig();
					if (saved.controller_type === mode) break;
				} catch (err) {
					readError = err;
				}
				await new Promise(resolve => setTimeout(resolve, 150));
			}
			if (saved?.controller_type !== mode) {
				throw new Error(`Controller mode was not saved${readError ? `: ${readError}` : ''}`);
			}

			// The board info report is unavailable on some Windows IIDX identities.
			// Accept either supported default identity, then verify its config HID.
			await restartAndWaitForReconnection(20000, expectedIdentity);
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
            if (config.legacy_phoenixwan_enabled && appState.boardInfo?.boardType === BoardType.RP2354B &&
                !legacyPhoenixwanTtModes.includes(config.tt_effect)) {
                config.tt_effect = TurntableMode.Static;
            }
			try {
				void updateConfig(config).catch((err) => { appState.error = `${err}`; });
			} catch (err) {
				appState.error = `${err}`;
			}
		}
	});
</script>

{#if config}
	{#if !config.supports_encoder_deadzone}
		<WarningAlert
			title={tr('ファームウェアが古いです', 'Outdated Firmware')}
			description={tr('エンコーダーカウント式デッドゾーンを使うには、ファームウェアを更新してください。', 'Update the firmware to use the encoder-count deadzone.')}
		/>
	{/if}

<Tabs.Root bind:value={settingsTab}>
  {#if showTabs}
  <Tabs.List class="mb-5 grid h-auto w-full grid-cols-6" aria-label={tr('設定カテゴリ', 'Settings categories')}>
    <Tabs.Trigger value="input">{tr('入力設定', 'Input')}</Tabs.Trigger>
    <Tabs.Trigger value="led">{tr('ボタンLED設定', 'Button LEDs')}</Tabs.Trigger>
    <Tabs.Trigger value="rgb">{tr('RGB LED設定', 'RGB LEDs')}</Tabs.Trigger>
    <Tabs.Trigger value="keys">{tr('キー割り当て', 'Key Bindings')}</Tabs.Trigger>
    <Tabs.Trigger value="monitor">{tr('モニター', 'Monitor')}</Tabs.Trigger>
    <Tabs.Trigger value="chatter">{tr('チャタリングテスト', 'Chatter Test')}</Tabs.Trigger>
  </Tabs.List>
  {/if}
  <Tabs.Content value="input" class="settings-card input-card">
	<div class="settings-group mode-settings mb-4">
		<div class="mode-field">
		<Label for="controller-type">{tr('コントローラーモード', 'Controller Mode')}</Label>
		<Select.Root
			type="single"
			value={config.controller_type}
			disabled={controllerRestarting}
			onValueChange={(value) => void changeControllerMode(value as ControllerType)}
		>
			<Select.Trigger class="w-full">{controllerModeLabel(config.controller_type)}</Select.Trigger>
			<Select.Content>
				<Select.Group>
					{#each Object.values(ControllerType) as value}
						<Select.Item {value}>{controllerModeLabel(value)}</Select.Item>
					{/each}
				</Select.Group>
			</Select.Content>
		</Select.Root>
		</div>
		{#if Object.values(ControllerType).includes(config.controller_type)}
			<div class="mode-field mode-input">
				<InputModes bind:inputMode={config.iidx_input_mode} />
			</div>
		{/if}
	</div>

	{#if controllerRestarting}
		<WarningAlert
			title={tr('USBを再認識しています', 'Reconnecting USB')}
			description={tr('コントローラーを自動再起動しています。そのままお待ちください。', 'The controller is restarting automatically. Please wait.')}
		/>
	{/if}

	{#if Object.values(ControllerType).includes(config.controller_type)}
		<div class="settings-group">
			<h3 class="icon-panel-heading mb-2 text-xl font-bold"><HeadingIcon kind="turntable" />{tr('ターンテーブル', 'Turntable')}</h3>

			<Switch label={tr('回転方向を反転', 'Reverse Turntable')} bind:checked={config.reverse_tt} />
			{#if config.version >= 20}
				<Switch
					label={config.digital_tt ? tr('ターンテーブル入力：デジタル', 'Turntable Input: Digital') : tr('ターンテーブル入力：アナログ', 'Turntable Input: Analog')}
					bind:checked={config.digital_tt}
					helpWrap
				>
					<p>{tr('アナログは回転をX軸、デジタルは回転方向を入力します。', 'Analog sends rotation as X-axis values; digital sends rotation direction.')}</p>
					<p>{tr('デジタルはB13／B14、キーボードでは設定したキーを送信します。', 'Digital sends B13/B14 in Joystick mode, or assigned keys in Keyboard mode.')}</p>
					<p>{tr('デジタルは入力が途切れないよう、保持時間の設定を推奨します。', 'For digital input, set a hold time to help prevent gaps between inputs.')}</p>
				</Switch>
				{#if !config.digital_tt && config.iidx_input_mode === InputMode.Keyboard}
					<p class="mb-4 text-sm">{tr('アナログ入力を使う場合は、入力モードをジョイスティックにしてください。', 'Select Joystick input mode to use analog turntable input.')}</p>
				{/if}
			{/if}
			<div class="mb-4">
				<ToolTipLabel
					forId="tt-deadzone"
					wrap
					label={config.supports_encoder_deadzone
                        ? tr('ターンテーブル デッドゾーン（カウント）', 'Turntable Deadzone (counts)')
                        : config.supports_angle_deadzone
						? tr('ターンテーブル デッドゾーン（°）', 'Turntable Deadzone (°)')
						: config.version >= 22
							? tr('ターンテーブル デッドゾーン（ms）', 'Turntable Deadzone (ms)')
							: tr('ターンテーブル デッドゾーン', 'Turntable Deadzone')}
				>
					{#if config.supports_encoder_deadzone}
                        <p>{tr('A相・B相の有効な状態変化を1カウントとします。', 'Each valid quadrature transition counts as one encoder count.')}</p>
                        <p>{tr('設定数を超えると回転に追従します。逆回転も同じです。', 'Ignores the configured number of counts, then follows rotation. Reversals use the same threshold.')}</p>
                        <p>{tr('感度設定の影響は受けません。0で無効です。', 'Independent of sensitivity. Set 0 to disable.')}</p>
                    {:else if config.supports_angle_deadzone}
						<p>{tr('X軸の回転量を角度で判定します。', 'Uses the X-axis rotation angle for the deadzone.')}</p>
						<p>{tr('設定角度を超えるまで入力せず、その後は回転に追従します。', 'Waits until the angle threshold is crossed, then follows rotation.')}</p>
						<p>{tr('小さな逆振れは無視し、逆回転も設定角度で判定します。', 'Ignores small reverse jitter and applies the same angle threshold to reversals.')}</p>
						<p>{tr('0°で無効になり、すぐに入力します。', 'Set 0° to disable it and accept input immediately.')}</p>
					{:else}
						<p>{tr('短い回転や誤入力を無視する時間です。', 'Ignores brief turntable movement and false input.')}</p>
						<p>{tr('0msで無効になります。', 'Set 0 ms to disable it.')}</p>
					{/if}
				</ToolTipLabel>
				<SliderInput
					bind:value={config.tt_deadzone}
					min={config.version >= 22 ? 0 : 1}
					max={config.supports_encoder_deadzone ? 50 : config.supports_angle_deadzone ? 30 : config.version >= 22 ? 255 : 6}
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
				<ToolTipLabel forId="tt-ratio" wrap label={tr('ターンテーブル感度', 'Turntable Sensitivity')}>
                    {#if config.supports_multiplier}
                    <p>{tr('1.000が基準です。値が大きいほどX軸の入力が大きくなります。', '1.000 is the baseline; higher values produce more X-axis movement.')}</p>
                    <p>{tr('0.500で半分、2.000で2倍の入力量になります。', '0.500 produces half the movement; 2.000 produces twice as much.')}</p>
                    <p>{tr('0.000で回転入力を無効にします。', '0.000 disables turntable input.')}</p>
                    {:else if config.supports_extended_sensitivity}
                    <p>{tr('同じ回転量でも、感度が高いほどX軸の入力が大きくなります。', 'Higher sensitivity produces more X-axis movement for the same rotation.')}</p>
                    <p>{tr('感度6が基準です。1〜5は基準の約17〜83%になります。', '6 is the baseline; 1–5 produce about 17–83% of it.')}</p>
                    <p>{tr('感度7〜10は25%ずつ増え、10では基準の2倍になります。', '7–10 increase by 25% per step; 10 is twice the baseline.')}</p>
                    {:else}
                    <p>{tr('同じ回転量でも、感度が高いほどX軸の入力が大きくなります。', 'Higher sensitivity produces more X-axis movement for the same rotation.')}</p>
                    <p>{tr('感度10が基準です。5では半分、1では10分の1になります。', '10 is the baseline; 5 is half and 1 is one tenth.')}</p>
                    {/if}
				</ToolTipLabel>
				{#if config.supports_multiplier}
                <SliderInput bind:value={config.tt_multiplier} min={0} max={2} step={0.001} id="tt-ratio" />
                {:else}
                <SliderInput bind:value={config.tt_ratio} min={1} max={config.version >= 26 ? 10 : 6} id="tt-ratio" />
                {/if}
			</div>
			<TurntableTest
				active={active && settingsTab === 'input'}
				supported={config.version >= 24}
				digital={Boolean(config.digital_tt)}
				streaming={config.version >= 30}
			/>

			{#if config.version >= 15 && config.version < 34}
				<Separator class="mb-4" />

				<h3 class="icon-panel-heading mb-2 text-xl font-bold"><HeadingIcon kind="chatter" />{tr('チャタリング防止', 'Debouncing')}</h3>
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
  <Tabs.Content value="led" class="settings-card led-card">
	{#if Object.values(ControllerType).includes(config.controller_type)}
		<div>
			<div class="settings-group">
			<h3 class="icon-panel-heading mb-2 text-xl font-bold"><HeadingIcon kind="led" />{tr('ボタンLED設定', 'Button LED Settings')}</h3>

			<Switch id="button-led-disable" label={tr('LEDを無効にする', 'Disable LEDs')} bind:checked={config.disable_leds} />
			</div>
			{#if !config.disable_leds}
				<div class="settings-group button-led-settings">
				<h3 class="icon-panel-heading mb-2 text-xl font-bold"><HeadingIcon kind="led" />{tr('ボタンLED', 'Button LEDs')}</h3>
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
				</div>
            {/if}
        </div>
    {/if}
</Tabs.Content>
<Tabs.Content value="rgb" class="settings-card rgb-card">
    {#if Object.values(ControllerType).includes(config.controller_type)}
        <div class:legacy-rgb-layout={appState.boardInfo?.boardType === BoardType.RP2354B && config.legacy_phoenixwan_enabled}>
            <div class="settings-group">
                <h3 class="icon-panel-heading"><HeadingIcon kind="rgb" />{tr('RGB LED設定', 'RGB LED Settings')}</h3>
                <Switch id="rgb-led-disable" label={tr('LEDを無効にする', 'Disable LEDs')} bind:checked={config.disable_leds} />
            </div>
            {#if !config.disable_leds}
				{@const legacyPhoenixwanMode = appState.boardInfo?.boardType === BoardType.RP2354B && config.legacy_phoenixwan_enabled}
				{@const ttModeMapping = Object.values(TurntableMode)}
				<div class="settings-group">
				{#if config.version >= 19 && !legacyPhoenixwanMode}
					<Switch label={tr('ターンテーブルとセンターバーの発光効果を連動', 'Link Turntable and Light Bar Effects')} bind:checked={config.link_bar_effect} disabled={appState.boardInfo?.boardType === BoardType.RP2354B && config.legacy_phoenixwan_enabled} />
				{/if}
				{#if appState.boardInfo?.boardType === BoardType.RP2354B && (appState.boardInfo.capabilities & BoardCapability.LegacyPhoenixwan) !== 0}
					<Switch label={tr('旧Phoenixwanを使用', 'Use legacy Phoenixwan')} bind:checked={config.legacy_phoenixwan_enabled} onchange={setLegacyPhoenixwan} disabled={legacyPhoenixwanBusy} helpWrap>
						<p>{tr('ONにすると、旧PhoenixwanのLEDをUART接続で制御します。', 'When ON, controls legacy Phoenixwan LEDs over UART.')}</p>
						<p>{tr('OFFにしても、旧PhoenixwanのLEDは最後の点灯状態を保つ場合があります。', 'When OFF, legacy Phoenixwan LEDs may retain their last lighting state.')}</p>
                        <p>{tr('センターバーは固定発光で、色と点灯数を相対調整します。', 'The center bar uses fixed lighting with relative color and LED count adjustments.')}</p>
					</Switch>
				{/if}

				</div>
				<div class="rgb-effects-grid grid gap-x-6 gap-y-2 md:grid-cols-2">
					<div class="min-w-0 settings-group">
		<LightEffectSelect
							label={tr('ターンテーブルの発光効果', 'Turntable Effect')}
							bind:effect={config.tt_effect}
							modeMapping={legacyPhoenixwanMode ? legacyPhoenixwanTtModes : ttModeMapping}
							legacyPhoenixwan={legacyPhoenixwanMode}
		/>
		{#if legacyPhoenixwanMode}
			{#if config.version >= 42 && config.supports_bar_led_count}
				{#if config.tt_effect === TurntableMode.Static}
					<PhoenixwanColorControls mode={1} />
				{:else if config.tt_effect === TurntableMode.Spin}
					<PhoenixwanColorControls mode={2} />
				{:else if config.tt_effect === TurntableMode.Shift}
					<PhoenixwanColorControls mode={3} />
				{:else if config.tt_effect === TurntableMode.RainbowStatic}
					<PhoenixwanColorControls mode={4} />
				{:else if config.tt_effect === TurntableMode.RainbowReactive}
					<PhoenixwanColorControls mode={5} />
				{:else if config.tt_effect === TurntableMode.Reactive}
					<PhoenixwanColorControls mode={6} />
				{:else if config.tt_effect === TurntableMode.Breathing}
					<PhoenixwanColorControls mode={7} />
				{/if}
			{:else}
				<p class="mb-4 text-sm">{tr('色調整には旧Phoenixwan対応ファームウェアV1.00以降へ更新してください。', 'Update to firmware V1.00 or later with legacy Phoenixwan color adjustment support.')}</p>
			{/if}
						{:else if config.tt_effect === TurntableMode.Static}
							<ColorPicker bind:hsv={config.tt_static_hsv} />
						{:else if config.tt_effect === TurntableMode.Spin}
							<ColorPicker bind:hsv={config.tt_spin_hsv} />
						{:else if config.tt_effect === TurntableMode.Shift}
							<ColorPicker bind:hsv={config.tt_shift_hsv} disabled />
						{:else if config.tt_effect === TurntableMode.RainbowStatic}
							<ColorPicker bind:hsv={config.tt_rainbow_static_hsv} disabled />
						{:else if config.tt_effect === TurntableMode.RainbowReactive}
							<ColorPicker bind:hsv={config.tt_rainbow_react_hsv} disabled />
						{:else if config.tt_effect === TurntableMode.RainbowSpin}
							<ColorPicker bind:hsv={config.tt_rainbow_spin_hsv} disabled />
						{:else if config.tt_effect === TurntableMode.Reactive}
							<ColorPicker bind:hsv={config.tt_react_hsv} />
						{:else if config.tt_effect === TurntableMode.Breathing}
							<ColorPicker bind:hsv={config.tt_breathing_hsv} />
						{/if}
					</div>

					<div class="min-w-0 settings-group">
						{#if legacyPhoenixwanMode}
							<LightEffectSelect
								label={tr('センターバーの発光効果', 'Center Bar Effect')}
								effect={BarMode.Static}
								modeMapping={[BarMode.Static]}
							/>
							{#if config.version >= 42 && config.supports_bar_led_count}
								<PhoenixwanColorControls centerBar />
								<div class="legacy-bar-count mb-4 flex items-center justify-between gap-4">
								<ToolTipLabel forId="legacy-phoenixwan-bar-count" wrap label={tr('センターバーの点灯数', 'Center Bar LED Count')}>
									<p>{tr('「−／＋」で2灯ずつ変更します。', 'Use −/+ to change the count by 2 LEDs.')}</p>
									<p>{tr('0灯で消灯し、2〜16灯を中央から左右対称に点灯します。', 'Set 0 to turn the light off, or 2–16 LEDs to light symmetrically from the center.')}</p>
								</ToolTipLabel>
									<div class="flex items-center gap-2">
										<Button variant="outline" size="sm" aria-label={tr('点灯数を2灯減らす', 'Decrease by 2 LEDs')} disabled={config.legacy_phoenixwan_bar_count === 0} onclick={() => { if (config) config.legacy_phoenixwan_bar_count = Math.max(0, config.legacy_phoenixwan_bar_count - 2); }}>−</Button>
										<span id="legacy-phoenixwan-bar-count" class="min-w-12 text-center text-sm tabular-nums" aria-live="polite">
											{config.legacy_phoenixwan_bar_count === 0 ? tr('消灯', 'Off') : `${config.legacy_phoenixwan_bar_count}${tr('灯', ' LEDs')}`}
										</span>
										<Button variant="outline" size="sm" aria-label={tr('点灯数を2灯増やす', 'Increase by 2 LEDs')} disabled={config.legacy_phoenixwan_bar_count >= 16} onclick={() => { if (config) config.legacy_phoenixwan_bar_count = Math.min(16, config.legacy_phoenixwan_bar_count + 2); }}>＋</Button>
									</div>
								</div>
							{:else}
								<p class="mb-4 text-sm">{tr('センターバーの色と点灯数の調整には旧Phoenixwan対応ファームウェアV1.00以降へ更新してください。', 'Update to firmware V1.00 or later with legacy Phoenixwan center bar controls.')}</p>
							{/if}
						{:else if config.version >= 19 && config.link_bar_effect}
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

				{#if !legacyPhoenixwanMode && config.version >= 16}
					<div class="settings-group rgb-adjust-settings">
					<h3 class="icon-panel-heading"><HeadingIcon kind="adjust" />{tr('RGB LED調整', 'RGB LED Adjustments')}</h3>
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
							label={tr('変化速度', 'Color Change Speed')}
						>
							<p>{tr('回転・シフト・動くレインボー・呼吸の速度です。', 'Controls Spin, Shift, moving rainbow effects and Breathing.')}</p>
							<p>{tr('1がゆっくり、5が速い設定です。', '1 is slow; 5 is fast.')}</p>
							<p>{tr('固定色・固定レインボーには影響しません。', 'Does not affect fixed colors or a static rainbow.')}</p>
						</ToolTipLabel>
						<SliderInput
							bind:value={config.rainbow_spin_speed}
							min={1}
							max={5}
							id="rainbow-spin-speed"
						/>
					</div>

					<div class="mb-4">
						<ToolTipLabel forId="tt-leds" wrap label={tr('ターンテーブルLED数', 'Turntable LEDs')}>
							<p>{tr('ターンテーブルのRGB LED数を設定します。', 'Sets the number of turntable RGB LEDs.')}</p>
							<p>{tr('接続しているLED数に合わせてください。', 'Match the number of LEDs connected.')}</p>
							<p>{tr('初期値は24。0にすると消灯します。', 'Default: 24. Set 0 to turn them off.')}</p>
						</ToolTipLabel>
						<SliderInput bind:value={config.tt_leds} min={0} max={255} id="tt-leds" />
					</div>
					<div class="mb-4">
						<ToolTipLabel forId="bar-leds" wrap label={tr('センターバーLED数', 'Center Bar LEDs')}>
							<p>{tr('センターバーのRGB LED数を設定します。', 'Sets the number of center bar RGB LEDs.')}</p>
							<p>{tr('接続しているLED数に合わせてください。', 'Match the number of LEDs connected.')}</p>
							<p>{tr('初期値は16。0にすると消灯します。', 'Default: 16. Set 0 to turn them off.')}</p>
						</ToolTipLabel>
						{#if config.supports_bar_led_count}<SliderInput bind:value={config.bar_leds} min={0} max={255} id="bar-leds" />{:else}<span>{tr('ファーム更新が必要です', 'Firmware update required')}</span>{/if}
					</div>
					</div>
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
  <Tabs.Content value="chatter" class="settings-card chatter-card"><ChatterTest {config} active={active && settingsTab === 'chatter'} /></Tabs.Content>
</Tabs.Root>

{/if}

<style>
  .rgb-effects-grid > :global(.settings-group) { min-height: 142px; }
  :global(.rgb-card .rgb-adjust-settings) { display: grid; grid-template-columns: repeat(2, minmax(0, 1fr)); column-gap: 20px; }
  :global(.rgb-card .rgb-adjust-settings > h3) { grid-column: 1 / -1; }
  :global(.rgb-card [data-slot="select-trigger"]) { width: 100%; }
  :global(.rgb-card .settings-group > .mb-4:not(.flex):not(.cp-dark)) { margin-bottom: 10px; }
  :global(.rgb-card .settings-group > .mb-4 > .mt-2.grid) { margin-top: 4px; }
  :global(.rgb-card .settings-group input[type="number"]) { height: 30px; }
  @media (max-height: 650px) {
    .rgb-effects-grid > :global(.settings-group) { min-height: 130px; }
    :global(html[data-design="blue-line"] .rgb-card .settings-group) { padding: 8px 10px !important; margin-bottom: 6px !important; }
    :global(.rgb-card .settings-group h3) { margin-bottom: 6px; }
    :global(html[data-design="blue-line"] .rgb-card .settings-group > .flex.items-center.justify-between) { padding: 4px 10px; }
    :global(.rgb-card .settings-group > .mb-4:not(.flex):not(.cp-dark)) { margin-bottom: 6px; }
    :global(.rgb-card .settings-group > .mb-4 > .mt-2.grid) { margin-top: 2px; }
    :global(.rgb-card .settings-group input[type="number"]) { height: 28px; }
  }
  @media (min-width: 681px) {
    .rgb-effects-grid { grid-template-columns: repeat(2, minmax(0, 1fr)); }
  }
  @media (max-width: 560px) {
    :global(.rgb-card .rgb-adjust-settings) { grid-template-columns: 1fr; }
  }
  .mode-settings { display: grid; grid-template-columns: repeat(2, minmax(0, 1fr)); gap: 16px; align-items: start; }
  .mode-field { min-width: 0; }
  .mode-input :global(.mb-4) { margin-bottom: 0; }
  .mode-input :global([data-slot="select-trigger"]) { width: 100%; min-width: 0; }
  :global(html[data-design="blue-line"] .input-card .settings-group) { padding: 10px 12px !important; margin-bottom: 8px !important; }
  :global(html[data-design="blue-line"] .input-card .settings-group > .mb-4) { margin-bottom: 8px; }
  :global(html[data-design="blue-line"] .input-card .settings-group h3) { margin-bottom: 6px; }
  :global(html[data-design="blue-line"] .input-card .settings-group > .flex.items-center.justify-between) { padding: 6px 12px; }
  :global(html[data-design="blue-line"] .input-card .settings-group > .mb-4 > .mt-2.grid) { margin-top: 4px; }
  :global(.input-card .mode-settings) { gap: 12px; }
  @media (max-height: 650px) {
    :global(html[data-design="blue-line"] .input-card .settings-group) { padding: 6px 10px !important; margin-bottom: 6px !important; }
    :global(html[data-design="blue-line"] .input-card .settings-group > .mb-4) { margin-bottom: 5px; }
    :global(html[data-design="blue-line"] .input-card .settings-group > .flex.items-center.justify-between) { padding: 4px 10px; }
    :global(html[data-design="blue-line"] .input-card .settings-group > .mb-4 > .mt-2.grid) { margin-top: 2px; }
    :global(html[data-design="blue-line"] .input-card .settings-group input[type="number"]) { height: 28px; }
  }
  @media (max-height: 620px) {
    :global(html[data-design="blue-line"] .input-card .settings-group > .mb-4:not(.flex)) { display: grid; grid-template-columns: minmax(0, 1fr) minmax(0, 1.1fr); gap: 10px; align-items: center; }
    :global(html[data-design="blue-line"] .input-card .settings-group > .mb-4:not(.flex) > .mt-2.grid) { margin-top: 0; }
    :global(html[data-design="blue-line"] .input-card .settings-group label) { font-size: 12px; }
  }
  :global(.settings-card) { border: 0; border-radius: 0; padding: 0; background: transparent; }
  :global(.settings-card .mb-4) { margin-bottom: 0.75rem; }
  :global(.settings-card h2), :global(.settings-card h3) { font-size: 1rem; margin-top: 0; margin-bottom: 0.75rem; }
  :global(.settings-card label) { line-height: 1.5; }
  :global(.keys-card), :global(.chatter-card) { padding: 0; }
  :global(.app-content:has(.chatter-card[data-state="active"]) > .flex.flex-col.gap-2) { display: flex; flex-direction: column; min-height: 0; height: 100%; }
  :global(.chatter-card[data-state="active"]) { display: flex; flex: 1 1 auto; flex-direction: column; min-height: 0; }

  :global(.legacy-rgb-layout .rgb-effects-grid) { grid-template-columns: repeat(2, minmax(0, 1fr)); gap: 12px; }
  @media (max-height: 650px) {
    :global(html[data-design="blue-line"] .legacy-rgb-layout .settings-group) { padding: 6px 10px !important; margin-bottom: 6px !important; }
    :global(.legacy-rgb-layout .settings-group > .mb-4) { margin-bottom: 8px !important; }
    :global(html[data-design="blue-line"] .legacy-rgb-layout .settings-group > .flex.items-center.justify-between) { padding: 5px 8px; }
    :global(.legacy-rgb-layout .settings-group h3) { margin-bottom: 8px; }
  }

  :global(.legacy-rgb-layout > .settings-group:first-child) { display: flex; align-items: center; justify-content: space-between; gap: 12px; }
  :global(.legacy-rgb-layout > .settings-group:first-child h3) { margin: 0; white-space: nowrap; }
  :global(.legacy-rgb-layout > .settings-group:first-child > .flex) { margin: 0; }
  :global(.legacy-rgb-layout .legacy-bar-count) { flex-direction: column; align-items: stretch; gap: 4px; padding: 0 !important; background: transparent !important; }
  :global(.legacy-rgb-layout .legacy-bar-count > .flex:last-child) { justify-content: flex-end; }
  :global(.legacy-rgb-layout .legacy-bar-count label) { white-space: nowrap; font-size: 12px; }
  :global(.legacy-rgb-layout .settings-group > .flex.items-center.justify-between) { padding: 5px 8px; }
  @media (max-height: 650px) {
    :global(.legacy-rgb-layout .settings-group label) { font-size: 12px; }
    :global(.legacy-rgb-layout .rgb-effects-grid) { gap: 8px; }
    :global(.legacy-rgb-layout .rgb-effects-grid button) { height: 28px !important; min-height: 28px !important; }
    :global(.legacy-rgb-layout .rgb-effects-grid [data-slot="select-trigger"]) { height: 30px; min-height: 30px; }
    :global(.legacy-rgb-layout .settings-group > .mb-4:not(.flex)) { margin-bottom: 6px !important; }
  }
</style>
