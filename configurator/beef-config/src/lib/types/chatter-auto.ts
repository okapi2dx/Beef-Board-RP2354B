// Extend a phase's lockout through the recovery edge as well as the bounce.
// Otherwise the lockout can expire while the raw button is still reversed.
export class ChatterAutoTuner {
	private bounced = Array<boolean>(7).fill(false);
	private intervals = Array<number>(7).fill(0);

	reset(): void {
		this.bounced.fill(false);
		this.intervals.fill(0);
	}

	observe(button: number, phaseStart: boolean, candidate: boolean,
		elapsed: number | undefined, currentUnits: number): number | undefined {
		if (phaseStart) {
			this.bounced[button] = false;
			this.intervals[button] = 0;
		} else if (elapsed !== undefined) {
			++this.intervals[button];
		}
		if (candidate) this.bounced[button] = true;
		if (!this.bounced[button] || elapsed === undefined) return undefined;
		// Each firmware interval is truncated to 100 us. The sum can lose
		// nearly one tick per interval, including the return edge.
		return Math.min(160, Math.max(currentUnits, elapsed + Math.max(1, this.intervals[button])));
	}
}
