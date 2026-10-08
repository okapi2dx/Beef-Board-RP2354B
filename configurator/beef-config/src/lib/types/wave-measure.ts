export function measureWaveInterval(
  samples: { timestamp: number; pressed: boolean }[],
  first: number,
  second: number,
  gaps: { start: number; end: number }[] = []
): { total: number; pressed: number; released: number } | undefined {
  const start = Math.min(first, second);
  const end = Math.max(first, second);
  if (!Number.isFinite(start) || !Number.isFinite(end) || start === end ||
      gaps.some(gap => gap.start < end && gap.end > start)) return undefined;
  const edges = samples.filter((sample, index) => index > 0 && sample.pressed !== samples[index - 1].pressed);
  const firstEdge = edges.find(edge => Math.abs(edge.timestamp - start) < 1e-7);
  if (!firstEdge || !edges.some(edge => Math.abs(edge.timestamp - end) < 1e-7)) return undefined;
  let state = firstEdge.pressed;
  let cursor = start;
  let pressed = 0;
  for (const edge of edges.filter(edge => edge.timestamp > start && edge.timestamp < end)) {
    if (state) pressed += edge.timestamp - cursor;
    cursor = edge.timestamp;
    state = edge.pressed;
  }
  if (state) pressed += end - cursor;
  return { total: end - start, pressed, released: end - start - pressed };
}
