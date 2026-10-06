<script lang="ts">
	import { onMount } from 'svelte';
	import { Settings, Sun, Keyboard, Gamepad2, Activity, Cpu, RotateCcw, Monitor } from '@lucide/svelte';

	import { Alert, AlertDescription, AlertTitle } from '$lib/components/ui/alert';
	import * as AlertDialog from '$lib/components/ui/alert-dialog/index.js';
	import { Button } from '$lib/components/ui/button';
	import { Skeleton } from '$lib/components/ui/skeleton';

	import ConfigScreen from '$lib/ConfigScreen.svelte';
	import FirmwareScreen from '$lib/FirmwareScreen.svelte';
	import NativeFirmwareScreen from '$lib/NativeFirmwareScreen.svelte';
	import Rp2354bFirmwareScreen from '$lib/Rp2354bFirmwareScreen.svelte';
	import LightDarkModeToggle from '$lib//LightDarkModeToggle.svelte';
	import LanguageSelect from '$lib/LanguageSelect.svelte';
	import { initializeLocale, tr } from '$lib/types/locale.svelte';
	import { formatDisplayVersion } from '$lib/types/version';
	import { Command, readBoardInfo, readFirmwareInfo, sendCommand, waitForReconnection, type FirmwareInfo } from '$lib/types/hid';
	import { isRp2354bBoard, RP2354B_BEEF_PRODUCT_ID, RP2354B_BEEF_VENDOR_ID } from '$lib/types/board';

	import { connectDevice, reconnectDevice, reconnectDfuDevice, appState } from '$lib/types/state.svelte';

	const Tab = {
		Config: 'config',
		DeviceInfo: 'device-info',
		Firmware: 'dfu'
	} as const;

	let browserSupported = $state(false);
	let activeTab = $state<string>(Tab.Config);
	let settingsSection = $state('input');
	let loading = $state(true);
	let nativeFlash = $state(false);
	let appVersion = $state('');
	let updateInfo = $state<Awaited<ReturnType<NonNullable<Window['beefNative']>['checkForUpdates']>>>(null);
	let firmwareInfo = $state<FirmwareInfo | null>(null);
	let resetDialogOpen = $state(false);
	let firmwareTarget = $state<'auto' | 'rp2354b' | 'at90'>('auto');
	let lastAutoFirmwareTarget = $state<'rp2354b' | 'at90'>('at90');
	let lastConnectedDevice: HIDDevice | null = null;

	onMount(async () => {
		initializeLocale();
		nativeFlash = !!window.beefNative;
		checkBrowserSupport();
		loading = false;
		if (window.beefNative) {
			appVersion = await window.beefNative.getVersion();
			updateInfo = await window.beefNative.checkForUpdates();
		}
	});

	function checkBrowserSupport(): void {
		browserSupported = 'hid' in navigator && 'usb' in navigator;
	}

	function controllerDisplayName(device: HIDDevice | null): string {
		if (!device) return 'BEEF BOARD';
		if (device.vendorId === 0x1ccf && device.productId === 0x1018) return 'IIDX Entry Model';
		if (device.vendorId === 0x1ccf && device.productId === 0x8048) return 'IIDX Premium Model';
		return device.productName || 'BEEF BOARD';
	}

	function connectedDeviceLabel(device: HIDDevice | null): string {
		const controller = controllerDisplayName(device);
		return appState.boardInfo ? `${controller} · ${appState.boardInfo.mcu}` : controller;
	}

	function deviceSerialNumber(device: HIDDevice): string | null {
		return (device as HIDDevice & { serialNumber?: string }).serialNumber ?? null;
	}

	async function resetConfiguration(): Promise<void> {
		if (appState.connecting) return;
		const deviceBeforeReset = appState.device;
		const reconnectIdentities = deviceBeforeReset
			? [
				{ vendorId: deviceBeforeReset.vendorId, productId: deviceBeforeReset.productId },
				// ResetConfig restores controller mode to Default. When the board is
				// currently using an IIDX VID/PID, Board Info can be unavailable on
				// Windows, so appState.boardInfo may have fallen back to AT90 even for
				// an RP2354B. Accept both supported default identities after reset.
				{ vendorId: 0xfeed, productId: 0x0000 },
				{ vendorId: RP2354B_BEEF_VENDOR_ID, productId: RP2354B_BEEF_PRODUCT_ID }
			]
			: undefined;
		// Dismiss the confirmation immediately; USB reset/re-enumeration can take
		// several seconds and must not keep the modal open while it completes.
		resetDialogOpen = false;
		appState.connecting = true;
		appState.error = undefined;
		try {
			try {
				await sendCommand(Command.ResetConfig);
			} catch {
				// Reset reboots USB immediately; the report can reject after the
				// board has already accepted the command. Still wait for it to return.
			}
			await waitForReconnection(20000, reconnectIdentities, deviceBeforeReset);
		} catch (err) {
			appState.error = `${tr('初期化後に基板へ再接続できませんでした', 'Could not reconnect to the board after reset')}: ${err}`;
		} finally {
			appState.connecting = false;
			resetDialogOpen = false;
		}
	}

	$effect(() => {
		const device = appState.device;
		if (device && device !== lastConnectedDevice) activeTab = Tab.DeviceInfo;
		lastConnectedDevice = device;
	});

	$effect(() => {
		const device = appState.device;
		if (!device) return;

		// Keep the RP2354B target selected while ROM BOOTSEL temporarily removes
		// the normal HID device.
		lastAutoFirmwareTarget = isRp2354bBoard(appState.boardInfo) ||
			(device.vendorId === RP2354B_BEEF_VENDOR_ID && device.productId === RP2354B_BEEF_PRODUCT_ID)
				? 'rp2354b'
				: 'at90';
	});

	function useRp2354bFirmware(): boolean {
		const device = appState.device;
		if (isRp2354bBoard(appState.boardInfo) ||
			(device?.vendorId === RP2354B_BEEF_VENDOR_ID && device.productId === RP2354B_BEEF_PRODUCT_ID)) return true;
		if (firmwareTarget === 'rp2354b') return true;
		return !device && firmwareTarget === 'auto' && lastAutoFirmwareTarget === 'rp2354b';
	}

	$effect(() => {
		const current = appState.device;
		if (!current) {
			firmwareInfo = null;
			return;
		}
		void readFirmwareInfo().then((info) => {
			if (appState.device === current) firmwareInfo = info;
		}).catch(() => {
			if (appState.device === current) firmwareInfo = null;
		});
	});
</script>

<main class="app-shell">
	<title>Beef Board Configurator</title>
	<div class="app-header">
		<div class="header-title-row">
			<span class="brand-icon" aria-hidden="true"><Monitor size={28} strokeWidth={1.8} /></span>
			<h1 class="flex items-baseline gap-2 text-2xl font-bold">
				<span>Beef Board Configurator</span>{#if appVersion}<span class="text-base font-semibold text-muted-foreground">{formatDisplayVersion(appVersion)}</span>{/if}
			</h1>
			<Button
				variant="outline"
				class="reset-action-button"
				onclick={() => appState.device ? reconnectDevice() : (appState.dfuReconnectAvailable ? reconnectDfuDevice() : connectDevice())}
				disabled={appState.disableConfigTab || appState.connecting}
			>
				{#if appState.connecting}
					{tr('接続中…', 'Connecting…')}
				{:else if appState.device || appState.dfuReconnectAvailable}
					{tr('再接続', 'Reconnect')}
				{:else}
					{tr('接続', 'Connect')}
				{/if}
			</Button>
		</div>
		<div class="header-controls">
			<LanguageSelect />
			<LightDarkModeToggle />
		</div>
	</div>
	{#if updateInfo?.updateAvailable}
		<Alert class="mb-4">
			<AlertTitle>{tr('アップデートがあります', 'Update available')}</AlertTitle>
			<AlertDescription class="flex flex-wrap items-center justify-between gap-3">
				<span>{tr(`最新版 ${updateInfo.latestVersion} が利用できます。`, `Version ${updateInfo.latestVersion} is available.`)}</span>
				<Button variant="outline" size="sm" onclick={() => window.beefNative?.openUpdate(updateInfo!.releaseUrl)}>
					{tr('Releaseを開く', 'Open Release')}
				</Button>
			</AlertDescription>
		</Alert>
	{/if}

	{#if loading}
		<Skeleton class="h-10" />
	{:else if !browserSupported}
		<Alert variant="destructive">
			<AlertTitle>Browser Not Supported</AlertTitle>
			<AlertDescription>
				WebHID/WebUSB is not supported in this browser. Please use a compatible browser like Chrome
				or Edge.
			</AlertDescription>
		</Alert>
	{:else}
		<div class="desktop-layout">
			<aside class="app-sidebar">
				<button
					class:active={activeTab === Tab.DeviceInfo}
					class="sidebar-device-card"
					type="button"
					aria-label={tr('ボード情報を表示', 'Show board information')}
					aria-current={activeTab === Tab.DeviceInfo ? 'page' : undefined}
					onclick={() => { activeTab = Tab.DeviceInfo; }}
				>
					<div class="device-status-row">
						<span class:connected={!!appState.device} class="status-dot"></span>
						<div>
							<div class="status-title">{tr('ボード', 'Board')}</div>
							<div class="status-subtitle">{appState.device ? connectedDeviceLabel(appState.device) : tr('未接続', 'Disconnected')}</div>
						</div>
					</div>
				</button>

				<nav class="sidebar-nav" aria-label={tr('メインメニュー', 'Main menu')}>
					<div class="sidebar-label">{tr('コントローラー設定', 'CONTROLLER')}</div>
					<button class:active={activeTab === Tab.Config && settingsSection === 'input'} class="sidebar-item" disabled={!appState.device || appState.disableConfigTab} onclick={() => { activeTab = Tab.Config; settingsSection = 'input'; }}>
						<span class="nav-icon" aria-hidden="true"><Settings size={23} /></span>
						<span class="nav-title">{tr('入力設定', 'Input')}</span>
						<span class="nav-subtitle">{tr('入力モード・ターンテーブル', 'Mode & turntable')}</span>
					</button>
					<button class:active={activeTab === Tab.Config && settingsSection === 'led'} class="sidebar-item" disabled={!appState.device || appState.disableConfigTab} onclick={() => { activeTab = Tab.Config; settingsSection = 'led'; }}>
						<span class="nav-icon" aria-hidden="true"><Sun size={23} /></span>
						<span class="nav-title">{tr('LED設定', 'LEDs')}</span>
						<span class="nav-subtitle led-nav-subtitle"><span>{tr('ボタン・ターンテーブル', 'Buttons, turntable')}</span><span>{tr('・センターバー', ' & bar')}</span></span>
					</button>
					<button class:active={activeTab === Tab.Config && settingsSection === 'keys'} class="sidebar-item" disabled={!appState.device || appState.disableConfigTab} onclick={() => { activeTab = Tab.Config; settingsSection = 'keys'; }}>
						<span class="nav-icon" aria-hidden="true"><Keyboard size={23} /></span>
						<span class="nav-title">{tr('キー割り当て', 'Key Bindings')}</span>
						<span class="nav-subtitle">{tr('キー・ボタン配置', 'Keys & button layout')}</span>
					</button>
					<button class:active={activeTab === Tab.Config && settingsSection === 'monitor'} class="sidebar-item" disabled={!appState.device || appState.disableConfigTab} onclick={() => { activeTab = Tab.Config; settingsSection = 'monitor'; }}>
						<span class="nav-icon" aria-hidden="true"><Gamepad2 size={23} /></span>
						<span class="nav-title">{tr('コントローラーモニター', 'Controller Monitor')}</span>
						<span class="nav-subtitle">{tr('入力状態をリアルタイム表示', 'Live input status')}</span>
					</button>
					<div class="sidebar-label sidebar-label-spaced">{tr('メンテナンス', 'MAINTENANCE')}</div>
					<button class:active={activeTab === Tab.Config && settingsSection === 'chatter'} class="sidebar-item" disabled={!appState.device || appState.disableConfigTab} onclick={() => { activeTab = Tab.Config; settingsSection = 'chatter'; }}>
						<span class="nav-icon" aria-hidden="true"><Activity size={23} /></span>
						<span class="nav-title">{tr('チャタリングテスト', 'Chatter Test')}</span>
						<span class="nav-subtitle">{tr('個別設定・入力エッジ・ログ', 'Per-button settings & input log')}</span>
					</button>
					<button class:active={activeTab === Tab.Firmware} class="sidebar-item" onclick={() => { activeTab = Tab.Firmware; }}>
						<span class="nav-icon" aria-hidden="true"><Cpu size={23} /></span>
						<span class="nav-title firmware-nav-title">
							<span>{tr('ファームウェア', 'Firmware')}</span>
							{#if appState.device && firmwareInfo}
								<span class="firmware-nav-version">{firmwareInfo.version ? formatDisplayVersion(firmwareInfo.version) : '-'}</span>
							{/if}
						</span>
						<span class="nav-subtitle">{tr('更新・書き込み', 'Update & flash')}</span>
					</button>

				</nav>

				<div class="sidebar-reset">
					<AlertDialog.Root bind:open={resetDialogOpen}>
						<AlertDialog.Trigger>
							{#snippet child({ props })}
								<button
									{...props}
									class="sidebar-item sidebar-reset-button"
									disabled={!appState.device || appState.disableConfigTab}
								>
									<span class="nav-icon" aria-hidden="true"><RotateCcw size={23} /></span>
									<span class="nav-title">{tr('設定を初期化', 'Reset Config')}</span>
									<span class="nav-subtitle">{tr('すべての設定を初期値に戻す', 'Restore all settings to defaults')}</span>
								</button>
							{/snippet}
						</AlertDialog.Trigger>
						<AlertDialog.Content>
							<AlertDialog.Header>
								<AlertDialog.Title>{tr('設定を初期化しますか？', 'Reset configuration?')}</AlertDialog.Title>
								<AlertDialog.Description>
									{tr('すべての設定を初期値に戻し、コントローラーを切断します。この操作は元に戻せません。', 'This resets all settings to defaults and disconnects the controller. This action cannot be undone.')}
								</AlertDialog.Description>
							</AlertDialog.Header>
							<AlertDialog.Footer>
								<Button
									variant="outline"
					class="reset-action-button"
					onclick={resetConfiguration}
					disabled={appState.connecting}
								>
									{tr('続行', 'Continue')}
								</Button>
								<AlertDialog.Cancel>{tr('キャンセル', 'Cancel')}</AlertDialog.Cancel>
							</AlertDialog.Footer>
						</AlertDialog.Content>
					</AlertDialog.Root>
				</div>
			</aside>

			<section class="app-content">
				{#if activeTab === Tab.DeviceInfo}
					<section class="device-info-page">
						<div class="device-info-heading">
							<div>
								<p class="device-info-eyebrow">{tr('デバイス', 'DEVICE')}</p>
								<h2>{tr('接続中の基板', 'Connected board')}</h2>
							</div>
							<span class:connected={!!appState.device} class="device-info-status">
								<span class="status-dot"></span>
								{appState.device ? tr('接続中', 'Connected') : tr('未接続', 'Disconnected')}
							</span>
						</div>

						{#if appState.device}
							<div class="device-hero-card">
								<div class="device-hero-icon" aria-hidden="true">B</div>
								<div>
									<p>{tr('基板名', 'Board')}</p>
									<h3>{connectedDeviceLabel(appState.device)}</h3>
								</div>
							</div>
							<div class="device-info-grid">
								<div class="device-info-card"><span>{tr('ファームウェア', 'Firmware')}</span><strong>{firmwareInfo?.version ? formatDisplayVersion(firmwareInfo.version) : '—'}</strong></div>
								<div class="device-info-card"><span>{tr('マイコン', 'MCU')}</span><strong>{appState.boardInfo?.mcu ?? '—'}</strong></div>
								<div class="device-info-card"><span>{tr('ハードウェア改訂', 'Hardware revision')}</span><strong>{appState.boardInfo?.hardwareRevision ?? tr('不明', 'Unknown')}</strong></div>
								<div class="device-info-card"><span>{tr('ブートローダー', 'Bootloader')}</span><strong>{appState.boardInfo?.bootloader ?? '—'}</strong></div>
								<div class="device-info-card"><span>USB ID（VID / PID）</span><strong>VID 0x{appState.device.vendorId.toString(16).padStart(4, '0').toUpperCase()}　/　PID 0x{appState.device.productId.toString(16).padStart(4, '0').toUpperCase()}</strong></div>
								<div class="device-info-card"><span>{tr('USB製品名', 'USB product name')}</span><strong>{appState.device.productName || tr('不明', 'Unknown')}</strong></div>
								<div class="device-info-card"><span>{tr('シリアル番号', 'Serial number')}</span><strong>{deviceSerialNumber(appState.device) || tr('取得できません', 'Unavailable')}</strong></div>
							</div>
							<div class="device-info-footer">
								<span>{tr('USB HIDで基板を認識しています', 'Board detected over USB HID')}</span>
							</div>
						{:else}
							<div class="empty-state">
							<h3>{tr('基板が接続されていません', 'No board connected')}</h3>
								<p>{tr('「接続」ボタンから基板を接続すると、デバイス情報がここに表示されます。', 'Connect a board to view its device information here.')}</p>
							</div>
						{/if}
					</section>
				{:else if activeTab === Tab.Config}
					{#if appState.device}
						<ConfigScreen active={true} bind:settingsTab={settingsSection} showTabs={false} />
					{:else}
						<div class="empty-state">
							<h2>{tr('BEEF BOARDを接続してください', 'Connect your BEEF BOARD')}</h2>
							<p>{tr('「接続」ボタンからコントローラーに接続すると設定を変更できます。', 'Use the Connect button on the left to start configuring the controller.')}</p>
						</div>
					{/if}
				{:else}
					{#if nativeFlash}
						<div class="mb-4 flex flex-wrap items-center gap-2">
							<span class="text-sm font-semibold">{tr('書き込み対象', 'Firmware target')}:</span>
							<Button
							variant="outline"
							class="reset-action-button"
							onclick={() => firmwareTarget = 'rp2354b'}
							disabled={firmwareTarget === 'rp2354b' || (firmwareTarget === 'auto' && useRp2354bFirmware())}
							>
							RP2354B
							</Button>
							<Button
								variant="outline"
								class="reset-action-button"
								onclick={() => firmwareTarget = 'at90'}
								disabled={firmwareTarget === 'at90' || (firmwareTarget === 'auto' && !useRp2354bFirmware())}
							>
								AT90USB1286
							</Button>
							{#if firmwareTarget !== 'auto'}
								<Button variant="ghost" onclick={() => firmwareTarget = 'auto'}>
									{tr('自動判定に戻す', 'Use auto detection')}
								</Button>
							{/if}
						</div>
						{#if useRp2354bFirmware()}
							<Rp2354bFirmwareScreen />
						{:else}
							<NativeFirmwareScreen />
						{/if}
					{:else}
						{#if useRp2354bFirmware()}
							<Rp2354bFirmwareScreen />
						{:else}
							<FirmwareScreen />
						{/if}
					{/if}
				{/if}

				{#if appState.error}
					<Alert variant="destructive" class="mt-4">
						<AlertTitle>{tr('エラー', 'Error')}</AlertTitle>
						<AlertDescription>{appState.error}</AlertDescription>
					</Alert>
				{/if}
			</section>
		</div>
	{/if}
</main>

<style>
	.app-shell {
		width: min(1500px, 100%);
		height: 100dvh;
		max-height: 100dvh;
		margin: 0 auto;
		padding: 18px 22px 28px;
		display: flex;
		flex-direction: column;
		overflow: hidden;
	}
	.header-title-row {
		display: flex;
		align-items: center;
		gap: 12px;
		min-width: 0;
	}
	.app-header {
		display: flex;
		align-items: center;
		justify-content: space-between;
		gap: 18px;
		margin-bottom: 18px;
		flex: 0 0 auto;
	}
	.desktop-layout {
		display: grid;
		grid-template-columns: 250px minmax(0, 1fr);
		gap: 20px;
		align-items: stretch;
		flex: 1 1 auto;
		min-height: 0;
		overflow: hidden;
	}
	.app-sidebar {
		position: static;
		min-height: 0;
		height: 100%;
		border: 1px solid var(--border);
		border-radius: 18px;
		background: var(--card);
		padding: 14px;
		box-shadow: 0 10px 30px rgba(15, 23, 42, 0.08);
		display: flex;
		flex-direction: column;
		overflow: hidden;
	}
	.sidebar-device-card {
		width: 100%;
		border: 1px solid transparent;
		border-radius: 12px;
		background: transparent;
		color: inherit;
		padding: 6px 6px 14px;
		border-bottom: 1px solid var(--border);
		text-align: left;
		cursor: pointer;
		transition: background 120ms ease, border-color 120ms ease;
	}
	.sidebar-device-card:hover,
	.sidebar-device-card:focus-visible {
		background: color-mix(in srgb, #3b82f6 7%, transparent);
		border-color: color-mix(in srgb, #3b82f6 26%, transparent);
		outline: none;
	}
	.sidebar-device-card.active {
		border-color: color-mix(in srgb, #3b82f6 28%, var(--border));
		background: color-mix(in srgb, #3b82f6 9%, var(--card));
		color: #2563eb;
		box-shadow: inset 4px 0 0 #3b82f6;
	}
	:global(.dark) .sidebar-device-card.active {
		border-color: color-mix(in srgb, #93c5fd 35%, var(--border));
		background: color-mix(in srgb, #3b82f6 14%, var(--card));
		color: #93c5fd;
	}
	.device-status-row {
		display: flex;
		align-items: center;
		gap: 10px;
	}
	.status-dot {
		width: 10px;
		height: 10px;
		border-radius: 999px;
		background: #94a3b8;
		box-shadow: 0 0 0 4px rgba(148, 163, 184, 0.16);
		flex: 0 0 auto;
	}
	.status-dot.connected {
		background: #22c55e;
		box-shadow: 0 0 0 4px rgba(34, 197, 94, 0.14);
	}
	.status-title {
		font-size: 14px;
		font-weight: 700;
	}
	.status-subtitle,
	.nav-subtitle {
		font-size: 11px;
		color: var(--muted-foreground);
	}
	.sidebar-nav {
		display: flex;
		flex-direction: column;
		gap: 5px;
		padding-top: 14px;
	}
	.sidebar-label {
		padding: 0 9px 5px;
		font-size: 10px;
		font-weight: 800;
		letter-spacing: 0.11em;
		color: var(--muted-foreground);
	}
	.sidebar-label-spaced {
		margin-top: 10px;
	}
	.sidebar-item {
		width: 100%;
		border: 1px solid transparent;
		border-radius: 11px;
		background: transparent;
		padding: 10px 11px;
		text-align: left;
		transition: background 120ms ease, border-color 120ms ease, transform 120ms ease;
	}
	.sidebar-item:not(:disabled):hover {
		background: color-mix(in srgb, #3b82f6 7%, transparent);
		border-color: color-mix(in srgb, #3b82f6 26%, transparent);
		color: #2563eb;
		transform: translateX(1px);
	}
	:global(.dark) .sidebar-item:not(:disabled):hover {
		background: color-mix(in srgb, #3b82f6 10%, transparent);
		color: #93c5fd;
	}
	.sidebar-item.active {
		border-color: color-mix(in srgb, #3b82f6 28%, var(--border));
		background: color-mix(in srgb, #3b82f6 9%, var(--card));
		color: #2563eb;
		box-shadow: inset 4px 0 0 #3b82f6;
	}
	.sidebar-item.active .nav-title {
		font-weight: 850;
	}
	.sidebar-item.active:not(:disabled):hover {
		background: color-mix(in srgb, #3b82f6 13%, var(--card));
		color: #2563eb;
	}
	:global(.dark) .sidebar-item.active {
		border-color: color-mix(in srgb, #93c5fd 35%, var(--border));
		background: color-mix(in srgb, #3b82f6 14%, var(--card));
		color: #93c5fd;
		box-shadow: inset 4px 0 0 #60a5fa;
	}
	:global(.dark) .sidebar-item.active:not(:disabled):hover {
		background: color-mix(in srgb, #3b82f6 20%, var(--card));
		color: #bfdbfe;
	}
	.sidebar-reset {
		margin-top: 16px;
		padding-top: 0;
	}
	.sidebar-reset-button {
		margin-top: 4px;
	}
	.sidebar-reset-button .nav-subtitle {
		color: #64748b;
	}
	.sidebar-item:disabled {
		opacity: 0.42;
		cursor: not-allowed;
	}
	.nav-title {
		display: block;
		font-size: 14px;
		font-weight: 750;
		line-height: 1.35;
	}
	.firmware-nav-title {
		display: flex;
		align-items: baseline;
		justify-content: space-between;
		gap: 8px;
	}
	.firmware-nav-version {
		font-size: 11px;
		font-weight: 700;
		color: var(--muted-foreground);
		white-space: nowrap;
	}
	.nav-subtitle {
		display: block;
		margin-top: 2px;
		line-height: 1.35;
	}
	.led-nav-subtitle {
		font-size: 10px;
		white-space: nowrap;
	}
	.app-content {
		min-width: 0;
		min-height: 0;
		overflow-y: auto;
		overflow-x: hidden;
		overscroll-behavior: contain;
		scrollbar-width: none;
	}
	.app-content::-webkit-scrollbar { display: none; }
	.empty-state {
		min-height: 340px;
		display: grid;
		place-content: center;
		gap: 8px;
		text-align: center;
		border: 1px dashed var(--border);
		border-radius: 18px;
		background: var(--card);
		padding: 32px;
	}
	.empty-state h2 {
		font-size: 20px;
		font-weight: 800;
	}
	.empty-state p {
		max-width: 520px;
		color: var(--muted-foreground);
	}
	.device-info-page { display: flex; flex-direction: column; gap: 18px; }
	.device-info-heading { display: flex; justify-content: space-between; align-items: center; gap: 16px; }
	.device-info-heading h2 { font-size: 25px; font-weight: 800; letter-spacing: -0.02em; }
	.device-info-eyebrow { color: var(--muted-foreground); font-size: 10px; font-weight: 800; letter-spacing: .12em; }
	.device-info-status { display: inline-flex; align-items: center; gap: 9px; padding: 8px 12px; border: 1px solid var(--border); border-radius: 999px; background: var(--card); font-size: 12px; font-weight: 700; }
	.device-info-status .status-dot { width: 8px; height: 8px; margin: 0; box-shadow: none; }
	.device-info-status.connected { color: #15803d; border-color: color-mix(in srgb, #22c55e 35%, var(--border)); }
	.device-hero-card { display: flex; align-items: center; gap: 16px; padding: 24px; border: 1px solid var(--border); border-radius: 18px; background: linear-gradient(120deg, color-mix(in srgb, #14b8a6 9%, var(--card)), var(--card) 65%); box-shadow: 0 10px 30px rgba(15, 23, 42, .05); }
	.device-hero-icon { display: grid; place-items: center; width: 56px; height: 56px; border-radius: 16px; background: #0f766e; color: white; font-size: 28px; font-weight: 800; }
	.device-hero-card p, .device-info-card span { color: var(--muted-foreground); font-size: 12px; }
	.device-hero-card h3 { font-size: 21px; font-weight: 800; }
	.device-info-grid { display: grid; grid-template-columns: repeat(2, minmax(0, 1fr)); gap: 12px; }
	.device-info-card { display: flex; flex-direction: column; gap: 7px; min-width: 0; padding: 16px 18px; border: 1px solid var(--border); border-radius: 14px; background: var(--card); }
	.device-info-card strong { overflow-wrap: anywhere; font-size: 14px; font-weight: 700; }
	.device-info-footer { display: flex; justify-content: space-between; align-items: center; gap: 12px; color: var(--muted-foreground); font-size: 12px; }
	/* Experimental desktop shell inspired by the diagnosis mockup. */
	.app-shell {
		width: 100%;
		padding: 0;
		background: #f5f8fc;
		--panel-border: #d4dfec;
		--panel-surface: #ffffff;
	}
	.app-header {
		min-height: 76px;
		padding: 16px 24px;
		margin-bottom: 0;
		border-bottom: 1px solid #d4dfec;
		background: linear-gradient(105deg, #ffffff, #f2f7ff);
		flex-wrap: wrap;
	}
	.brand-icon { color: #2563eb; display: flex; flex: 0 0 auto; }
	.header-title-row { gap: 14px; flex-wrap: wrap; }
	.header-title-row h1 { font-size: 22px; letter-spacing: -.025em; }
	.header-controls { display: flex; align-items: center; gap: 12px; flex-wrap: wrap; }
	.desktop-layout { grid-template-columns: 248px minmax(0, 1fr); gap: 12px; padding: 12px; }
	.app-sidebar {
		padding: 0;
		border: 0; border-radius: 0;
		background: transparent;
		box-shadow: none;
		overflow-y: auto; overflow-x: hidden; scrollbar-width: none;
	}
	.app-sidebar::-webkit-scrollbar { display: none; }
	.sidebar-device-card {
		margin: 0 0 8px; width: 100%; padding: 10px 12px;
		border: 1px solid #d4dfec; border-radius: 8px; border-bottom: 1px solid #d4dfec;
		background: #fff;
	}
	.sidebar-nav { gap: 6px; padding-top: 8px; overflow: visible; flex-shrink: 0; }
	.sidebar-label { padding-left: 10px; font-size: 9px; font-weight: 700; }
	.sidebar-item {
		display: grid; grid-template-columns: 24px minmax(0, 1fr); column-gap: 12px;
		align-items: center; padding: 8px 10px; border: 1px solid #d4dfec;
		border-radius: 8px; background: #fff; cursor: pointer;
		color: #334155;
		box-shadow: 0 2px 6px rgba(15, 23, 42, .025);
	}
	.nav-icon { grid-column: 1; grid-row: 1 / 3; display: flex; color: #64748b; }
	.nav-title { grid-column: 2; font-size: 14px; font-weight: 600; line-height: 1.5; }
	.nav-subtitle { grid-column: 2; min-width: 0; font-size: 10.5px; line-height: 1.4; overflow-wrap: anywhere; }
	.led-nav-subtitle { white-space: normal; overflow: visible; text-overflow: clip; }
	.led-nav-subtitle > span { display: inline-block; }
	.sidebar-item:disabled { opacity: .55; }
	.sidebar-item:not(:disabled):hover,
	.sidebar-device-card:hover,
	.sidebar-device-card:focus-visible {
		background: #f2f7ff; border-color: #b8c9df; color: #1d4ed8;
		transform: none;
	}
	.sidebar-item.active,
	.sidebar-item.active:not(:disabled):hover {
		background: #eaf2ff;
		border-color: #9bb9ed; color: #1d4ed8; box-shadow: inset 3px 0 0 #3b82f6;
	}
	.sidebar-item.active .nav-title { font-weight: 650; }
	.sidebar-item.active .nav-icon,
	.sidebar-item.active .nav-subtitle,
	.sidebar-item.active .firmware-nav-version { color: #496b9c; }
	.sidebar-item:focus-visible { outline: 2px solid #60a5fa; outline-offset: -2px; }
	.sidebar-reset { margin-top: auto; padding: 16px 0 0; border-top: 1px solid #d4dfec; flex: 0 0 auto; }
	.app-content { padding: 0; }
	:global(.app-content .settings-group),
	:global(.app-content .panel),
	.device-info-card,
	.device-hero-card {
		border: 1px solid var(--panel-border);
		border-radius: 8px;
		background: var(--panel-surface);
		box-shadow: none;
	}
	:global(.app-content .settings-group) { min-width: 0; padding: 14px; margin-bottom: 12px; }
	:global(.app-content .settings-group > :last-child) { margin-bottom: 0; }
	:global(.app-content .settings-group h2),
	:global(.app-content .settings-group h3) { font-size: 16px; font-weight: 650; line-height: 1.4; }
	:global(.app-content .grid > .settings-group) { margin-bottom: 12px; }
	.empty-state { border-style: solid; border-radius: 8px; min-height: 360px; }
	.device-info-card, .device-hero-card { border-radius: 8px; }
	:global(.dark) .app-shell { background: #050b18; --panel-border: #24354f; --panel-surface: #101c30; }
	:global(.dark) .app-header {
		background: linear-gradient(105deg, #0b1527, #050c1a);
		border-color: #24354f;
	}
	:global(.dark) .brand-icon { color: #9bd7ff; }
	:global(.dark) .app-sidebar { background: transparent; }
	:global(.dark) .sidebar-device-card,
	:global(.dark) .sidebar-item { background: #101c30; border-color: #24354f; }
	:global(.dark) .sidebar-item { color: #d3dfef; }
	:global(.dark) .nav-icon { color: #9fb7d4; }
	:global(.dark) .sidebar-item.active,
	:global(.dark) .sidebar-item.active:not(:disabled):hover {
		background: #172b49;
		border-color: #3b6598; color: #bfdbfe; box-shadow: inset 3px 0 0 #60a5fa;
	}
	:global(.dark) .sidebar-item.active .nav-icon,
	:global(.dark) .sidebar-item.active .nav-subtitle,
	:global(.dark) .sidebar-item.active .firmware-nav-version { color: #bfdbfe; }
	:global(.dark) .sidebar-item:not(:disabled):hover,
	:global(.dark) .sidebar-device-card:hover,
	:global(.dark) .sidebar-device-card:focus-visible { background: #13243d; border-color: #365273; color: #bfdbfe; }
	:global(.dark) .sidebar-reset { border-color: #24354f; }
	:global(.dark) .sidebar-reset-button .nav-subtitle { color: #94a3b8; }
	:global(.dark) .empty-state { background: #101a2d; border-color: #283b56; }
	@media (max-height: 760px) and (min-width: 681px) {
		.app-header { min-height: 64px; padding: 10px 18px; }
		.app-sidebar { padding: 0; }
		.sidebar-device-card { padding: 6px 12px; margin-bottom: 4px; }
		.sidebar-item { padding: 6px 9px; }
		.sidebar-nav { gap: 4px; }
		.nav-title { font-size: 13px; line-height: 1.3; }
		.nav-subtitle { margin-top: 2px; font-size: 10.5px; line-height: 1.35; }
		.sidebar-label-spaced { margin-top: 4px; }
		.sidebar-reset { padding-top: 8px; }
	}
	@media (max-height: 620px) and (min-width: 681px) {
		.sidebar-label { display: none; }
		.sidebar-nav { gap: 3px; }
		.sidebar-item { padding: 4px 8px; }
		.nav-title { font-size: 12px; line-height: 1.2; }
		.nav-subtitle { font-size: 10px; line-height: 1.3; }
		.sidebar-device-card { margin-bottom: 2px; }
	}
	@media (min-width: 681px) and (max-width: 820px) {
		.desktop-layout { grid-template-columns: 220px minmax(0, 1fr); gap: 8px; padding: 8px; }
		.sidebar-item { column-gap: 8px; }
	}
	@media (max-width: 680px) {
		.app-shell {
			height: auto;
			max-height: none;
			padding: 12px;
			overflow: visible;
		}
		.app-header {
			align-items: flex-start;
			flex-direction: column;
		}
		.header-title-row {
			flex-wrap: wrap;
		}
		.desktop-layout {
			grid-template-columns: 1fr;
			flex: none;
			overflow: visible;
			padding: 0;
		}
		.app-sidebar {
			position: static;
			height: auto;
			overflow: visible;
		}
		.sidebar-nav {
			display: flex;
			flex-direction: column;
		}
		.sidebar-reset {
			margin-top: 16px;
			padding-top: 0;
		}
		.app-content {
			overflow: visible;
			padding: 0;
		}
		.device-info-grid { grid-template-columns: 1fr; }
		.device-info-heading { align-items: flex-start; }
		.device-info-footer { align-items: flex-start; flex-direction: column; }
	}
</style>

