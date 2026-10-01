// Firmware indexes its six-entry ring by the wrapping 16-bit sequence.
// Around wrap, older entries can share a slot with a newer entry. Never
// interpret the overwritten slot twice or invent an operation start.
export function diagnosticEventSlots(previous: number, current: number) {
  const count = (current - previous + 65536) % 65536;
  const seen = new Set<number>();
  const slots: number[] = [];
  let lost = count > 6;
  for (let offset = 0; offset < Math.min(count, 6); offset++) {
    const sequence = (current - offset + 65536) % 65536;
    const slot = (sequence % 6) * 3;
    if (seen.has(slot)) { lost = true; continue; }
    seen.add(slot);
    slots.push(slot);
  }
  return { slots: slots.reverse(), lost };
}
