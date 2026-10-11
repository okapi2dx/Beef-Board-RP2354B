// Extend a phase's lockout through the recovery edge as well as the bounce.
// Otherwise the lockout can expire while the raw button is still reversed.
export class ChatterAutoTuner {
	private bounced = Array<boolean>(7).fill(false);

	reset(): void {
		this.bounced.fill(false);
	}

	observe(button: number, phaseStart: boolean, candidate: boolean,
		elapsed: number | undefined, currentUnits: number): number | undefined {
		if (phaseStart) this.bounced[button] = false;
		if (candidate) this.bounced[button] = true;
		if (!this.bounced[button] || elapsed === undefined) return undefined;
		return Math.min(160, Math.max(currentUnits, elapsed + 1));
	}
}
