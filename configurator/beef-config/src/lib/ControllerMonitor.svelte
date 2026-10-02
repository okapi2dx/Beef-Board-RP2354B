<script lang="ts">
	import { readDiagnostics, parseDiagnostics, ReportId, type Diagnostics } from '$lib/types/hid';
	import { appState } from '$lib/types/state.svelte';
	import { isRp2354bBoard } from '$lib/types/board';
	import type { Config } from '$lib/types/config.svelte';
	import { TurntableMode, BarMode, type Hsv } from '$lib/types/types.svelte';
	import { tr } from '$lib/types/locale.svelte';
	let { config, active }: { config: Config; active: boolean } = $props();
	let data = $state<Diagnostics | null>(null);
	let side = $state('1P'),
		stale = $state(true);
	import { createLedEmulator } from '$lib/led-emulator.js';
	let frame = $state({
		ring: Array(24).fill('#000000'),
		bar: Array(16).fill('#000000'),
		buttonLevels: Array(11).fill(0)
	});
	const labels = ['B1', 'B2', 'B3', 'B4', 'B5', 'B6', 'B7', 'E1', 'E2', 'E3', 'E4'];
	const destinationLabels = [
		...[0, 1, 2, 3, 4, 5, 6].map((bit) => ({ label: `B${bit + 1}`, bit })),
		{ label: 'B8', bit: 13 },
		...[7, 8, 9, 10].map((bit, index) => ({ label: `B${index + 9}`, bit }))
	];
	const legacyMonitorPollMs = 75;
	const positions = [
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
	function logicalButtonState(buttons: number): number {
		if (config.version < 28 || config.iidx_input_mode !== 'Joystick') return buttons;
		let mapped = buttons & ~0x7ff;
		for (let physical = 0; physical < config.button_mapping.length; physical++) {
			if (buttons & (1 << physical)) {
				const destination = config.button_mapping[physical];
				mapped |= destination === 11 ? 1 << 13 : 1 << destination;
			}
		}
		return mapped;
	}
	const displayedButtons = $derived(data ? logicalButtonState(data.buttons) : 0);
	const pressed = $derived(destinationLabels.filter(({ bit }) => displayedButtons & (1 << bit)).map(({ label }) => label));
	const effectLabels: Record<string, string> = {
		Static: '固定',
		Spin: '回転',
		Shift: 'シフト',
		'Rainbow Static': 'レインボー（固定）',
		'Rainbow Reactive': 'レインボー（反応）',
		'Rainbow Spin': 'レインボー（回転）',
		Reactive: '反応',
		Breathing: '呼吸',
		HID: 'HID連動',
		Off: 'オフ',
		'Key Spectrum (P1)': 'キー・スペクトラム（P1）',
		'Key Spectrum (P2)': 'キー・スペクトラム（P2）',
		'Tape LED (P1)': 'テープLED（P1）',
		'Tape LED (P2)': 'テープLED（P2）'
	};
	function effectLabel(value: string): string {
		return tr(effectLabels[value] ?? value, value);
	}
	function axisLabel(value: number | undefined): string {
		return value === undefined ? '---' : String(value).padStart(3, '0');
	}
	function barRgbAt(i: number) {
		return frame.bar[side === '1P' ? 15 - i : i];
	}
	function barGradient() {
		return (
			'linear-gradient(to bottom,' +
			Array.from({ length: 16 }, (_, i) => barRgbAt(i) + ' ' + (i / 15) * 100 + '%').join(',') +
			')'
		);
	}
	function ringBackground() {
		return (
			'conic-gradient(' +
			[...frame.ring, frame.ring[0]].map((c, i) => c + ' ' + (i / 24) * 100 + '%').join(',') +
			')'
		);
	}
	$effect(() => {
		const device = appState.device;
		if (!device) return;

		// v30+ receives diagnostics through the config interface's interrupt IN
		// endpoint. This does not block the controller's main loop, unlike
		// repeated Feature Report reads over endpoint 0. Keep the listener
		// registered while this component exists; rendering still pauses when
		// the Monitor tab is inactive.
		if (config.version >= 30) {
			data = null;
			stale = true;
			const onInput = (event: HIDInputReportEvent) => {
				if (event.reportId !== ReportId.DiagnosticsStream) return;
				try {
					const next = parseDiagnostics(event.data);
					data = next;
					stale = false;
				} catch {
					stale = true;
				}
			};
			device.addEventListener('inputreport', onInput);
			return () => device.removeEventListener('inputreport', onInput);
		}

		if (!active || config.version < 24) return;

		// Compatibility path for older firmware. Keep this deliberately slow:
		// Feature Report reads are control transfers and can delay input handling.
		let disposed = false,
			timer: ReturnType<typeof setTimeout>,
			failures = 0;
		data = null;
		stale = true;
		async function poll() {
			if (disposed || !device || appState.device !== device) return;
			try {
				const next = await readDiagnostics();
				if (disposed || appState.device !== device) return;
				data = next;
				failures = 0;
				stale = false;
			} catch {
				if (++failures >= 10) {
					stale = true;
					data = null;
				}
			}
			// Keep input feedback close to the display refresh rate. The read is
			// awaited before scheduling the next one, so requests never overlap.
			if (!disposed) timer = setTimeout(poll, legacyMonitorPollMs);
		}
		void poll();
		return () => {
			disposed = true;
			clearTimeout(timer);
		};
	});
	$effect(() => {
		// Reset the local phase on connection changes. Keep advancing while another
		// settings tab is open; only publishing the rendered frame is paused.
		const device = appState.device;
		if (!device) return;
		const emulate = createLedEmulator();
		let last = performance.now(),
			handle: number;
		function draw(now: number) {
			const next = emulate(config, data, now - last, isRp2354bBoard(appState.boardInfo), displayedButtons);
			if (active) frame = next;
			last = now;
			handle = requestAnimationFrame(draw);
		}
		handle = requestAnimationFrame(draw);
		return () => cancelAnimationFrame(handle);
	});
</script>

<section aria-label={tr('コントローラーモニター', 'Controller Monitor')}>
	<div class="monitor-heading">
		<strong>{tr('コントローラーモニター', 'Controller Monitor')}</strong><span
			class="text-muted-foreground text-sm"
			>{config.version < 24
				? tr('対応ファームウェアへ更新してください', 'Update firmware to enable monitoring')
				: stale
					? tr('入力待機中', 'Waiting for input')
					: tr('入力取得中', 'Monitoring inputs')}</span
		>
	</div>
	<div class="monitor-layout">
		<div class="monitor-panels">
			<div class="panel panel-primary">
				<strong>{tr('入力状態', 'Input state')}</strong>
				<div class="status-list">
					<div class="status-row">
						<span>{tr('X軸', 'X Axis')}</span><b data-testid="monitor-axis-value"
							>{axisLabel(data?.output)}/255</b
						>
					</div>
					<div class="status-row pressed-row">
						<span>{tr('割当先', 'Destination')}</span>
						<b class="pressed-value">
							{#if pressed.length}
								{#if pressed.length > 7}
									<span class="pressed-line">{#each pressed.slice(0, 7) as label}<span>{label}</span>{/each}</span>
									<span class="pressed-line">{#each pressed.slice(7) as label}<span>{label}</span>{/each}</span>
								{:else}
									<span class="pressed-line">{#each pressed as label}<span>{label}</span>{/each}</span>
								{/if}
							{:else}
								—
							{/if}
						</b>
					</div>
				</div>
			</div>
			<div class="panel panel-primary">
				<strong>{tr('LEDプレビュー', 'LED Preview')}</strong>
				<div class="status-list">
					<div class="status-row">
						<span>{tr('ターンテーブル', 'Turntable')}</span><b data-testid="monitor-tt-effect"
							>{effectLabel(config.tt_effect)}</b
						>
					</div>
					<div class="status-row">
						<span>{tr('センターバー', 'Light bar')}</span><b data-testid="monitor-bar-effect"
							>{effectLabel(config.link_bar_effect ? config.tt_effect : config.bar_effect)}</b
						>
					</div>
				</div>
			</div>
			<div class="panel panel-layout">
				<strong>{tr('表示する配置', 'Layout')}</strong>
				<div class="side-buttons">
					{#each ['1P', '2P'] as s}<button
							class:selected={side === s}
							aria-pressed={side === s}
							onclick={() => (side = s)}>{s}</button
						>{/each}
				</div>
			</div>
		</div>
		<div class="stage">
			<div class="stage-title" data-testid="monitor-side">{side}</div>
			<div class="device" class:second={side === '2P'}>
				<div class="disc-area">
					<div
						class="ring"
						data-testid="monitor-ring"
						data-led-source="simulation"
						style:background={ringBackground()}
					>
						<div class="disc">
							<span style:transform={`rotate(${((data?.output ?? 0) * 360) / 256}deg)`}>●</span>
						</div>
					</div>
				</div>
				<div
					class="bar"
					aria-label={tr(
						'センターバー：LEDの発光状態をグラデーション表示',
						'Center bar: LED lighting shown as a gradient'
					)}
					data-top-rgb={barRgbAt(0)}
					data-bottom-rgb={barRgbAt(15)}
					style:background={barGradient()}
				></div>
				<div class="keyboard">
					{#each labels as label, i}<div
							class="key"
							class:black={i < 7 && i % 2 === 1}
							style:left={`${(positions[i][0] / 310) * 100}%`}
							style:top={`${(positions[i][1] / 260) * 100}%`}
							role="img"
							aria-label={`${label}: ${((data?.buttons ?? 0) & (1 << i)) ? tr('押下', 'Pressed') : tr('離している', 'Released')}`}
						>
							<div
								class="key-light"
								style:opacity={frame.buttonLevels[i] / 100}
								style:background={'#ef4444'}
							></div>
							<span>{label}</span>
						</div>{/each}
				</div>
			</div>
		</div>
	</div>
</section>

<style>
	.bar {
		background: #263244;
		outline: 2px solid #000;
		border-radius: 6px !important;
		width: 3% !important;
		align-self: stretch;
		min-height: 0 !important;
		box-shadow: 0 0 8px color-mix(in srgb, currentColor 20%, transparent);
	}
	.monitor-heading {
		display: flex;
		justify-content: space-between;
		gap: 12px;
		margin-bottom: 10px;
	}
	.monitor-layout {
		display: flex;
		flex-direction: column;
		gap: 10px;
	}
	.monitor-panels {
		display: grid;
		grid-template-columns: minmax(0, 1.15fr) minmax(0, 1.15fr) minmax(150px, 0.7fr);
		gap: 12px;
	}
	.stage {
		background: #0f172a;
		border-radius: 14px;
		padding: 12px 18px 18px;
		color: #e2e8f0;
		min-width: 0;
	}
	.stage-title {
		font-size: 16px;
		font-weight: 700;
		margin-bottom: 6px;
	}
	.device {
		display: flex;
		align-items: stretch;
		gap: 1.35%;
		padding: 1.35%;
		width: min(100%, 1120px);
		margin: 0 auto;
		aspect-ratio: 1.88;
		background: linear-gradient(145deg, #eef2f6, #aab5c1);
		border-radius: 18px;
		border: 3px double #718094;
		box-shadow: inset 0 0 0 1px #f8fafc;
	}
	.device.second {
		flex-direction: row-reverse;
	}
	.disc-area {
		flex: 1 1 0;
		min-width: 0;
		display: grid;
		place-items: center;
		padding: 1.5%;
		background: linear-gradient(145deg, #e8edf2, #c4ced8);
		border: 1px solid #718094;
		border-radius: 13px;
		overflow: hidden;
	}
	.ring {
		width: min(100%, 500px);
		max-height: 100%;
		padding: 7px;
		border-radius: 50%;
		aspect-ratio: 1;
		box-shadow: 0 0 0 2px #101827, 0 0 0 5px #7b8797;
	}
	.disc {
		height: 100%;
		border-radius: 50%;
		background: radial-gradient(circle, #101827 35%, #2b3544 36%, #2b3544 52%, #0c1320 53%);
		display: grid;
		place-items: center;
	}
	.disc span {
		display: block;
		width: 100%;
		height: 100%;
		text-align: center;
		padding-top: 15%;
		color: #67e8f9;
		font-size: 14px;
	}
	.bar {
		flex: 0 0 3.2%;
		width: auto !important;
		box-sizing: border-box;
		height: 100%;
		min-height: 180px;
		border: 1px solid #718094;
		border-radius: 10px !important;
		box-shadow: inset 0 0 0 3px #d7dee6, 0 0 4px rgba(15, 23, 42, 0.3);
	}
	.keyboard {
		position: relative;
		flex: 1 1 0;
		min-width: 0;
		aspect-ratio: auto;
		padding: 1.4%;
		background: linear-gradient(145deg, #e8edf2, #c4ced8);
		border: 1px solid #718094;
		border-radius: 12px;
	}
	.key {
		position: absolute;
		width: 15%;
		height: 22%;
		border: 2px solid #7b8797;
		border-radius: 6px;
		background: #edf1f6;
		color: #172033;
		display: grid;
		place-items: center;
		overflow: hidden;
		box-shadow: inset 0 -4px #bac4d0;
	}
	.key:nth-child(n + 8) {
		height: 16%;
	}
	.key.black {
		background: #202b3b;
		color: #fff;
	}
	.key-light {
		position: absolute;
		inset: 0;
		background: #38bdf8;
	}
	.key span {
		position: relative;
		font-size: 11px;
		font-weight: 700;
	}
	.panel {
		border: 1px solid var(--border);
		border-radius: 10px;
		padding: 13px 14px;
		font-size: 12px;
		background: color-mix(in srgb, var(--card) 96%, var(--muted));
	}
	.panel-primary {
		min-height: 118px;
	}
	.panel-layout {
		min-width: 0;
	}
	.panel strong {
		font-size: 14px;
	}
	.status-list {
		margin-top: 8px;
	}
	.status-row {
		display: flex;
		justify-content: space-between;
		align-items: flex-start;
		gap: 10px;
		padding: 8px 0;
	}
	.status-row + .status-row {
		border-top: 1px solid var(--border);
	}
	.status-row span {
		color: var(--muted-foreground);
		flex-shrink: 0;
	}
	.status-row b {
		text-align: right;
		overflow-wrap: anywhere;
		font-variant-numeric: tabular-nums;
	}
	.pressed-row {
		gap: 4px;
		min-height: 46px;
	}
	.pressed-value {
		display: flex;
		flex: 1 1 0;
		flex-direction: column;
		align-content: flex-start;
		min-width: 0;
		text-align: right;
		font-size: 11px;
		line-height: 1.45;
		min-height: 2.9em;
	}
	.pressed-value span {
		color: inherit;
		white-space: nowrap;
	}
	.pressed-line {
		display: flex;
		flex-wrap: wrap;
		justify-content: flex-end;
		column-gap: 3px;
	}
	.panel button {
		border: 1px solid var(--border);
		border-radius: 6px;
		padding: 8px;
		margin-top: 10px;
		cursor: pointer;
	}
	.side-buttons {
		display: flex;
		gap: 8px;
	}
	.side-buttons button {
		flex: 1;
	}
	.selected {
		background: var(--primary);
		color: var(--primary-foreground);
	}
	@media (max-width: 899px) {
		.monitor-panels {
			display: grid;
			grid-template-columns: repeat(2, minmax(0, 1fr));
			gap: 8px;
		}
		.panel-layout {
			grid-column: 1 / -1;
		}
		.stage {
			padding: 12px;
		}
		.bar {
			min-height: 150px;
		}
	}
	@media (max-width: 560px) {
		.monitor-panels {
			grid-template-columns: 1fr;
		}
		.panel-layout {
			grid-column: auto;
		}
	}
</style>
