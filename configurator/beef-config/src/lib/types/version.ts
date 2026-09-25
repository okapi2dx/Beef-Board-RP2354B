export function formatDisplayVersion(version: string): string {
  const match = /^v?(\d+)\.(\d+)(?:\.(\d+))?$/i.exec(version.trim());
  if (!match) return version;

  const [, major, minor, patch = '0'] = match;
  const base = `V${major}.${minor.padStart(2, '0')}`;
  return Number(patch) === 0 ? base : `${base}.${Number(patch)}`;
}
