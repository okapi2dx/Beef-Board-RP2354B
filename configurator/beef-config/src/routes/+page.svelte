<script lang="ts">
	import { onMount } from 'svelte';

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
	import { Command, readFirmwareInfo, sendCommand, waitForReconnection, type FirmwareInfo } from '$lib/types/hid';
	import { isRp2354bBoard, RP2354B_BEEF_PRODUCT_ID, RP2354B_BEEF_VENDOR_ID } from '$lib/types/board';

	import { connectDevice, reconnectDevice, reconnectDfuDevice, appState } from '$lib/types/state.svelte';

	const Tab = {
		Config: 'config',
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
	<title>Beef Board Configurator{appVersion ? ` ${formatDisplayVersion(appVersion)}` : ''}</title>
	<div class="app-header">
		<div class="header-title-row">
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
		<div class="flex items-center gap-3">
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
				<div class="sidebar-device-card">
					<div class="device-status-row">
						<span class:connected={!!appState.device} class="status-dot"></span>
						<div>
							<div class="status-title">{appState.device ? tr('接続済み', 'Connected') : tr('未接続', 'Disconnected')}</div>
							<div class="status-subtitle">{connectedDeviceLabel(appState.device)}</div>
						</div>
					</div>
				</div>

				<nav class="sidebar-nav" aria-label={tr('メインメニュー', 'Main menu')}>
					<div class="sidebar-label">{tr('コントローラー設定', 'CONTROLLER')}</div>
					<button class:active={activeTab === Tab.Config && settingsSection === 'input'} class="sidebar-item" disabled={!appState.device || appState.disableConfigTab} onclick={() => { activeTab = Tab.Config; settingsSection = 'input'; }}>
						<span class="nav-title">{tr('入力設定', 'Input')}</span>
						<span class="nav-subtitle">{tr('入力モード・ターンテーブル', 'Mode & turntable')}</span>
					</button>
					<button class:active={activeTab === Tab.Config && settingsSection === 'led'} class="sidebar-item" disabled={!appState.device || appState.disableConfigTab} onclick={() => { activeTab = Tab.Config; settingsSection = 'led'; }}>
						<span class="nav-title">{tr('LED設定', 'LEDs')}</span>
						<span class="nav-subtitle led-nav-subtitle">{tr('ボタン・ターンテーブル・センターバー', 'Buttons, turntable & bar')}</span>
					</button>
					<button class:active={activeTab === Tab.Config && settingsSection === 'keys'} class="sidebar-item" disabled={!appState.device || appState.disableConfigTab} onclick={() => { activeTab = Tab.Config; settingsSection = 'keys'; }}>
						<span class="nav-title">{tr('キー割り当て', 'Key Bindings')}</span>
						<span class="nav-subtitle">{tr('キー・ボタン配置', 'Keys & button layout')}</span>
					</button>
					<button class:active={activeTab === Tab.Config && settingsSection === 'monitor'} class="sidebar-item" disabled={!appState.device || appState.disableConfigTab} onclick={() => { activeTab = Tab.Config; settingsSection = 'monitor'; }}>
						<span class="nav-title">{tr('コントローラーモニター', 'Controller Monitor')}</span>
						<span class="nav-subtitle">{tr('入力状態をリアルタイム表示', 'Live input status')}</span>
					</button>

					<div class="sidebar-label sidebar-label-spaced">{tr('メンテナンス', 'MAINTENANCE')}</div>
					<button class:active={activeTab === Tab.Firmware} class="sidebar-item" onclick={() => { activeTab = Tab.Firmware; }}>
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
									class="sidebar-item sidebar-reset-button reset-action-button"
									disabled={!appState.device || appState.disableConfigTab}
								>
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
									onclick={async () => {
										await sendCommand(Command.ResetConfig);
										await waitForReconnection();
										resetDialogOpen = false;
									}}
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
				{#if activeTab === Tab.Config}
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
		padding: 6px 6px 14px;
		border-bottom: 1px solid var(--border);
	}
	.device-status-row {
		display: flex;
		align-items: center;
		gap: 10px;
		margin-bottom: 12px;
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
		border-color: var(--border);
		background: var(--muted);
		box-shadow: inset 3px 0 0 currentColor;
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
		padding-right: 4px;
	}
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
	@media (max-width: 820px) {
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
		}
		.app-sidebar {
			position: static;
			height: auto;
			overflow: visible;
		}
		.sidebar-nav {
			display: grid;
			grid-template-columns: repeat(2, minmax(0, 1fr));
		}
		.sidebar-label {
			grid-column: 1 / -1;
		}
		.sidebar-reset {
			margin-top: 16px;
			padding-top: 0;
		}
		.app-content {
			overflow: visible;
			padding-right: 0;
		}
	}
</style>

