<script lang="ts">
	import { tick } from 'svelte';
	import { parseChatterCounts, parseDiagnostics, ReportId, type ChatterCounts, type Diagnostics } from '$lib/types/hid';
	import type { Config } from '$lib/types/config.svelte';
	import { appState } from '$lib/types/state.svelte';
	import { tr } from '$lib/types/locale.svelte';
	import SliderInput from '$lib/SliderInput.svelte';
	import HelpText from '$lib/HelpText.svelte';
	import { Switch as UiSwitch } from '$lib/components/ui/switch';
	import { diagnosticEventSlots } from '$lib/types/chatter-events';
	import { WaveClock } from '$lib/types/wave-history';

	let { config, active }: { config: Config; active: boolean } = $props();
	let outputButtons = $state(0);
	let signalSamples = $state<{ timestamp: number; buttons: number }[]>([{ timestamp: -1, buttons: 0 }]);
	let waveMarkers = $state<{ id: number; timestamp: number; buttonIndex: number; phase: 'press' | 'release' }[]>([]);
	let buttonEdges = $state<number[]>(Array(7).fill(0));
	let chatterCounts = $state<number[]>(Array(7).fill(0));
	let edgeEvents = $state<{ id: number; timestamp: number; waveTimestamp: number; button: string; buttonIndex: number; pressed: boolean; phase: 'press' | 'release'; interval: number; threshold: number; chatter: boolean; logVisible: boolean }[]>([]);
	let edgeMode = $state<'press' | 'release'>('press');
	let logFilter = $state<'all' | 'press' | 'release'>('all');
	let autoSetting = $state(false);
	let bulkSetting = $state(false);
	let waveformNow = $state(0);
	let viewingLive = $state(true);
	let fixedViewEnd = $state(0);
	let selectedLogId = $state<number>();
	let waveClock = new WaveClock(performance.now());
	let nextLogId = 0;
	let resetDiagnostics: (() => void) | undefined;
	let logViewport = $state<HTMLDivElement>();
	let logScrollTop = $state(0);
	let logScrollExtent = $state(0);
	let logViewportHeight = $state(190);
	let logDragging = false;
	let logPointerOffset = 0;
	let lastLogInteractionAt = 0;
	let logJumpQueued = false;
	const labels = ['B1', 'B2', 'B3', 'B4', 'B5', 'B6', 'B7'];
	const waveWindowMs = 1000;
	const maxWaveSamples = 20000;
	const maxLogEntries = 1000;
	const logRowHeight = 19;
	const logOverscan = 5;
	const totalEdges = $derived(buttonEdges.reduce((sum, count) => sum + count, 0));
	const detectedCount = $derived(chatterCounts.reduce((sum, count) => sum + count, 0));
	const detectionRate = $derived(totalEdges ? detectedCount / totalEdges * 100 : 0);
	const percent = (detected: number, edges: number) => (edges ? detected / edges * 100 : 0).toFixed(1);
	const visibleLog = $derived(edgeEvents.filter((entry) => entry.logVisible && (logFilter === 'all' || entry.phase === logFilter)));
	const oldestWaveTime = $derived(edgeEvents.at(-1)?.waveTimestamp);
	const earliestViewEnd = $derived(oldestWaveTime === undefined ? waveformNow : Math.max(0, oldestWaveTime + waveWindowMs / 2));
	const maxBackMs = $derived(Math.max(0, waveformNow - earliestViewEnd));
	const viewEnd = $derived(viewingLive ? waveformNow : Math.max(earliestViewEnd, fixedViewEnd));
	const viewBackMs = $derived(Math.min(maxBackMs, Math.max(0, waveformNow - viewEnd)));
	const firstRenderedLog = $derived(Math.max(0, Math.min(visibleLog.length, Math.floor(logScrollTop / logRowHeight) - logOverscan)));
	const lastRenderedLog = $derived(Math.min(visibleLog.length, Math.ceil((logScrollTop + logViewportHeight) / logRowHeight) + logOverscan));
	const renderedLog = $derived(visibleLog.slice(firstRenderedLog, lastRenderedLog));
	const selectedLog = $derived(edgeEvents.find((entry) => entry.id === selectedLogId));
	const logThumbHeight = $derived(Math.min(logViewportHeight, Math.max(24, logViewportHeight * logViewportHeight / (logScrollExtent + logViewportHeight))));
	const logThumbTop = $derived(logScrollExtent ? logScrollTop / logScrollExtent * (logViewportHeight - logThumbHeight) : 0);

	function updateLogScroll(): void {
		if (!logViewport) return;
		logViewportHeight = logViewport.clientHeight;
		logScrollExtent = Math.max(0, logViewport.scrollHeight - logViewportHeight);
		logScrollTop = logViewport.scrollTop;
	}

	function focusLog(entry: (typeof edgeEvents)[number]): void {
		selectedLogId = entry.id;
		viewingLive = false;
		fixedViewEnd = entry.waveTimestamp + waveWindowMs / 2;
	}

	function onLogScroll(): void {
		updateLogScroll();
		if (performance.now() - lastLogInteractionAt > 500) return;
		const centerIndex = Math.max(0, Math.min(visibleLog.length - 1,
			Math.floor((logScrollTop + logViewportHeight / 2) / logRowHeight)));
		const entry = visibleLog[centerIndex];
		if (entry) focusLog(entry);
	}

	function markLogInteraction(): void {
		lastLogInteractionAt = performance.now();
	}

	function returnLogToNewest(): void {
		// A programmatic jump must not select a historical row in onLogScroll.
		lastLogInteractionAt = 0;
		if (logViewport) logViewport.scrollTop = 0;
		logScrollTop = 0;
		if (logJumpQueued) return;
		logJumpQueued = true;
		void tick().then(() => {
			logJumpQueued = false;
			if (logViewport) logViewport.scrollTop = 0;
			logScrollTop = 0;
			updateLogScroll();
		});
	}

	$effect(() => {
		if (!logViewport) return;
		const viewport = logViewport;
		const observer = new ResizeObserver(updateLogScroll);
		const onPointerMove = (event: PointerEvent) => { if (event.buttons) markLogInteraction(); };
		observer.observe(viewport);
		viewport.addEventListener('wheel', markLogInteraction);
		viewport.addEventListener('pointerdown', markLogInteraction);
		viewport.addEventListener('pointermove', onPointerMove);
		viewport.addEventListener('keydown', markLogInteraction);
		updateLogScroll();
		return () => {
			observer.disconnect();
			viewport.removeEventListener('wheel', markLogInteraction);
			viewport.removeEventListener('pointerdown', markLogInteraction);
			viewport.removeEventListener('pointermove', onPointerMove);
			viewport.removeEventListener('keydown', markLogInteraction);
		};
	});

	$effect(() => {
		visibleLog.length;
		void tick().then(updateLogScroll);
	});

	function scrollLogFromPointer(event: PointerEvent): void {
		if (!logViewport || !logScrollExtent) return;
		markLogInteraction();
		const rail = event.currentTarget as HTMLButtonElement;
		const bounds = rail.getBoundingClientRect();
		const available = bounds.height - logThumbHeight;
		if (available <= 0) return;
		const target = Math.max(0, Math.min(available, event.clientY - bounds.top - logPointerOffset));
		logViewport.scrollTop = target / available * logScrollExtent;
		onLogScroll();
	}

	function startLogDrag(event: PointerEvent): void {
		if (!logScrollExtent) return;
		const rail = event.currentTarget as HTMLButtonElement;
		const thumbTop = rail.getBoundingClientRect().top + logThumbTop;
		logPointerOffset = event.clientY >= thumbTop && event.clientY <= thumbTop + logThumbHeight
			? event.clientY - thumbTop : logThumbHeight / 2;
		logDragging = true;
		rail.setPointerCapture(event.pointerId);
		scrollLogFromPointer(event);
	}

	function moveLogDrag(event: PointerEvent): void {
		if (logDragging) scrollLogFromPointer(event);
	}

	function stopLogDrag(): void {
		logDragging = false;
	}

	function scrollLogFromKeyboard(event: KeyboardEvent): void {
		if (!logViewport) return;
		markLogInteraction();
		const amount = event.key === 'ArrowDown' ? 32 : event.key === 'ArrowUp' ? -32 : event.key === 'PageDown' ? logViewportHeight : event.key === 'PageUp' ? -logViewportHeight : undefined;
		if (amount !== undefined) logViewport.scrollTop += amount;
		else if (event.key === 'Home') logViewport.scrollTop = 0;
		else if (event.key === 'End') logViewport.scrollTop = logScrollExtent;
		else return;
		event.preventDefault();
		onLogScroll();
	}

	function moveWaveHistory(event: Event): void {
		const back = Number((event.currentTarget as HTMLInputElement).value);
		selectedLogId = undefined;
		viewingLive = back === 0;
		if (!viewingLive) fixedViewEnd = waveformNow - back;
	}

	function setEdgeMode(mode: 'press' | 'release'): void {
		edgeMode = mode;
	}

	function resetWaveHistory(initialButtons = 0): void {
		const now = performance.now();
		waveClock = new WaveClock(now);
		waveClock.record(now, initialButtons, false);
		waveformNow = 0;
		viewingLive = true;
		fixedViewEnd = 0;
		selectedLogId = undefined;
		signalSamples = [{ timestamp: -1, buttons: initialButtons }];
		waveMarkers = [];
	}

	function trimWaveHistory(): void {
		const oldest = edgeEvents.at(-1);
		if (!oldest) return;
		const cutoff = oldest.waveTimestamp - waveWindowMs / 2;
		const firstRecent = signalSamples.findIndex((sample) => sample.timestamp >= cutoff);
		if (firstRecent > 1) signalSamples = signalSamples.slice(firstRecent - 1);
		if (signalSamples.length > maxWaveSamples) signalSamples = signalSamples.slice(-maxWaveSamples);
		if (waveMarkers.length) waveMarkers = waveMarkers.filter((marker) => marker.id >= oldest.id);
	}

	const waveforms = $derived.by(() => labels.map((label, buttonIndex) => {
		const y = 34 + buttonIndex * 41;
		const windowStart = viewEnd - waveWindowMs;
		const samples = signalSamples.filter((sample) => sample.timestamp >= windowStart && sample.timestamp <= viewEnd);
		const previousSample = signalSamples.findLast((sample) => sample.timestamp < windowStart);
		let state = Boolean((previousSample?.buttons ?? 0) & (1 << buttonIndex));
		const high = y - 10;
		const low = y + 10;
		let startX = 48;
		const segments: { x: number; width: number; pressed: boolean }[] = [];
		const transitions: number[] = [];
		const points: { x: number; phase: 'press' | 'release'; id: number }[] = [];
		for (const sample of samples) {
			const nextState = Boolean(sample.buttons & (1 << buttonIndex));
			if (nextState === state) continue;
			const x = Math.max(48, Math.min(694, 48 + (sample.timestamp - windowStart) / waveWindowMs * 646));
			segments.push({ x: startX, width: Math.max(0, x - startX), pressed: state });
			transitions.push(x);
			startX = x;
			state = nextState;
		}
		segments.push({ x: startX, width: Math.max(0, 694 - startX), pressed: Boolean(state) });
		for (const entry of waveMarkers) {
			if (entry.buttonIndex !== buttonIndex || entry.timestamp < windowStart || entry.timestamp > viewEnd) continue;
			const threshold = entry.phase === 'press' ? config.iidx_button_debounce[buttonIndex] : config.sdvx_keys.button_release_debounce[buttonIndex];
			if (threshold <= 0) continue;
			const x = Math.max(48, Math.min(694, 48 + (entry.timestamp - windowStart) / waveWindowMs * 646));
			points.push({ x, phase: entry.phase, id: entry.id });
		}
		const selectedX = selectedLog?.buttonIndex === buttonIndex && selectedLog.waveTimestamp >= windowStart && selectedLog.waveTimestamp <= viewEnd
			? Math.max(48, Math.min(694, 48 + (selectedLog.waveTimestamp - windowStart) / waveWindowMs * 646)) : undefined;
		return { label, buttonIndex, y, high, low, segments, transitions, points, selectedX, active: Boolean(outputButtons & (1 << buttonIndex)) };
	}));

	$effect(() => {
		if (!active) return;
		let frame = 0;
		let running = true;
		const animate = () => {
			if (!running) return;
			waveformNow = waveClock.advance(performance.now());
			frame = window.requestAnimationFrame(animate);
		};
		frame = window.requestAnimationFrame(animate);
		return () => { running = false; window.cancelAnimationFrame(frame); };
	});

	$effect(() => {
		const device = appState.device;
		if (!device) {
			outputButtons = 0;
			resetWaveHistory();
			edgeEvents = [];
			return;
		}
		if (!active) return;
		let previousSequence: number | undefined;
		let lastCounters: ChatterCounts | undefined;
		const lastEdgeAt = Array(7).fill(0) as number[];
		const lastPhase = Array<'press' | 'release' | undefined>(7).fill(undefined);
		const phaseElapsed = Array(7).fill(0) as number[];
		const hideRecovery = Array(7).fill(false) as boolean[];
		edgeEvents = [];
		resetWaveHistory();
		if (config.version < 41) return;
		const resetStream = () => {
			lastEdgeAt.fill(0);
			lastPhase.fill(undefined);
			phaseElapsed.fill(0);
			hideRecovery.fill(false);
		};
		resetDiagnostics = resetStream;

		const onInput = (event: HIDInputReportEvent) => {
			if (event.reportId === ReportId.ChatterCounts) {
				let counts: ChatterCounts;
				try { counts = parseChatterCounts(event.data); } catch { return; }
				if (lastCounters) {
					buttonEdges = buttonEdges.map((n, i) => n + ((counts.edges[i] - lastCounters!.edges[i] + 65536) % 65536));
					chatterCounts = chatterCounts.map((n, i) => n + ((counts.detected[i] - lastCounters!.detected[i] + 65536) % 65536));
				}
				lastCounters = counts;
				return;
			}
			if (event.reportId !== ReportId.DiagnosticsStream) return;
			let data: Diagnostics;
			try { data = parseDiagnostics(event.data); } catch { return; }
			if (data.chatterEdges.length !== 7 || data.edgeEvents.length !== 18) return;
			const sampleTime = performance.now();
			const nextButtons = data.buttons & 0x7f;
			if (nextButtons & ~outputButtons) { viewingLive = true; selectedLogId = undefined; lastLogInteractionAt = 0; }
			const hasEdge = previousSequence !== undefined && data.edgeSequence !== previousSequence;
			waveformNow = waveClock.record(sampleTime, nextButtons, hasEdge || nextButtons !== outputButtons);
			outputButtons = nextButtons;
			if (signalSamples.at(-1)?.buttons !== outputButtons)
				signalSamples = [...signalSamples, { timestamp: waveformNow, buttons: outputButtons }];
			if (previousSequence === undefined) { previousSequence = data.edgeSequence; return; }
			const { slots, lost } = diagnosticEventSlots(previousSequence, data.edgeSequence);
			const unseen = slots.length;
			if (!unseen) return;
			if (lost) resetStream();
			const now = sampleTime;
			const events: typeof edgeEvents = [];
			const markers: typeof waveMarkers = [];
			const eventAt = Array(unseen).fill(now) as number[];
			const laterAt = Array<number | undefined>(7).fill(undefined);
			const laterInterval = Array(7).fill(0) as number[];
			for (let n = unseen - 1; n >= 0; n--) {
				const slot = slots[n];
				const buttonIndex = data.edgeEvents[slot] & 0x0f;
				if (buttonIndex >= 7) continue;
				const interval = data.edgeEvents[slot + 1] | (data.edgeEvents[slot + 2] << 8);
				const later = laterAt[buttonIndex];
				eventAt[n] = later === undefined || laterInterval[buttonIndex] >= 10000 ? now : later - laterInterval[buttonIndex] / 10;
				laterAt[buttonIndex] = eventAt[n];
				laterInterval[buttonIndex] = interval;
			}
			for (let n = 0; n < unseen; n++) {
				const slot = slots[n];
				const code = data.edgeEvents[slot];
				const interval = data.edgeEvents[slot + 1] | (data.edgeEvents[slot + 2] << 8);
				const buttonIndex = code & 0x0f;
				if (buttonIndex >= 7) continue;
				const pressed = Boolean(code & 0x80);
				const phase = code & 0x20 ? 'release' : 'press';
				const phaseStart = Boolean(code & 0x10);
				const estimated = eventAt[n];
				const fromPrevious = lastEdgeAt[buttonIndex] && interval < 10000 ? lastEdgeAt[buttonIndex] + interval / 10 : estimated;
				const timestamp = Math.abs(estimated - fromPrevious) <= 100 ? fromPrevious : estimated;
				lastEdgeAt[buttonIndex] = timestamp;
				const threshold = phase === 'press' ? config.iidx_button_debounce[buttonIndex] : config.sdvx_keys.button_release_debounce[buttonIndex];
				const candidate = Boolean(code & 0x40);
				if (phaseStart) { lastPhase[buttonIndex] = phase; phaseElapsed[buttonIndex] = 0; hideRecovery[buttonIndex] = false; }
				else if (lastPhase[buttonIndex] === phase) phaseElapsed[buttonIndex] += interval;
				const detected = data.edgeDetected?.[slot / 3];
				const chatter = candidate && threshold > 0 && (detected ??
					(lastPhase[buttonIndex] === phase && phaseElapsed[buttonIndex] < threshold));
				const isRecovery = hideRecovery[buttonIndex] && !candidate && (phase === 'press' ? pressed : !pressed);
				if (isRecovery) hideRecovery[buttonIndex] = false;
				if (chatter) hideRecovery[buttonIndex] = true;
				const id = nextLogId++;
				const waveTimestamp = Math.max(0, waveformNow - Math.max(0, sampleTime - timestamp));
				if (!isRecovery) events.push({ id, timestamp, waveTimestamp, button: labels[buttonIndex], buttonIndex, pressed, phase, interval, threshold, chatter, logVisible: true });
				if (chatter) markers.push({ id, timestamp: waveTimestamp, buttonIndex, phase });

				if (autoSetting && candidate && lastPhase[buttonIndex] === phase) {
					setButtonDebounce(buttonIndex, Math.min(160, Math.max(threshold, phaseElapsed[buttonIndex] + 1)), phase);
				}
			}
			if (events.length) edgeEvents = [...events.reverse(), ...edgeEvents].slice(0, maxLogEntries);
			if (markers.length) waveMarkers = [...waveMarkers, ...markers];
			trimWaveHistory();
			if (selectedLogId !== undefined && !edgeEvents.some((entry) => entry.id === selectedLogId)) selectedLogId = undefined;
			returnLogToNewest();
			previousSequence = data.edgeSequence;
		};
		device.addEventListener('inputreport', onInput);
		return () => {
			device.removeEventListener('inputreport', onInput);
			if (resetDiagnostics === resetStream) resetDiagnostics = undefined;
		};
	});

	function setButtonDebounce(index: number, value: number, mode: 'press' | 'release' = edgeMode): void {
		const bounded = Math.max(0, Math.min(160, Math.round(value)));
		const current = mode === 'press' ? config.iidx_button_debounce : config.sdvx_keys.button_release_debounce;
		if (bulkSetting) {
			if (mode === 'press') config.iidx_button_debounce = current.map(() => bounded);
			else config.sdvx_keys.button_release_debounce = current.map(() => bounded);
		} else {
			current[index] = bounded;
		}
		if (bounded === 0) {
			edgeEvents = edgeEvents.map((entry) => {
				const applies = entry.phase === mode && (bulkSetting || entry.buttonIndex === index);
				return applies && entry.chatter ? { ...entry, chatter: false } : entry;
			});
			waveMarkers = waveMarkers.filter((entry) => !(entry.phase === mode && (bulkSetting || entry.buttonIndex === index)));
		}
	}

	function setBulkSetting(checked: boolean): void {
		bulkSetting = checked;
		if (checked) {
			const current = edgeMode === 'press' ? config.iidx_button_debounce : config.sdvx_keys.button_release_debounce;
			const shared = current[0] ?? 0;
			if (edgeMode === 'press') config.iidx_button_debounce = current.map(() => shared);
			else config.sdvx_keys.button_release_debounce = current.map(() => shared);
		}
	}

	function resetLog(): void {
		resetDiagnostics?.();
		buttonEdges = Array(7).fill(0);
		chatterCounts = Array(7).fill(0);
		edgeEvents = [];
		resetWaveHistory(outputButtons);
		if (logViewport) logViewport.scrollTop = 0;
		updateLogScroll();
	}
</script>

{#if config.version < 41}
	<div class="rounded-lg border border-amber-500/40 bg-amber-500/10 p-4" role="status">
		<strong>{tr('対応ファームウェアが必要です', 'Firmware update required')}</strong>
		<p class="mt-1 text-sm">{tr('B1〜B7個別のデバウンスを使うには、実験版ファームウェアを書き込んでください。', 'Flash the experimental firmware to use individual B1–B7 debounce settings.')}</p>
	</div>
{:else}
	<div class="chatter-test">
		<div class="left-column">
			<section class="panel waveform-panel" aria-label={tr('チャタリング波形', 'Chatter waveform')}>
				<h2>{tr('チャタリング波形', 'Chatter waveform')}</h2>
				<div class="wave-legend"><span class="legend-press">{tr('押下中', 'Pressed')}</span><span class="legend-release">{tr('解放中', 'Released')}</span><span class="legend-chatter">{tr('チャタリング', 'Chatter')}</span></div>
				<svg class="waveform" viewBox="0 0 700 320" preserveAspectRatio="none" role="img" aria-label={tr('ボタン入力波形', 'Button input waveform')}>
					{#each [0, 1, 2, 3, 4] as tick}
						<line x1={48 + tick * 161.5} x2={48 + tick * 161.5} y1="10" y2="310" class="grid-line" />
					{/each}
					{#each waveforms as wave}
						<rect x="0" y={wave.y - 19} width="700" height="38" rx="5" class:lane-active={wave.active} class="lane-background" />
						<circle cx="9" cy={wave.y} r="7" class:status-active={wave.active} class="status-dot" />
						<text x="18" y={wave.y + 4} class:label-active={wave.active} class="wave-label">{wave.label}</text>
						<line x1="48" x2="694" y1={wave.low} y2={wave.low} class="lane-line" />
						{#each wave.segments as segment}
							<rect x={segment.x} y={segment.pressed ? wave.high - 4 : wave.low - 2} width={segment.width} height={segment.pressed ? 16 : 4} rx="2" class:pressed-segment={segment.pressed} class="wave-segment" />
						{/each}
						{#each wave.transitions as x}
							<line x1={x} x2={x} y1={wave.high} y2={wave.low} class="wave-transition" />
						{/each}
						{#each wave.points as point (point.id)}
							<line x1={point.x} x2={point.x} y1={(point.phase === 'press' ? wave.high : wave.low) - 7} y2={(point.phase === 'press' ? wave.high : wave.low) + 7} class="chatter-marker" />
							<circle cx={point.x} cy={point.phase === 'press' ? wave.high : wave.low} r="5" class="chatter-marker-point"><title>{wave.label} · {point.phase === 'press' ? tr('押下時', 'On press') : tr('解放時', 'On release')} · {tr('チャタリング', 'Chatter')}</title></circle>
						{/each}
						{#if wave.selectedX !== undefined}
							<line x1={wave.selectedX} x2={wave.selectedX} y1={wave.high - 14} y2={wave.low + 14} class="selected-wave-edge" data-testid="selected-wave-edge" />
						{/if}
					{/each}
				</svg>
				<div class="wave-history">
					<input type="range" min="0" max={maxBackMs} step="any" value={viewBackMs} oninput={moveWaveHistory} aria-label={tr('波形履歴', 'Waveform history')} title={tr('左へ動かすと保持中の入力ログまでさかのぼります。右端で最新の波形に戻ります。', 'Move left to review the retained input log. Move to the right edge to follow live input.')} data-testid="wave-history" />
				</div>
				<div class="summary-row">
					<div class="summary total"><span>{tr('総エッジ数', 'Total edges')}</span><strong>{totalEdges}</strong></div>
					<div class="summary chatter"><span>{tr('検知数', 'Detected')}</span><strong>{detectedCount}</strong></div>
					<div class="summary percent"><span>{tr('割合', 'Percent')}</span><strong>{detectionRate.toFixed(1)}%</strong></div>
				</div>
			</section>
			<section class="panel log-panel" aria-label={tr('入力ログ', 'Input log')}>
				<div class="panel-heading">
					<h2>{tr('入力ログ', 'Input log')}</h2>
					<div class="log-heading-actions">
						<div class="log-filter" role="group" aria-label={tr('入力ログ表示', 'Input log filter')}>
							<button type="button" class:log-filter-selected={logFilter === 'all'} aria-pressed={logFilter === 'all'} onclick={() => logFilter = 'all'}>{tr('すべて', 'All')}</button>
							<button type="button" class:log-filter-selected={logFilter === 'press'} aria-pressed={logFilter === 'press'} onclick={() => logFilter = 'press'}>{tr('押下', 'Press')}</button>
							<button type="button" class:log-filter-selected={logFilter === 'release'} aria-pressed={logFilter === 'release'} onclick={() => logFilter = 'release'}>{tr('解放', 'Release')}</button>
						</div>
						<button type="button" class="reset-log" onclick={resetLog}>{tr('リセット', 'Reset')}</button>
					</div>
				</div>
				<div class="log-header"><span>{tr('ボタン', 'Button')}</span><span>{tr('操作', 'Action')}</span><span>{tr('間隔', 'Interval')}</span><span>{tr('設定値', 'Setting')}</span><span>{tr('判定', 'Result')}</span></div>
				<div class="log-scroll">
					<div class="log-list" bind:this={logViewport} role="region" aria-label={tr('入力ログ一覧', 'Input log entries')} onscroll={onLogScroll} data-testid="input-log">
						{#if visibleLog.length === 0}<p class="empty">{tr('ボタンを押すと入力履歴を表示します。', 'Press a button to see its input history.')}</p>{/if}
						<div aria-hidden="true" style:height={`${firstRenderedLog * logRowHeight}px`}></div>
						{#each renderedLog as entry (entry.id)}
							<button type="button" class="log-entry" class:chatter-entry={entry.chatter} class:selected-log-entry={entry.id === selectedLogId} aria-pressed={entry.id === selectedLogId} onclick={() => focusLog(entry)} data-testid="input-log-entry" data-log-id={entry.id}><b>{entry.button}</b><span>{entry.phase === 'press' ? tr('押下', 'Press') : tr('解放', 'Release')}</span><span class="log-measure"><span>{entry.interval >= 10000 ? '999.9' : (entry.interval / 10).toFixed(1)}</span><span>ms</span></span><span class="log-measure"><span>{(entry.threshold / 10).toFixed(1)}</span><span>ms</span></span><span>{entry.chatter ? tr('検知', 'Detected') : tr('通常', 'Normal')}</span></button>
						{/each}
						<div aria-hidden="true" style:height={`${(visibleLog.length - lastRenderedLog) * logRowHeight}px`}></div>
					</div>
					<button type="button" class="log-scrollbar" aria-label={tr('入力ログをスクロール', 'Scroll input log')} onpointerdown={startLogDrag} onpointermove={moveLogDrag} onpointerup={stopLogDrag} onpointercancel={stopLogDrag} onkeydown={scrollLogFromKeyboard}>
						<span class="log-scrollbar-thumb" style:height={`${logThumbHeight}px`} style:transform={`translateY(${logThumbTop}px)`}></span>
					</button>
				</div>
			</section>
		</div>

		<aside class="panel settings-panel" aria-label={tr('チャタリング防止設定', 'Debounce settings')}>
			<div class="toggle-row"><span class="toggle-label">{tr('自動設定', 'Auto setting')} <HelpText label={tr('自動設定', 'Auto setting')} side="left" sideOffset={8}><p>{tr('押下時・解放時の両方の揺れを検知すると、', 'When a bounce is found on press or release,')}</p><p>{tr('それぞれの操作開始から揺れまでの時間+0.1msに設定します。', 'set that phase to time since its start +0.1 ms.')}</p></HelpText></span><UiSwitch aria-label={tr('自動設定', 'Auto setting')} bind:checked={autoSetting} /></div>
			<div class="toggle-row bulk-row"><span class="toggle-label">{tr('ボタン一括設定', 'Apply to all buttons')} <HelpText label={tr('ボタン一括設定', 'Apply to all buttons')} side="left" sideOffset={8}><p>{tr('ONにすると、選択中モードの値を', 'When on, changing a slider applies its value')}</p><p>{tr('B1〜B7へ一括適用します。', 'to B1–B7 in the selected mode.')}</p></HelpText></span><UiSwitch aria-label={tr('ボタン一括設定', 'Apply to all buttons')} checked={bulkSetting} onCheckedChange={setBulkSetting} /></div>
			<div class="mode-row" role="group" aria-label={tr('チャタリング設定モード', 'Chatter setting mode')}>
				<button type="button" class:mode-selected={edgeMode === 'press'} aria-pressed={edgeMode === 'press'} onclick={() => setEdgeMode('press')}>{tr('押下時', 'On press')}</button>
				<button type="button" class:mode-selected={edgeMode === 'release'} aria-pressed={edgeMode === 'release'} onclick={() => setEdgeMode('release')}>{tr('解放時', 'On release')}</button>
			</div>
			<div class="mode-description">
				<p>{tr('押下時はボタンを押した時のチャタリングを抑制します。', 'On press suppresses chatter when a button is pressed.')}</p>
				<p>{tr('解放時はボタンを離した時のチャタリングを抑制します。', 'On release suppresses chatter when a button is released.')}</p>
			</div>
			<div class="button-sliders">
				{#each labels as label, i}
					<div class="slider-row">
					<div class="button-label"><b>{label}</b><small class:pressed={outputButtons & (1 << i)}>{outputButtons & (1 << i) ? tr('押下', 'Pressed') : tr('解放', 'Released')}</small></div>
						<div class="button-controls">
							<div class="button-metrics">
								<span><small>{tr('エッジ', 'Edges')}</small><b>{buttonEdges[i]}</b></span>
								<span><small>{tr('検知数', 'Detected')}</small><b>{chatterCounts[i]}</b></span>
								<span><small>{tr('割合', 'Percent')}</small><b>{percent(chatterCounts[i], buttonEdges[i])}%</b></span>
							</div>
							<SliderInput value={(edgeMode === 'press' ? config.iidx_button_debounce[i] : config.sdvx_keys.button_release_debounce[i]) / 10} onValueChange={(value) => setButtonDebounce(i, Math.round(value * 10))} min={0} max={16} step={0.1} id={`chatter-${edgeMode}-${i}`} compact />
						</div>
					</div>
				{/each}
			</div>
		</aside>
	</div>
{/if}

<style>
	.chatter-test { display:grid; grid-template-columns:minmax(0,1.25fr) minmax(280px,.75fr); gap:10px; align-items:stretch; height:100%; min-height:0; }
	.left-column { display:grid; grid-template-rows:minmax(0,1fr) 264px; gap:4px; min-width:0; min-height:0; }
	.panel { min-width:0; border:1px solid hsl(var(--border)); border-radius:12px; background:hsl(var(--card)); padding:10px; }
	.panel h2 { margin:0; font-size:1.02rem; font-weight:700; }
	.wave-legend { display:flex; flex-wrap:wrap; gap:12px; margin:8px 0 5px; color:hsl(var(--muted-foreground)); font-size:.7rem; }
	.wave-legend span { display:inline-flex; align-items:center; gap:5px; }
	.wave-legend span::before { content:""; display:inline-block; width:9px; height:9px; border-radius:2px; background:#22d3ee; }
	.wave-legend .legend-release::before { height:4px; background:#64748b; }
	.wave-legend .legend-chatter::before { border-radius:50%; background:#c2410c; }
	.waveform-panel { display:flex; flex-direction:column; min-height:0; }
	.waveform { display:block; flex:1 1 auto; min-height:150px; width:100%; height:auto; overflow:visible; }
	.grid-line { stroke:rgba(148,163,184,.2); stroke-width:1; stroke-dasharray:3 4; }
	.lane-background { fill:rgba(148,163,184,.055); }
	.lane-background.lane-active { fill:rgba(34,211,238,.095); }
	.lane-line { stroke:rgba(148,163,184,.24); stroke-width:1; }
	.status-dot { fill:#475569; }
	.status-dot.status-active { fill:#22d3ee; }
	.wave-label { fill:#cbd5e1; font-size:22px; font-weight:700; }
	.wave-label.label-active { fill:#67e8f9; }
	.wave-segment { fill:#64748b; }
	.wave-segment.pressed-segment { fill:#22d3ee; }
	.wave-transition { stroke:#22d3ee; stroke-width:3; }
	.chatter-marker { stroke:#c2410c; stroke-width:4; }
	.chatter-marker-point { fill:#c2410c; stroke:#0f172a; stroke-width:1.4; }
	.selected-wave-edge { stroke:#facc15; stroke-width:3; stroke-dasharray:4 3; }
	.wave-history { margin:-2px 2px 6px 48px; height:18px; display:flex; align-items:center; }
	.wave-history input { width:100%; margin:0; cursor:pointer; accent-color:#22d3ee; direction:rtl; }
	.summary-row { display:grid; grid-template-columns:repeat(3,minmax(0,1fr)); gap:6px; }
	.summary { display:grid; grid-template-columns:minmax(0,1fr) auto; align-items:center; gap:4px; min-width:0; overflow:hidden; border:1px solid #cbd5e1; border-radius:9px; padding:6px 7px; background:#ffffff; }
	.summary span { min-width:0; color:#475569; font-size:.69rem; white-space:nowrap; }
	.summary strong { min-width:0; max-width:100%; font-size:1rem; color:#0284c7; font-variant-numeric:tabular-nums; white-space:nowrap; text-align:right; }
	.summary.chatter strong { color:#c2410c; }
	.summary.percent strong { color:#c2410c; font-size:.92rem; }
	:global(.dark) .summary { border-color:rgba(255,255,255,.12); background:rgba(15,23,42,.35); }
	:global(.dark) .summary span { color:hsl(var(--muted-foreground)); }
	:global(.dark) .summary strong { color:#22d3ee; }
	:global(.dark) .summary.chatter strong,
	:global(.dark) .summary.percent strong { color:#c2410c; }
	.panel-heading { display:flex; justify-content:space-between; align-items:center; gap:10px; }
	.log-heading-actions { display:flex; align-items:center; gap:8px; }
	.log-filter { display:flex; gap:3px; }
	.log-filter button { border:1px solid #cbd5e1; border-radius:5px; padding:2px 7px; color:#0f172a; font-size:.68rem; line-height:1.35; background:#ffffff; opacity:1; transition:background .15s ease,border-color .15s ease,color .15s ease; }
	.log-filter button:hover { border-color:#0284c7; background:#f8fafc; }
	.log-filter button.log-filter-selected { border-color:#0369a1; background:#0369a1; color:white; font-weight:700; box-shadow:0 0 0 1px rgba(2,132,199,.2) inset; }
	.panel-heading .reset-log { color:#93c5fd; font-size:.8rem; text-decoration:underline; }
	.log-scroll { display:grid; grid-template-columns:minmax(0,1fr) 10px; gap:4px; height:190px; min-height:190px; }
	.log-list { min-width:0; min-height:0; overflow-y:auto; overflow-x:hidden; scrollbar-width:none; }
	.log-list::-webkit-scrollbar { display:none; }
	.log-scrollbar { position:relative; width:10px; height:100%; padding:0; overflow:hidden; border:0; border-radius:8px; background:rgba(100,116,139,.25); touch-action:none; cursor:pointer; }
	.log-scrollbar-thumb { position:absolute; top:0; left:1px; right:1px; display:block; max-height:100%; border-radius:7px; background:#64748b; }
	.log-header,.log-entry { display:grid; grid-template-columns:42px 58px repeat(3,minmax(72px,1fr)); gap:6px; align-items:center; }
	.log-header { margin-right:14px; padding:7px 2px; color:hsl(var(--muted-foreground)); font-size:.73rem; }
	.log-entry { width:100%; height:19px; box-sizing:border-box; border:0; border-top:1px solid hsl(var(--border)); padding:2px 2px; background:transparent; color:inherit; font-size:.72rem; line-height:1.2; text-align:left; cursor:pointer; }
	.log-entry:hover,.log-entry.selected-log-entry { background:rgba(250,204,21,.13); }
	.log-entry.selected-log-entry { box-shadow:inset 3px 0 #facc15; }
	.log-header span,.log-entry span { min-width:0; white-space:nowrap; }
	.log-measure { display:grid; grid-template-columns:5ch auto; column-gap:3px; align-items:baseline; font-variant-numeric:tabular-nums; }
	.log-measure span:first-child { text-align:right; }
	.log-entry.chatter-entry { color:#c2410c; background:rgba(249,115,22,.08); }
	.empty { color:hsl(var(--muted-foreground)); }
	.empty { padding:14px 2px; font-size:.8rem; }
	.settings-panel { display:flex; flex-direction:column; min-height:0; }
	.toggle-row { display:flex; align-items:center; justify-content:space-between; gap:10px; font-weight:650; }
	.toggle-label { display:inline-flex; align-items:center; gap:8px; }
	.bulk-row { border-top:1px solid hsl(var(--border)); padding-top:.7rem; }
	.button-sliders { display:grid; grid-template-rows:repeat(7,minmax(0,1fr)); flex:1 1 auto; min-height:0; margin-top:.15rem; }
	.mode-row { display:grid; grid-template-columns:repeat(2,minmax(0,1fr)); align-items:center; gap:4px; margin:.35rem 0 .25rem; padding:4px; border:1px solid hsl(var(--border)); border-radius:9px; background:#f8fafc; }
	.mode-row button { border:1px solid #cbd5e1; border-radius:6px; padding:.45rem .3rem; color:#0f172a; font-size:.8rem; background:#ffffff; opacity:1; transition:background .15s ease,border-color .15s ease,color .15s ease; }
	.mode-row button:hover { border-color:#0284c7; background:color-mix(in srgb,#0ea5e9 10%,hsl(var(--card))); }
	.mode-row button.mode-selected { border-color:#0369a1; background:#0369a1; color:white; font-weight:700; box-shadow:0 0 0 1px rgba(2,132,199,.2) inset; }
	:global(.dark) .log-filter button { color:hsl(var(--muted-foreground)); background:rgba(15,23,42,.62); opacity:.72; }
	:global(.dark) .log-filter button:hover { border-color:hsl(var(--muted-foreground)); color:hsl(var(--foreground)); opacity:1; }
	:global(.dark) .log-filter button.log-filter-selected { border-color:#22d3ee; background:#0e7490; color:white; opacity:1; box-shadow:0 0 0 1px rgba(34,211,238,.22) inset; }
	:global(.dark) .mode-row { background:rgba(15,23,42,.35); }
	:global(.dark) .mode-row button { color:hsl(var(--muted-foreground)); background:rgba(15,23,42,.62); opacity:.72; }
	:global(.dark) .mode-row button:hover { border-color:hsl(var(--muted-foreground)); color:hsl(var(--foreground)); opacity:1; }
	:global(.dark) .mode-row button.mode-selected { border-color:#22d3ee; background:#0e7490; color:white; opacity:1; box-shadow:0 0 0 1px rgba(34,211,238,.22) inset; }
	.mode-description { margin:0 2px .65rem; color:hsl(var(--muted-foreground)); font-size:.64rem; letter-spacing:-.04em; line-height:1.5; white-space:nowrap; }
	.slider-row { display:grid; grid-template-columns:42px minmax(0,1fr); align-items:center; gap:6px; min-height:0; padding:.35rem 0; border-top:1px solid hsl(var(--border)); }
	.button-controls { min-width:0; }
	.button-metrics { display:grid; grid-template-columns:repeat(3,minmax(0,1fr)); gap:3px; margin-bottom:2px; text-align:center; font-variant-numeric:tabular-nums; }
	.button-metrics span { display:flex; flex-direction:column; min-width:0; line-height:1.1; }
	.button-metrics small { color:hsl(var(--muted-foreground)); font-size:.59rem; white-space:nowrap; }
	.button-metrics b { font-size:.7rem; white-space:nowrap; }
	.button-label { display:grid; justify-items:center; gap:1px; line-height:1.05; }
	.button-label small { width:max-content; color:hsl(var(--muted-foreground)); font-size:.65rem; }
	.button-label small.pressed { color:#22d3ee; }
	@media(max-height:740px) { .panel { padding:8px; } .wave-legend { margin:5px 0 3px; } .wave-history { margin-bottom:3px; } .log-header { padding:4px 2px; } .left-column { grid-template-rows:minmax(0,1fr) 264px; } .slider-row { padding:0; } .mode-description { margin-bottom:.15rem; line-height:1.2; } .mode-row { margin:.2rem 0; padding:2px; } .mode-row button { padding:.3rem .2rem; } .bulk-row { padding-top:.35rem; } }
	@media(max-height:650px) { .waveform { min-height:85px; } }
	@media(max-width:850px) { .chatter-test { grid-template-columns:1fr; } .left-column { grid-template-rows:auto auto; } .settings-panel { order:-1; } }
</style>
