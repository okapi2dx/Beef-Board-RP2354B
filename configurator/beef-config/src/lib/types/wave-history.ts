// Waveform time advances while a button is held or briefly after an edge.
// Long periods with every button released occupy no space in the history.
export class WaveClock {
	private lastReal: number;
	private lastActivity = -Infinity;
	private buttons = 0;
	private readonly tailMs: number;
	virtualNow = 0;

	constructor(realNow: number, tailMs = 250) {
		this.lastReal = realNow;
		this.tailMs = tailMs;
	}

	advance(realNow: number): number {
		if (realNow <= this.lastReal) return this.virtualNow;
		const activeEnd = this.buttons ? realNow : Math.min(realNow, this.lastActivity + this.tailMs);
		this.virtualNow += Math.max(0, activeEnd - this.lastReal);
		this.lastReal = realNow;
		return this.virtualNow;
	}

	record(realNow: number, buttons: number, hasEdge: boolean): number {
		this.advance(realNow);
		this.buttons = buttons;
		if (hasEdge) this.lastActivity = realNow;
		return this.virtualNow;
	}
}
