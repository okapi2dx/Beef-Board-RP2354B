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
	import { replayDebounce } from '$lib/types/wave-debounce';
	import { measureWaveInterval } from '$lib/types/wave-measure';

	let { config, active }: { config: Config; active: boolean } = $props();
	let outputButtons = $state(0);
	let rawButtons = $state(0);
	let signalSamples = $state<{ timestamp: number; buttonIndex: number; pressed: boolean }[]>([]);
	let outputSamples = $state<typeof signalSamples>([]);
	let waveMode = $state<'before' | 'after'>('before');
	let waveGaps = $state<{ start: number; end: number }[]>([]);
	let buttonEdges = $state<number[]>(Array(7).fill(0));
	let chatterCounts = $state<number[]>(Array(7).fill(0));
	let edgeEvents = $state<{ id: number; timestamp: number; waveTimestamp: number; button: string; buttonIndex: number; pressed: boolean; phase: 'press' | 'release'; interval: number; threshold: number; chatter: boolean; candidate: boolean; phaseElapsed: number | undefined; logVisible: boolean }[]>([]);
	let edgeMode = $state<'press' | 'release'>('press');
	let logFilter = $state<'all' | 'press' | 'release'>('all');
	let autoSetting = $state(false);
	let bulkSetting = $state(false);
	let waveformNow = $state(0);
	let viewingLive = $state(true);
	let fixedViewEnd = $state(0);
	let selectedLogId = $state<number>();
	let waveFocus = $state<{ timestamp: number; buttonIndex: number }>();
	let measurementEdges = $state<number[]>([]);
	let wavePanned = $state(false);
	let waveDragging = $state(false);
	let wavePointer: { id: number; x: number; end: number; scale: number; minCenter: number; maxCenter: number; markerId?: string } | undefined;
	let detailView = $state<{ entry: (typeof edgeEvents)[number]; centerTimestamp: number; samples: typeof signalSamples; entries: typeof edgeEvents; gaps: typeof waveGaps; availableUntil: number }>();
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
	const detailWindowMs = 50;
	const maxWaveSamples = 20000;
	const maxLogEntries = 1000;
	const logRowHeight = 26;
	const logOverscan = 5;
	const totalEdges = $derived(buttonEdges.reduce((sum, count) => sum + count, 0));
	const detectedCount = $derived(chatterCounts.reduce((sum, count) => sum + count, 0));
	const detectionRate = $derived(totalEdges ? detectedCount / totalEdges * 100 : 0);
	const percent = (detected: number, edges: number) => (edges ? detected / edges * 100 : 0).toFixed(1);
	const currentLog = $derived(edgeEvents.map(withCurrentSetting));
	const visibleLog = $derived(currentLog.filter((entry) => entry.logVisible && (logFilter === 'all' || entry.pressed === (logFilter === 'press'))));
	const oldestWaveTime = $derived(edgeEvents.at(-1)?.waveTimestamp);
	const earliestViewEnd = $derived(oldestWaveTime === undefined ? waveformNow : Math.max(0, oldestWaveTime + waveWindowMs / 2));
	const maxBackMs = $derived(Math.max(0, waveformNow - earliestViewEnd));
	const viewEnd = $derived(viewingLive ? waveformNow : waveFocus || wavePanned ? fixedViewEnd : Math.max(earliestViewEnd, fixedViewEnd));
	const viewBackMs = $derived(Math.min(maxBackMs, Math.max(0, waveformNow - viewEnd)));
	const firstRenderedLog = $derived(Math.max(0, Math.min(visibleLog.length, Math.floor(logScrollTop / logRowHeight) - logOverscan)));
	const lastRenderedLog = $derived(Math.min(visibleLog.length, Math.ceil((logScrollTop + logViewportHeight) / logRowHeight) + logOverscan));
	const renderedLog = $derived(visibleLog.slice(firstRenderedLog, lastRenderedLog));
	const selectedLog = $derived(detailView ? withCurrentSetting(detailView.entry) : currentLog.find((entry) => entry.id === selectedLogId));
	const waveMarkers = $derived((detailView?.entries ?? edgeEvents).map(withCurrentSetting).filter((entry) => entry.chatter)
		.map((entry) => ({ id: entry.id, timestamp: entry.waveTimestamp, buttonIndex: entry.buttonIndex, phase: entry.phase, pressed: entry.pressed })));
	const displayedWindowMs = $derived(detailView ? detailWindowMs : waveWindowMs);
	const displayedEnd = $derived(detailView ? detailView.centerTimestamp + detailWindowMs / 2 : viewEnd);
	const replayedSamples = $derived(replayDebounce(detailView?.samples ?? signalSamples,
		config.iidx_button_debounce, config.sdvx_keys.button_release_debounce, detailView?.gaps ?? waveGaps));
	const intervalMeasurement = $derived(detailView && measurementEdges.length === 2
		? measureWaveInterval((waveMode === 'before' ? detailView.samples : replayedSamples)
			.filter(sample => sample.buttonIndex === detailView?.entry.buttonIndex),
			measurementEdges[0], measurementEdges[1], detailView.gaps) : undefined);
	const measurementPositions = $derived(measurementEdges.map(timestamp => 48 +
		(timestamp - displayedEnd + displayedWindowMs) / displayedWindowMs * 646));
	const displayedGaps = $derived((detailView?.gaps ?? waveGaps)
		.filter((gap) => gap.end >= displayedEnd - displayedWindowMs && gap.start <= displayedEnd)
		.map((gap) => {
			const x = Math.max(48, 48 + (gap.start - displayedEnd + displayedWindowMs) / displayedWindowMs * 646);
			const endX = Math.min(694, 48 + (gap.end - displayedEnd + displayedWindowMs) / displayedWindowMs * 646);
			return { x, width: Math.max(3, endX - x) };
		}));
	const detailUnavailableX = $derived(detailView && detailView.availableUntil < displayedEnd
		? Math.max(48, 694 - (displayedEnd - detailView.availableUntil) / detailWindowMs * 646) : undefined);
	const logThumbHeight = $derived(Math.min(logViewportHeight, Math.max(24, logViewportHeight * logViewportHeight / (logScrollExtent + logViewportHeight))));
	const logThumbTop = $derived(logScrollExtent ? logScrollTop / logScrollExtent * (logViewportHeight - logThumbHeight) : 0);

	function withCurrentSetting(entry: (typeof edgeEvents)[number]): (typeof edgeEvents)[number] {
		const threshold = entry.phase === 'press' ? config.iidx_button_debounce[entry.buttonIndex] : config.sdvx_keys.button_release_debounce[entry.buttonIndex];
		return { ...entry, threshold, chatter: entry.candidate && threshold > 0 && entry.phaseElapsed !== undefined && entry.phaseElapsed < threshold };
	}

	function updateLogScroll(): void {
		if (!logViewport) return;
		logViewportHeight = logViewport.clientHeight;
		logScrollExtent = Math.max(0, logViewport.scrollHeight - logViewportHeight);
		logScrollTop = logViewport.scrollTop;
	}

	function focusLog(entry: (typeof edgeEvents)[number]): void {
		if (detailView) return;
		wavePanned = false;
		waveFocus = undefined;
		selectedLogId = entry.id;
		viewingLive = false;
		fixedViewEnd = entry.waveTimestamp + waveWindowMs / 2;
	}

	function showLogDetail(entry: (typeof edgeEvents)[number], centerTimestamp = entry.waveTimestamp): void {
		measurementEdges = [];
		wavePanned = false;
		waveFocus = undefined;
		detailView = {
			entry: { ...(edgeEvents.find((log) => log.id === entry.id) ?? entry) },
			centerTimestamp,
			samples: signalSamples.filter((sample) => sample.buttonIndex === entry.buttonIndex).map((sample) => ({ ...sample })),
			entries: edgeEvents.filter((log) => log.buttonIndex === entry.buttonIndex).map((log) => ({ ...log })),
			gaps: waveGaps.map((gap) => ({ ...gap })),
			availableUntil: waveformNow
		};
		selectedLogId = entry.id;
		viewingLive = false;
		lastLogInteractionAt = 0;
	}

	function focusWaveAt(buttonIndex: number, timestamp: number): void {
		if (timestamp < 0 || timestamp > (detailView?.availableUntil ?? waveformNow)) return;
		if (detailView) {
			const edges = waveforms.find(wave => wave.buttonIndex === buttonIndex)?.edgeLabels ?? [];
			const edge = edges.find(edge => Math.abs(edge.timestamp - timestamp) < 1e-7);
			if (!edge) return;
			if (measurementEdges.some(selected => Math.abs(selected - edge.timestamp) < 1e-7)) return;
			measurementEdges = measurementEdges.length === 1 ? [...measurementEdges, edge.timestamp] : [edge.timestamp];
			return;
		}
		const tolerance = displayedWindowMs * 10 / 646;
		const nearby = edgeEvents.filter((entry) => entry.buttonIndex === buttonIndex)
			.reduce<(typeof edgeEvents)[number] | undefined>((closest, entry) =>
				!closest || Math.abs(entry.waveTimestamp - timestamp) < Math.abs(closest.waveTimestamp - timestamp) ? entry : closest, undefined);
		const entry = nearby && Math.abs(nearby.waveTimestamp - timestamp) <= tolerance ? nearby : undefined;
		selectedLogId = entry?.id;
		wavePanned = false;
		waveFocus = { timestamp, buttonIndex };
		viewingLive = false;
		fixedViewEnd = timestamp + waveWindowMs / 2;
		lastLogInteractionAt = 0;
		if (entry) {
			if (!visibleLog.some((log) => log.id === entry.id)) logFilter = 'all';
			void tick().then(() => {
				const index = visibleLog.findIndex((log) => log.id === entry.id);
				if (logViewport && index >= 0) {
					logViewport.scrollTop = Math.max(0, index * logRowHeight - (logViewport.clientHeight - logRowHeight) / 2);
					updateLogScroll();
				}
			});
		}
	}

	function focusWave(event: MouseEvent, clickedMarkerId?: string): void {
		const bounds = (event.currentTarget as SVGSVGElement).getBoundingClientRect();
		const x = (event.clientX - bounds.left) / bounds.width * 700;
		const y = (event.clientY - bounds.top) / bounds.height * 320;
		if (x < 48 || x > 694) return;
		if (displayedGaps.some((gap) => x >= gap.x && x <= gap.x + gap.width) ||
			(detailUnavailableX !== undefined && x >= detailUnavailableX)) return;
		const markerId = clickedMarkerId ?? (event.target as Element).closest('[data-wave-marker-id]')?.getAttribute('data-wave-marker-id');
		const marker = markerId == null ? undefined : edgeEvents.find((entry) => entry.id === Number(markerId));
		if (marker) {
			const markerX = 48 + (marker.waveTimestamp - displayedEnd + displayedWindowMs) / displayedWindowMs * 646;
			if (waveforms.some((wave) => wave.buttonIndex === marker.buttonIndex && wave.transitions.some((edge) => Math.abs(edge - markerX) < .001))) {
				focusWaveAt(marker.buttonIndex, marker.waveTimestamp);
				return;
			}
		}
		const toleranceX = 6 * 700 / bounds.width;
		const toleranceY = 6 * 320 / bounds.height;
		const wave = waveforms.find((wave) => y >= wave.high - toleranceY && y <= wave.low + toleranceY);
		if (!wave) return;
		const edge = wave.edgeLabels.reduce<(typeof wave.edgeLabels)[number] | undefined>((closest, edge) =>
			closest === undefined || Math.abs(edge.x - x) < Math.abs(closest.x - x) ? edge : closest, undefined);
		if (edge === undefined || Math.abs(edge.x - x) > toleranceX) return;
		focusWaveAt(wave.buttonIndex, edge.timestamp);
	}

	function startWaveDrag(event: PointerEvent): void {
		if (event.button !== 0 || !event.isPrimary) return;
		const svg = event.currentTarget as SVGSVGElement;
		const bounds = svg.getBoundingClientRect();
		if (event.clientX < bounds.left + 48 / 700 * bounds.width || event.clientX > bounds.left + 694 / 700 * bounds.width) return;
		wavePointer = { id: event.pointerId, x: event.clientX, end: displayedEnd,
			scale: displayedWindowMs / (bounds.width * 646 / 700),
			minCenter: Math.max(0, oldestWaveTime ?? 0), maxCenter: detailView?.availableUntil ?? waveformNow,
			markerId: (event.target as Element).closest('[data-wave-marker-id]')?.getAttribute('data-wave-marker-id') ?? undefined };
		waveDragging = false;
		svg.focus();
		svg.setPointerCapture(event.pointerId);
		event.preventDefault();
	}

	function moveWaveDrag(event: PointerEvent): void {
		if (!wavePointer || wavePointer.id !== event.pointerId) return;
		const delta = event.clientX - wavePointer.x;
		if (!waveDragging && Math.abs(delta) < 3) return;
		waveDragging = true;
		wavePanned = true;
		viewingLive = false;
		const center = Math.max(wavePointer.minCenter, Math.min(wavePointer.maxCenter,
			wavePointer.end - displayedWindowMs / 2 - delta * wavePointer.scale));
		if (detailView) detailView.centerTimestamp = center;
		else fixedViewEnd = center + waveWindowMs / 2;
	}

	function stopWaveDrag(event: PointerEvent): void {
		if (!wavePointer || wavePointer.id !== event.pointerId) return;
		if (event.type === 'pointerup' && !waveDragging) focusWave(event, wavePointer.markerId);
		wavePointer = undefined;
		waveDragging = false;
		const svg = event.currentTarget as SVGSVGElement;
		if (svg.hasPointerCapture(event.pointerId)) svg.releasePointerCapture(event.pointerId);
	}

	function focusWaveFromKeyboard(event: KeyboardEvent): void {
		if (event.key === 'Escape') {
			event.preventDefault();
			detailView = undefined;
			waveFocus = undefined;
			wavePanned = false;
			wavePointer = undefined;
			waveDragging = false;
			selectedLogId = undefined;
			viewingLive = true;
			returnLogToNewest();
			return;
		}
		if (event.key !== 'Enter' && event.key !== ' ') return;
		event.preventDefault();
		const wave = waveforms.find((wave) => wave.buttonIndex === (detailView?.entry.buttonIndex ?? selectedLog?.buttonIndex ?? 0));
		const edge = wave?.edgeLabels.reduce<(typeof wave.edgeLabels)[number] | undefined>((closest, edge) =>
			closest === undefined || Math.abs(edge.x - 371) < Math.abs(closest.x - 371) ? edge : closest, undefined);
		if (wave && edge !== undefined) focusWaveAt(wave.buttonIndex, edge.timestamp);
	}

	function closeLogDetail(): void {
		const entry = detailView?.entry;
		detailView = undefined;
		if (!entry) return;
		focusLog(entry);
		lastLogInteractionAt = 0;
		void tick().then(() => {
			const index = visibleLog.findIndex((log) => log.id === entry.id);
			if (logViewport && index >= 0) {
				logViewport.scrollTop = Math.max(0, index * logRowHeight - (logViewport.clientHeight - logRowHeight) / 2);
				updateLogScroll();
			}
		});
	}

	function onLogScroll(): void {
		updateLogScroll();
		if (detailView || performance.now() - lastLogInteractionAt > 500) return;
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
		wavePanned = false;
		const back = Number((event.currentTarget as HTMLInputElement).value);
		selectedLogId = undefined;
		waveFocus = undefined;
		viewingLive = back === 0;
		if (!viewingLive) fixedViewEnd = waveformNow - back;
	}

	function setEdgeMode(mode: 'press' | 'release'): void {
		edgeMode = mode;
	}

	function resetWaveHistory(initialButtons = 0, initialOutputButtons = initialButtons): void {
		wavePanned = false;
		wavePointer = undefined;
		waveDragging = false;
		const now = performance.now();
		waveClock = new WaveClock(now);
		waveClock.record(now, initialButtons, false);
		waveformNow = 0;
		viewingLive = true;
		fixedViewEnd = 0;
		selectedLogId = undefined;
		waveFocus = undefined;
		detailView = undefined;
		rawButtons = initialButtons;
		signalSamples = labels.map((_, buttonIndex) => ({ timestamp: -1, buttonIndex, pressed: Boolean(initialButtons & (1 << buttonIndex)) }));
		outputSamples = labels.map((_, buttonIndex) => ({ timestamp: -1, buttonIndex, pressed: Boolean(initialOutputButtons & (1 << buttonIndex)) }));
		waveGaps = [];
	}

	function trimWaveHistory(): void {
		const oldest = edgeEvents.at(-1);
		if (!oldest) return;
		const cutoff = oldest.waveTimestamp - waveWindowMs / 2;
		const trim = (history: typeof signalSamples) => {
			const retained = history.filter((sample) => sample.timestamp >= cutoff).slice(-maxWaveSamples);
			const historyStart = Math.max(cutoff, retained[0]?.timestamp ?? cutoff);
			const seeds = labels.flatMap((_, buttonIndex) => {
				const previous = history.findLast((sample) => sample.buttonIndex === buttonIndex && sample.timestamp < historyStart);
				return previous ? [previous] : [];
			});
			return [...seeds, ...retained].sort((a, b) => a.timestamp - b.timestamp);
		};
		signalSamples = trim(signalSamples);
		outputSamples = trim(outputSamples);
		waveGaps = waveGaps.filter((gap) => gap.end >= cutoff);
	}

	const waveforms = $derived.by(() => labels.flatMap((label, buttonIndex) => {
		if (detailView && buttonIndex !== detailView.entry.buttonIndex) return [];
		const y = detailView ? 160 : 34 + buttonIndex * 38;
		const windowStart = displayedEnd - displayedWindowMs;
		const sourceSamples = (waveMode === 'before' ? detailView?.samples ?? signalSamples
			: replayedSamples)
			.filter((sample) => sample.buttonIndex === buttonIndex && sample.timestamp <= (detailView?.availableUntil ?? waveformNow));
		const samples = sourceSamples.filter((sample) => sample.timestamp >= windowStart && sample.timestamp <= displayedEnd);
		const previousSample = sourceSamples.findLast((sample) => sample.timestamp < windowStart);
		let state = previousSample?.pressed ?? false;
		const high = y - (detailView ? 70 : 10);
		const low = y + (detailView ? 70 : 10);
		let startX = 48;
		const segments: { x: number; width: number; pressed: boolean }[] = [];
		const transitions: number[] = [];
		const edgeLabels: { x: number; pressed: boolean; timestamp: number }[] = [];
		const points: { x: number; phase: 'press' | 'release'; pressed: boolean; id: number }[] = [];
		for (const sample of samples) {
			const nextState = sample.pressed;
			if (nextState === state) continue;
			const x = Math.max(48, Math.min(694, 48 + (sample.timestamp - windowStart) / displayedWindowMs * 646));
			segments.push({ x: startX, width: Math.max(0, x - startX), pressed: state });
			transitions.push(x);
			edgeLabels.push({ x, pressed: nextState, timestamp: sample.timestamp });
			startX = x;
			state = nextState;
		}
		segments.push({ x: startX, width: Math.max(0, 694 - startX), pressed: Boolean(state) });
		for (const entry of waveMarkers) {
			if (entry.buttonIndex !== buttonIndex || entry.timestamp < windowStart || entry.timestamp > displayedEnd) continue;
			const threshold = entry.phase === 'press' ? config.iidx_button_debounce[buttonIndex] : config.sdvx_keys.button_release_debounce[buttonIndex];
			if (!detailView && threshold <= 0) continue;
			const x = Math.max(48, Math.min(694, 48 + (entry.timestamp - windowStart) / displayedWindowMs * 646));
			points.push({ x, phase: entry.phase, pressed: waveMode === 'before' ? entry.pressed : sourceSamples.findLast((sample) => sample.timestamp <= entry.timestamp)?.pressed ?? false, id: entry.id });
		}
		const focusTimestamp = waveFocus?.timestamp ?? selectedLog?.waveTimestamp;
		const focusButtonIndex = waveFocus?.buttonIndex ?? selectedLog?.buttonIndex;
		const selectedX = focusButtonIndex === buttonIndex && focusTimestamp !== undefined && focusTimestamp >= windowStart && focusTimestamp <= displayedEnd
			? Math.max(48, Math.min(694, 48 + (focusTimestamp - windowStart) / displayedWindowMs * 646)) : undefined;
		const detailTimestamp = detailView?.centerTimestamp;
		const displayedPressed = sourceSamples.findLast((sample) => sample.timestamp <= (detailTimestamp ?? waveformNow))?.pressed ?? false;
		return [{ label, buttonIndex, y, high, low, segments, transitions, edgeLabels, points, selectedX, active: displayedPressed }];
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
		edgeEvents = [];
		resetWaveHistory();
		if (config.version < 41) return;
		const resetStream = () => {
			lastEdgeAt.fill(0);
			lastPhase.fill(undefined);
			phaseElapsed.fill(0);
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
			const nextRawButtons = data.rawButtons & 0x7f;
			if (!detailView && !waveFocus && !wavePanned && nextButtons & ~outputButtons) { viewingLive = true; selectedLogId = undefined; lastLogInteractionAt = 0; }
			const hasEdge = previousSequence !== undefined && data.edgeSequence !== previousSequence;
			waveformNow = waveClock.record(sampleTime, nextRawButtons, hasEdge || nextRawButtons !== rawButtons);
			outputButtons = nextButtons;
			rawButtons = nextRawButtons;
			const recordOutputState = () => {
				const changes = labels.flatMap((_, buttonIndex) => {
					const pressed = Boolean(nextButtons & (1 << buttonIndex));
					const previous = outputSamples.findLast((sample) => sample.buttonIndex === buttonIndex);
					return previous?.pressed === pressed ? [] : [{ timestamp: waveformNow, buttonIndex, pressed }];
				});
				if (changes.length) outputSamples = [...outputSamples, ...changes];
			};
			const reconcileRawState = () => {
				const corrections = labels.flatMap((_, buttonIndex) => {
					const pressed = Boolean(nextRawButtons & (1 << buttonIndex));
					const previous = signalSamples.findLast((sample) => sample.buttonIndex === buttonIndex);
					return previous?.pressed === pressed ? [] : [{ timestamp: waveformNow, buttonIndex, pressed }];
				});
				if (corrections.length) signalSamples = [...signalSamples, ...corrections];
			};
			if (previousSequence === undefined) { reconcileRawState(); recordOutputState(); previousSequence = data.edgeSequence; return; }
			const { slots, lost } = diagnosticEventSlots(previousSequence, data.edgeSequence);
			const unseen = slots.length;
			if (!unseen) { reconcileRawState(); recordOutputState(); return; }
			if (lost) resetStream();
			const now = sampleTime;
			const events: typeof edgeEvents = [];
			const rawSamples: typeof signalSamples = [];
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
				if (phaseStart) { lastPhase[buttonIndex] = phase; phaseElapsed[buttonIndex] = 0; }
				else if (lastPhase[buttonIndex] === phase) phaseElapsed[buttonIndex] += interval;
				const detected = data.edgeDetected?.[slot / 3];
				const chatter = candidate && threshold > 0 && (detected ??
					(lastPhase[buttonIndex] === phase && phaseElapsed[buttonIndex] < threshold));
				const id = nextLogId++;
				const waveTimestamp = waveformNow - Math.max(0, sampleTime - timestamp);
				rawSamples.push({ timestamp: waveTimestamp, buttonIndex, pressed });
				events.push({ id, timestamp, waveTimestamp, button: labels[buttonIndex], buttonIndex, pressed, phase, interval, threshold, chatter, candidate, phaseElapsed: lastPhase[buttonIndex] === phase ? phaseElapsed[buttonIndex] : undefined, logVisible: true });

				if (autoSetting && candidate && lastPhase[buttonIndex] === phase) {
					setButtonDebounce(buttonIndex, Math.min(160, Math.max(threshold, phaseElapsed[buttonIndex] + 1)), phase);
				}
			}
			// A complete short burst can arrive while the idle-compressed clock is
			// still at zero. Preserve its recorded intervals instead of clamping
			// every edge to the same position.
			const padding = rawSamples.length ? Math.max(0, -Math.min(...rawSamples.map((sample) => sample.timestamp))) : 0;
			if (padding) {
				waveClock.virtualNow += padding;
				waveformNow += padding;
				for (const sample of rawSamples) sample.timestamp += padding;
				for (const entry of events) entry.waveTimestamp += padding;
			}
			if (lost) {
				const end = rawSamples.length ? Math.min(...rawSamples.map((sample) => sample.timestamp)) : waveformNow;
				waveGaps = [...waveGaps, { start: Math.min(end, signalSamples.at(-1)?.timestamp ?? 0), end }];
			}
			if (rawSamples.length) signalSamples = [...signalSamples, ...rawSamples].sort((a, b) => a.timestamp - b.timestamp);
			reconcileRawState();
			recordOutputState();
			if (events.length) edgeEvents = [...events.reverse(), ...edgeEvents].slice(0, maxLogEntries);
			trimWaveHistory();
			if (!detailView && selectedLogId !== undefined && !edgeEvents.some((entry) => entry.id === selectedLogId)) selectedLogId = undefined;
			if (events.length) {
				detailView = undefined;
				waveFocus = undefined;
				wavePanned = false;
				wavePointer = undefined;
				waveDragging = false;
				selectedLogId = undefined;
				viewingLive = true;
				returnLogToNewest();
			}
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
		resetWaveHistory(rawButtons, outputButtons);
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
			<section class="panel waveform-panel" class:wave-detail-panel={detailView !== undefined} aria-label={tr('チャタリング波形', 'Chatter waveform')}>
				<div class="panel-heading wave-heading">
					<div class="wave-title">
						<h2>{detailView ? `${tr('入力波形', 'Input waveform')} — ${detailView.entry.button} · 50ms` : tr('チャタリング波形', 'Chatter waveform')}</h2>
						<HelpText label={tr('チャタリング波形と詳細表示', 'Chatter waveform and details')} side="bottom" sideOffset={8} wrap>
							<p>{tr('「詳細」で50ms表示。2つのエッジを選ぶと区間・押下・解放時間を測定します。3つ目で選び直し、ドラッグで移動。「戻る」で選択ログへ、新入力・Escで最新へ戻ります。', 'Details shows 50ms. Select two edges to measure total, pressed and released time. A third edge starts a new measurement; drag to pan. Back returns to the log; new input or Esc returns to live view.')}</p>
						</HelpText>
					</div>
					<div class="wave-actions">
						<div class="wave-mode" role="group" aria-label={tr('波形の表示', 'Waveform display')}>
							<button type="button" class:wave-mode-selected={waveMode === 'before'} aria-pressed={waveMode === 'before'} onclick={() => { if (waveMode !== 'before') measurementEdges = []; waveMode = 'before'; }}>{tr('抑制前', 'Before suppression')}</button>
							<button type="button" class:wave-mode-selected={waveMode === 'after'} aria-pressed={waveMode === 'after'} onclick={() => { if (waveMode !== 'after') measurementEdges = []; waveMode = 'after'; }}>{tr('抑制後', 'After suppression')}</button>
						</div>
					{#if detailView}
  <button type="button" class="wave-detail-button" onclick={closeLogDetail}>{tr('戻る', 'Back')}</button>
{:else}
  <button type="button" class="wave-detail-button" disabled={!selectedLog} onclick={() => { if (selectedLog) showLogDetail(selectedLog); }} title={tr('入力ログを選択して、そのボタンの50ms波形を表示します。', 'Select an input log to show its button waveform over 50ms.')} data-testid="wave-detail">{tr('詳細', 'Details')}</button>
{/if}
</div>
</div>
<div class="wave-legend"><span class="legend-press">{tr('押下中', 'Pressed')}</span><span class="legend-release">{tr('解放中', 'Released')}</span><span class="legend-chatter">{tr('チャタリング', 'Chatter')}</span>{#if displayedGaps.length}<span class="wave-note missing-note">{tr('入力イベントの欠落あり・再現できない区間', 'Missing input events: this range cannot be replayed')}</span>{/if}</div>
				<svg class="waveform" class:detail-waveform={detailView !== undefined} viewBox="0 0 700 320" preserveAspectRatio="none" role="button" tabindex="0" class:wave-dragging={waveDragging} onpointerdown={startWaveDrag} onpointermove={moveWaveDrag} onpointerup={stopWaveDrag} onpointercancel={stopWaveDrag} onlostpointercapture={stopWaveDrag} onkeydown={focusWaveFromKeyboard} aria-label={detailView ? `${detailView.entry.button} ${tr('詳細波形・50ms', 'detail waveform, 50ms')}` : tr('ボタン入力波形', 'Button input waveform')} data-testid="button-waveform">
					{#if detailView}
						<rect x="48" y="10" width="646" height="275" rx="4" class="detail-plot-background" />
						{#each Array.from({ length: 21 }, (_, i) => i) as tick}
							<line x1={48 + tick * 32.3} x2={48 + tick * 32.3} y1="10" y2="285" class="minor-grid-line" />
						{/each}
					{/if}
					{#each [0, 1, 2, 3, 4] as tick}
						<line x1={48 + tick * 161.5} x2={48 + tick * 161.5} y1="10" y2="285" class="grid-line" />
						<text x={48 + tick * 161.5} y="310" text-anchor={tick === 0 ? 'start' : tick === 4 ? 'end' : 'middle'} class="time-label">{detailView ? tick * 12.5 : Math.round(-displayedWindowMs + tick * displayedWindowMs / 4)}ms</text>
					{/each}
					{#if detailView && measurementPositions.length === 2}
						<rect x={Math.max(48, Math.min(...measurementPositions))} y="45" width={Math.max(0, Math.min(694, Math.max(...measurementPositions)) - Math.max(48, Math.min(...measurementPositions)))} height="220" class="measurement-range" />
					{/if}
					{#each waveforms as wave}
						{#if !detailView}<rect x="0" y={wave.y - 17} width="700" height="34" rx="5" class:lane-active={wave.active} class="lane-background" />{/if}
						<circle cx="9" cy={wave.y} r="7" class:status-active={wave.active} class="status-dot" />
						<text x="18" y={wave.y + 4} class:label-active={wave.active} class="wave-label">{wave.label}</text>
						<line x1="48" x2="694" y1={wave.low} y2={wave.low} class="lane-line" />
						{#if !detailView}<line x1="0" x2="694" y1={wave.y + 19} y2={wave.y + 19} class="lane-divider" />{/if}
						{#if detailView}
							<line x1="48" x2="694" y1={wave.high} y2={wave.high} class="lane-line" />
							<text x="44" y={wave.high - 12} text-anchor="end" class="state-label">{tr('押下', 'ON')}</text>
							<text x="44" y={wave.low + 24} text-anchor="end" class="state-label">{tr('解放', 'OFF')}</text>
						{/if}
						{#each wave.segments as segment}
							{#if detailView}
								<line x1={segment.x} x2={segment.x + segment.width} y1={segment.pressed ? wave.high : wave.low} y2={segment.pressed ? wave.high : wave.low} class="detail-signal" />
							{:else}
							<line x1={segment.x} x2={segment.x + segment.width} y1={segment.pressed ? wave.high : wave.low} y2={segment.pressed ? wave.high : wave.low} class:signal-pressed={segment.pressed} class="overview-signal" />
							{/if}
						{/each}
						{#each wave.transitions as x}
							<line x1={x} x2={x} y1={wave.high} y2={wave.low} class:overview-transition={!detailView} class="wave-transition" />
						{/each}
						{#each wave.points as point (point.id)}
							{#if detailView}<line x1={point.x} x2={point.x} y1="50" y2="260" class="detail-chatter-guide" />{/if}
							<line x1={point.x} x2={point.x} y1={(point.pressed ? wave.high : wave.low) - 7} y2={(point.pressed ? wave.high : wave.low) + 7} class="chatter-marker" />
							<circle cx={point.x} cy={point.pressed ? wave.high : wave.low} r="5" class="chatter-marker-point" data-wave-marker-id={point.id}><title>{wave.label} · {point.phase === 'press' ? tr('押下時', 'On press') : tr('解放時', 'On release')} · {tr('チャタリング', 'Chatter')}</title></circle>
						{/each}
						{#if wave.selectedX !== undefined && !detailView}
							<line x1={wave.selectedX} x2={wave.selectedX} y1={detailView ? 20 : wave.high - 14} y2={detailView ? 285 : wave.low + 14} class="selected-wave-edge" data-testid="selected-wave-edge" />
						{/if}
						{#if detailView}{#each wave.edgeLabels as edge}
							<circle cx={edge.x} cy={edge.pressed ? wave.high : wave.low} r="6" class="detail-edge-hit"><title>{edge.pressed ? tr('押下', 'Press') : tr('解放', 'Release')}</title></circle>
						{/each}{/if}
					{/each}
					{#if detailView}{#each measurementPositions as x, index}
						{#if x >= 48 && x <= 694}
							<line x1={x} x2={x} y1="40" y2="280" class="measurement-edge" data-testid="measurement-edge" />
							<text x={Math.max(58, Math.min(684, x))} y="30" text-anchor="middle" class="measurement-number">{index + 1}</text>
							{/if}
					{/each}{/if}
					{#each displayedGaps as gap}
						<rect x={gap.x} y="10" width={gap.width} height={detailView ? 275 : 300} class="missing-wave" data-testid="wave-gap"><title>{tr('入力イベントを取得できなかった範囲', 'Range with missing input events')}</title></rect>
					{/each}
					{#if detailUnavailableX !== undefined}
						<rect x={detailUnavailableX} y="10" width={694 - detailUnavailableX} height="275" class="unavailable-wave" />
						{#if 694 - detailUnavailableX > 55}<text x={(detailUnavailableX + 694) / 2} y="270" text-anchor="middle" class="time-label">{tr('未取得', 'No data')}</text>{/if}
					{/if}
				</svg>
				<div class="wave-footer">
				{#if detailView}
					<div class="detail-info" data-testid="wave-detail-info"><span>{detailView.entry.pressed ? tr('押下', 'Press') : tr('解放', 'Release')}</span><span>{selectedLog!.phase === 'press' ? tr('押下時', 'On press') : tr('解放時', 'On release')}</span><span>{tr('間隔', 'Interval')} {(detailView.entry.interval / 10).toFixed(1)}ms</span><span>{tr('設定値', 'Setting')} {(selectedLog!.threshold / 10).toFixed(1)}ms</span><span class:detail-chatter={selectedLog!.chatter}>{selectedLog!.chatter ? tr('チャタリング検知', 'Chatter detected') : tr('通常入力', 'Normal input')}</span></div>
					<div class="measurement-info" aria-live="polite" data-testid="wave-measurement">
						{#if intervalMeasurement}
							<span>{tr('区間', 'Interval')} <b>{intervalMeasurement.total.toFixed(1)} ms</b></span>
							<span>{tr('押下', 'Pressed')} <b>{intervalMeasurement.pressed.toFixed(1)} ms</b></span>
							<span>{tr('解放', 'Released')} <b>{intervalMeasurement.released.toFixed(1)} ms</b></span>
						{:else}
							<span>{measurementEdges.length === 2 ? tr('欠落または設定変更により測定できません。選び直してください。', 'Cannot measure across missing data or changed edges. Select again.') : measurementEdges.length === 1 ? tr('2つ目のエッジを選択してください', 'Select the second edge') : tr('2つのエッジを選択して区間を測定', 'Select two edges to measure an interval')}</span>
						{/if}
						<button type="button" disabled={!measurementEdges.length} onclick={() => measurementEdges = []}>{tr('解除', 'Clear')}</button>
					</div>
				{:else}
				<div class="wave-history">
					<input type="range" min="0" max={maxBackMs} step="any" value={viewBackMs} oninput={moveWaveHistory} aria-label={tr('波形履歴', 'Waveform history')} title={tr('左へ動かすと保持中の入力ログまでさかのぼります。右端で最新の波形に戻ります。', 'Move left to review the retained input log. Move to the right edge to follow live input.')} data-testid="wave-history" />
				</div>
				<div class="summary-row">
					<div class="summary total"><span>{tr('総エッジ数', 'Total edges')}</span><strong>{totalEdges}</strong></div>
					<div class="summary chatter"><span>{tr('検知数', 'Detected')}</span><strong>{detectedCount}</strong></div>
					<div class="summary percent"><span>{tr('割合', 'Percent')}</span><strong>{detectionRate.toFixed(1)}%</strong></div>
				</div>
				{/if}
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
				<div class="log-header"><span>{tr('ボタン', 'Button')}</span><span>{tr('操作', 'Action')}</span><span title={tr('揺れを判定する操作の区分', 'Operation phase used for chatter detection')}>{tr('区分', 'Phase')}</span><span>{tr('間隔', 'Interval')}</span><span>{tr('設定値', 'Setting')}</span><span>{tr('判定', 'Result')}</span></div>
				<div class="log-scroll">
					<div class="log-list" bind:this={logViewport} role="region" aria-label={tr('入力ログ一覧', 'Input log entries')} onscroll={onLogScroll} data-testid="input-log">
						{#if visibleLog.length === 0}<p class="empty">{tr('ボタンを押すと入力履歴を表示します。', 'Press a button to see its input history.')}</p>{/if}
						<div aria-hidden="true" style:height={`${firstRenderedLog * logRowHeight}px`}></div>
						{#each renderedLog as entry (entry.id)}
							<div class="log-entry-row" class:chatter-entry={entry.chatter} class:selected-log-entry={entry.id === selectedLogId}>
								<button type="button" class="log-entry" aria-pressed={entry.id === selectedLogId} onclick={() => detailView ? showLogDetail(entry) : focusLog(entry)} data-testid="input-log-entry" data-log-id={entry.id}><b>{entry.button}</b><span>{entry.pressed ? tr('押下', 'Press') : tr('解放', 'Release')}</span><span>{entry.phase === 'press' ? tr('押下時', 'On press') : tr('解放時', 'On release')}</span><span class="log-measure"><span>{entry.interval >= 10000 ? '999.9' : (entry.interval / 10).toFixed(1)}</span><span>ms</span></span><span class="log-measure"><span>{(entry.threshold / 10).toFixed(1)}</span><span>ms</span></span><span>{entry.chatter ? tr('検知', 'Detected') : tr('通常', 'Normal')}</span></button>
							</div>
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
			<div class="toggle-row"><span class="toggle-label">{tr('自動設定', 'Auto setting')} <HelpText label={tr('自動設定', 'Auto setting')} side="left" sideOffset={8}><p>{tr('押下時・解放時の両方の揺れを検知すると、', 'When a bounce is found on press or release,')}</p><p>{tr('それぞれの操作開始から揺れまでの時間+0.1msに設定します。', 'set that phase to time since its start +0.1 ms.')}</p></HelpText></span><span class="flex shrink-0 items-center gap-2"><span aria-hidden="true" class="text-xs font-medium text-muted-foreground">{autoSetting ? 'ON' : 'OFF'}</span><UiSwitch aria-label={tr('自動設定', 'Auto setting')} bind:checked={autoSetting} /></span></div>
			<div class="toggle-row bulk-row"><span class="toggle-label">{tr('ボタン一括設定', 'Apply to all buttons')} <HelpText label={tr('ボタン一括設定', 'Apply to all buttons')} side="left" sideOffset={8}><p>{tr('ONにすると、選択中モードの値を', 'When on, changing a slider applies its value')}</p><p>{tr('B1〜B7へ一括適用します。', 'to B1–B7 in the selected mode.')}</p></HelpText></span><span class="flex shrink-0 items-center gap-2"><span aria-hidden="true" class="text-xs font-medium text-muted-foreground">{bulkSetting ? 'ON' : 'OFF'}</span><UiSwitch aria-label={tr('ボタン一括設定', 'Apply to all buttons')} checked={bulkSetting} onCheckedChange={setBulkSetting} /></span></div>
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

	.detail-waveform { border:1px solid var(--border); border-radius:6px; }
	.detail-plot-background { fill:var(--card); }
	.minor-grid-line { stroke:var(--muted-foreground); stroke-opacity:.09; stroke-width:1; }
	.detail-waveform .grid-line { stroke:var(--muted-foreground); stroke-opacity:.22; }
	.detail-waveform .lane-line { stroke:var(--muted-foreground); stroke-opacity:.2; stroke-dasharray:3 4; }
	.detail-signal { stroke:var(--primary); stroke-width:3; vector-effect:non-scaling-stroke; }
	.detail-waveform .wave-transition { stroke:var(--primary); stroke-width:3; vector-effect:non-scaling-stroke; }
	.detail-chatter-guide { stroke:#d97706; stroke-width:1.2; stroke-dasharray:4 4; opacity:.65; }
	.detail-waveform .chatter-marker { stroke:#d97706; }
	.detail-waveform .chatter-marker-point { fill:#d97706; stroke:var(--card); }
	.detail-waveform .selected-wave-edge { stroke:var(--primary); stroke-width:1.5; stroke-dasharray:5 4; }
	.state-label { fill:var(--foreground); font-size:15px; font-weight:600; }
	.measurement-range { fill:var(--primary); opacity:.09; pointer-events:none; }
	.measurement-edge { stroke:var(--primary); stroke-width:1.5; stroke-dasharray:4 3; pointer-events:none; vector-effect:non-scaling-stroke; }
	.measurement-number { fill:var(--primary); font-size:19px; font-weight:700; pointer-events:none; }
	.measurement-info { display:flex; flex-wrap:wrap; align-items:center; gap:4px 12px; padding:5px 7px; margin-top:5px; border:1px solid var(--border); border-radius:5px; font-size:.7rem; color:var(--muted-foreground); font-variant-numeric:tabular-nums; }
	.measurement-info b { color:var(--foreground); }
	.measurement-info button { margin-left:auto; border:1px solid var(--border); border-radius:4px; padding:1px 5px; background:var(--card); color:var(--foreground); cursor:pointer; }
	.measurement-info button:disabled { opacity:.4; cursor:default; }
	.detail-edge-hit { fill:transparent; cursor:pointer; }
	.wave-footer { flex:0 0 72px; height:72px; display:flex; flex-direction:column; justify-content:space-between; gap:3px; padding-top:3px; }
	.wave-legend .legend-press::before { background:var(--primary); }
	.wave-detail-panel .legend-release::before { background:var(--primary); }
	.wave-legend .legend-chatter::before { background:#d97706; }
	.chatter-test { display:grid; grid-template-columns:minmax(0,1.25fr) minmax(280px,.75fr); gap:10px; align-items:stretch; height:100%; min-height:0; }
	.left-column { display:grid; grid-template-rows:minmax(0,1fr) 264px; gap:4px; min-width:0; min-height:0; }
	.panel { min-width:0; border:1px solid var(--border); border-radius:12px; background:var(--card); padding:10px; }
	.panel h2 { margin:0; font-size:1.02rem; font-weight:700; }
	.wave-legend { display:flex; flex-wrap:wrap; gap:12px; margin:8px 0 5px; color:var(--muted-foreground); font-size:.7rem; }
	.wave-legend span { display:inline-flex; align-items:center; gap:5px; }
	.wave-legend span::before { content:""; display:inline-block; width:9px; height:9px; border-radius:2px; background:#22d3ee; }
	.wave-legend .legend-release::before { height:4px; background:#64748b; }
	.wave-legend .legend-chatter::before { border-radius:50%; background:#c2410c; }
	.wave-legend .wave-note::before { display:none; }
	.missing-note { color:#c2410c; }
	.missing-wave { fill:var(--card); stroke:#c2410c; stroke-width:1; stroke-dasharray:4 3; }
	.waveform-panel { display:flex; flex-direction:column; min-height:0; }
	.waveform { cursor:grab; touch-action:none; user-select:none; display:block; flex:1 1 auto; min-height:150px; width:100%; height:auto; overflow:visible; }
	.grid-line { stroke:rgba(148,163,184,.2); stroke-width:1; stroke-dasharray:3 4; }
	.lane-background { fill:rgba(148,163,184,.055); }
	.lane-background.lane-active { fill:rgba(34,211,238,.095); }
	.lane-line { stroke:rgba(148,163,184,.24); stroke-width:1; }
	.status-dot { fill:#475569; }
	.status-dot.status-active { fill:#22d3ee; }
	.wave-label { fill:#64748b; font-size:22px; font-weight:700; }
	.wave-label.label-active { fill:#0369a1; }
	:global(.dark) .wave-label { fill:#cbd5e1; }
	:global(.dark) .wave-label.label-active { fill:#67e8f9; }
	.overview-signal { stroke:var(--muted-foreground); stroke-width:1.5; vector-effect:non-scaling-stroke; }
	.overview-signal.signal-pressed { stroke:var(--primary); }
	.wave-transition.overview-transition { stroke:var(--primary); stroke-width:1.5; vector-effect:non-scaling-stroke; }
	.lane-divider { stroke:var(--border); stroke-width:1; stroke-opacity:.55; }
	.wave-transition { stroke:#22d3ee; stroke-width:3; }
	.chatter-marker { stroke:#c2410c; stroke-width:4; }
	.chatter-marker-point { fill:#c2410c; stroke:#0f172a; stroke-width:1.4; }
	.selected-wave-edge { stroke:#facc15; stroke-width:3; stroke-dasharray:4 3; }
	.time-label { fill:var(--muted-foreground); font-size:18px; }
	.unavailable-wave { fill:var(--card); opacity:.9; }
	.wave-heading { flex-wrap:nowrap; gap:4px; }
	.wave-title { display:flex; align-items:center; gap:6px; min-width:0; overflow:hidden; }
	.wave-title h2 { overflow:hidden; white-space:nowrap; text-overflow:ellipsis; }
	.wave-actions { display:flex; align-items:center; gap:6px; margin-left:auto; flex-shrink:0; }
	.wave-mode { display:flex; gap:4px; }
	.wave-mode button { border:1px solid var(--border); border-radius:5px; padding:3px 6px; background:var(--card); color:var(--foreground); font-size:.7rem; cursor:pointer; }
	.wave-mode button.wave-mode-selected { border-color:#0369a1; background:#0369a1; color:#fff; }
	:global(.dark) .wave-mode button.wave-mode-selected { border-color:#22d3ee; background:#0e7490; }
	.wave-detail-button { flex-shrink:0; border:1px solid var(--border); border-radius:5px; padding:3px 8px; font-size:.68rem; color:var(--foreground); background:var(--card); cursor:pointer; }
	.wave-detail-button:disabled { opacity:.45; cursor:default; }
	.wave-detail-button:not(:disabled):hover { border-color:#0284c7; }
	.detail-info { display:flex; flex-wrap:wrap; gap:2px 8px; margin-top:3px; font-size:.65rem; color:var(--muted-foreground); font-variant-numeric:tabular-nums; }
	.detail-chatter { color:#c2410c; font-weight:700; }
	.wave-history { margin:-2px 2px 6px 48px; height:18px; display:flex; align-items:center; }
	.wave-history input { width:100%; margin:0; cursor:pointer; accent-color:#22d3ee; direction:rtl; }
	.summary-row { display:grid; grid-template-columns:repeat(3,minmax(0,1fr)); gap:6px; }
	.summary { display:grid; grid-template-columns:minmax(0,1fr) auto; align-items:center; gap:4px; min-width:0; overflow:hidden; border:1px solid #cbd5e1; border-radius:9px; padding:6px 7px; background:#ffffff; }
	.summary span { min-width:0; color:#475569; font-size:.69rem; white-space:nowrap; }
	.summary strong { min-width:0; max-width:100%; font-size:1rem; color:#0284c7; font-variant-numeric:tabular-nums; white-space:nowrap; text-align:right; }
	.summary.chatter strong { color:#c2410c; }
	.summary.percent strong { color:#c2410c; font-size:.92rem; }
	:global(.dark) .summary { border-color:rgba(255,255,255,.12); background:rgba(15,23,42,.35); }
	:global(.dark) .summary span { color:var(--muted-foreground); }
	:global(.dark) .summary strong { color:#22d3ee; }
	:global(.dark) .summary.chatter strong,
	:global(.dark) .summary.percent strong { color:#c2410c; }
	.panel-heading { display:flex; justify-content:space-between; align-items:center; gap:10px; }
	.log-heading-actions { display:flex; align-items:center; gap:8px; }
	.log-filter { display:flex; gap:3px; }
	.log-filter button { border:1px solid #cbd5e1; border-radius:5px; padding:2px 7px; color:#0f172a; font-size:.68rem; line-height:1.35; background:#ffffff; opacity:1; transition:background .15s ease,border-color .15s ease,color .15s ease; }
	.log-filter button:hover { border-color:#0284c7; background:#f8fafc; }
	.log-filter button.log-filter-selected { border-color:#0369a1; background:#0369a1; color:white; font-weight:700; box-shadow:0 0 0 1px rgba(2,132,199,.2) inset; }
	.panel-heading .reset-log { color:#0369a1; font-size:.8rem; text-decoration:underline; }
	:global(.dark) .panel-heading .reset-log { color:#93c5fd; }
	.log-scroll { display:grid; grid-template-columns:minmax(0,1fr) 10px; gap:4px; height:190px; min-height:190px; }
	.log-list { min-width:0; min-height:0; overflow-y:auto; overflow-x:hidden; scrollbar-width:none; }
	.log-list::-webkit-scrollbar { display:none; }
	.log-scrollbar { position:relative; width:10px; height:100%; padding:0; overflow:hidden; border:0; border-radius:8px; background:rgba(100,116,139,.25); touch-action:none; cursor:pointer; }
	.log-scrollbar-thumb { position:absolute; top:0; left:1px; right:1px; display:block; max-height:100%; border-radius:7px; background:#64748b; }
	.log-header,.log-entry { display:grid; grid-template-columns:36px 30px 40px repeat(2,minmax(52px,1fr)) 30px; gap:3px; align-items:center; }
	.log-header { margin-right:14px; padding:7px 2px; color:var(--muted-foreground); font-size:.73rem; }
	.log-entry-row { display:grid; grid-template-columns:minmax(0,1fr); align-items:center; height:26px; box-sizing:border-box; border-top:1px solid var(--border); padding:2px; }
	.log-entry { width:100%; height:100%; min-width:0; box-sizing:border-box; border:0; padding:0; background:transparent; color:inherit; font-size:.68rem; line-height:1.2; text-align:left; cursor:pointer; }
	.log-entry-row:hover,.log-entry-row.selected-log-entry { background:rgba(250,204,21,.13); }
	.log-entry-row.selected-log-entry { box-shadow:inset 3px 0 #facc15; }
	.log-header span,.log-entry span { min-width:0; white-space:nowrap; }
	.log-measure { display:grid; grid-template-columns:5ch auto; column-gap:1px; align-items:baseline; font-variant-numeric:tabular-nums; }
	.log-measure span:first-child { text-align:right; }
	.log-entry-row.chatter-entry { color:#c2410c; background:rgba(249,115,22,.08); }
	.empty { color:var(--muted-foreground); }
	.empty { padding:14px 2px; font-size:.8rem; }
	.settings-panel { display:flex; flex-direction:column; min-height:0; }
	.toggle-row { display:flex; align-items:center; justify-content:space-between; gap:10px; font-weight:650; }
	.toggle-label { display:inline-flex; align-items:center; gap:8px; }
	.bulk-row { border-top:1px solid var(--border); padding-top:.7rem; }
	.button-sliders { display:grid; grid-template-rows:repeat(7,minmax(0,1fr)); flex:1 1 auto; min-height:0; margin-top:.15rem; }
	.mode-row { display:grid; grid-template-columns:repeat(2,minmax(0,1fr)); align-items:center; gap:4px; margin:.35rem 0 .25rem; padding:4px; border:1px solid var(--border); border-radius:9px; background:#f8fafc; }
	.mode-row button { border:1px solid #cbd5e1; border-radius:6px; padding:.45rem .3rem; color:#0f172a; font-family:inherit; font-weight:600; font-size:.8rem; background:#ffffff; opacity:1; transition:background .15s ease,border-color .15s ease,color .15s ease; }
	.mode-row button:hover { border-color:#0284c7; background:color-mix(in srgb,#0ea5e9 10%,var(--card)); }
	.mode-row button.mode-selected { border-color:#0369a1; background:#0369a1; color:white; font-weight:600; box-shadow:0 0 0 1px rgba(2,132,199,.2) inset; }
	:global(.dark) .log-filter button { color:var(--muted-foreground); background:rgba(15,23,42,.62); opacity:.72; }
	:global(.dark) .log-filter button:hover { border-color:var(--muted-foreground); color:var(--foreground); opacity:1; }
	:global(.dark) .log-filter button.log-filter-selected { border-color:#22d3ee; background:#0e7490; color:white; opacity:1; box-shadow:0 0 0 1px rgba(34,211,238,.22) inset; }
	:global(.dark) .mode-row { background:rgba(15,23,42,.35); }
	:global(.dark) .mode-row button { color:var(--muted-foreground); background:rgba(15,23,42,.62); opacity:.72; }
	:global(.dark) .mode-row button:hover { border-color:var(--muted-foreground); color:var(--foreground); opacity:1; }
	:global(.dark) .mode-row button.mode-selected { border-color:#22d3ee; background:#0e7490; color:white; opacity:1; box-shadow:0 0 0 1px rgba(34,211,238,.22) inset; }
	.mode-description { margin:0 2px .65rem; color:var(--muted-foreground); font-size:.64rem; letter-spacing:-.04em; line-height:1.5; white-space:nowrap; }
	.slider-row { display:grid; grid-template-columns:42px minmax(0,1fr); align-items:center; gap:6px; min-height:0; padding:.35rem 0; border-top:1px solid var(--border); }
	.button-controls { min-width:0; }
	.button-metrics { display:grid; grid-template-columns:repeat(3,minmax(0,1fr)); gap:3px; margin-bottom:2px; text-align:center; font-variant-numeric:tabular-nums; }
	.button-metrics span { display:flex; flex-direction:column; min-width:0; line-height:1.1; }
	.button-metrics small { color:var(--muted-foreground); font-size:.59rem; white-space:nowrap; }
	.button-metrics b { font-size:.7rem; white-space:nowrap; }
	.button-label { display:grid; justify-items:center; gap:1px; line-height:1.05; }
	.button-label small { width:max-content; color:var(--muted-foreground); font-size:.65rem; }
	.button-label small.pressed { color:#22d3ee; }
	@media(max-height:740px) { .panel { padding:8px; } .wave-legend { margin:5px 0 3px; } .wave-history { margin-bottom:3px; } .log-header { padding:4px 2px; } .left-column { grid-template-rows:minmax(0,1fr) 264px; } .slider-row { padding:0; } .mode-description { margin-bottom:.15rem; line-height:1.2; } .mode-row { margin:.2rem 0; padding:2px; } .mode-row button { padding:.3rem .2rem; } .bulk-row { padding-top:.35rem; } }
	.waveform.wave-dragging { cursor:grabbing; }
	@media(max-height:650px) { .waveform { min-height:85px; } }
	@media(max-width:850px) {
		.chatter-test { grid-template-columns:1fr; height:auto; flex:0 0 auto; }
		.left-column { grid-template-rows:minmax(280px,auto) auto; }
		.settings-panel { order:-1; }
		.button-sliders { flex:0 0 auto; grid-template-rows:repeat(7,minmax(58px,auto)); }
		.waveform { flex:none; height:240px; }
	}
</style>
