export type WaveSample = { timestamp: number; buttonIndex: number; pressed: boolean };
export type WaveGap = { start: number; end: number };

// First press/release is immediate; only subsequent reverse edges are locked
// out. A raw state that persists until expiry is adopted at expiry.
export function replayDebounce(samples: readonly WaveSample[], pressUnits: readonly number[],
	releaseUnits: readonly number[], gaps: readonly WaveGap[] = []): WaveSample[] {
	const result: WaveSample[] = [];
	for (let buttonIndex = 0; buttonIndex < 7; buttonIndex++) {
		const edges = samples.filter((sample) => sample.buttonIndex === buttonIndex).sort((a, b) => a.timestamp - b.timestamp);
		if (!edges.length) continue;
		const origin = edges[0].timestamp;
		const toTicks = (timestamp: number) => Math.round((timestamp - origin) * 10);
		let raw = edges[0].pressed;
		let accepted = raw;
		let guardUntil = -Infinity;
		let guardAt = -Infinity;
		let previousTime = origin;
		result.push({ timestamp: origin, buttonIndex, pressed: accepted });
		const emit = (timestamp: number, ticks: number) => {
			accepted = raw;
			result.push({ timestamp, buttonIndex, pressed: accepted });
			const units = (accepted ? pressUnits : releaseUnits)[buttonIndex] ?? 0;
			guardUntil = ticks + units;
			guardAt = timestamp + units / 10;
		};
		for (const edge of edges.slice(1)) {
			if (gaps.some((gap) => gap.end >= previousTime && gap.start <= edge.timestamp)) {
				guardUntil = guardAt = -Infinity;
				raw = accepted = edge.pressed;
				result.push({ ...edge });
				previousTime = edge.timestamp;
				continue;
			}
			const time = toTicks(edge.timestamp);
			if (raw !== accepted && guardUntil < time) emit(Math.min(guardAt, edge.timestamp), guardUntil);
			raw = edge.pressed;
			if (raw !== accepted && time >= guardUntil) emit(edge.timestamp, time);
			previousTime = edge.timestamp;
		}
		if (raw !== accepted && !gaps.some((gap) => gap.end >= previousTime && gap.start <= guardAt)) emit(guardAt, guardUntil);
	}
	return result.sort((a, b) => a.timestamp - b.timestamp);
}
