const RELEASE_API = 'https://api.github.com/repos/okapi2dx/Beef-Board-RP2354B/releases/latest';
function parseVersion(tag) {
  const match = /^v?(\d+(?:\.\d+)*)$/i.exec(String(tag || '').trim());
  return match ? match[1].split('.').map(Number) : null;
}

function compareVersions(left, right) {
  const a = parseVersion(left);
  const b = parseVersion(right);
  if (!a || !b) return 0;
  const length = Math.max(a.length, b.length);
  for (let i = 0; i < length; i += 1) {
    const difference = (a[i] || 0) - (b[i] || 0);
    if (difference !== 0) return Math.sign(difference);
  }
  return 0;
}

async function checkForUpdate(fetchImpl, currentVersion) {
  if (!parseVersion(currentVersion)) throw new Error('Current app version is invalid');
  const response = await fetchImpl(RELEASE_API, {
    headers: {
      Accept: 'application/vnd.github+json',
      'X-GitHub-Api-Version': '2022-11-28',
      'User-Agent': 'Beef-Board-Configurator'
    }
  });
  if (!response.ok) throw new Error(`GitHub release check failed (${response.status})`);

  const release = await response.json();
  const latestVersion = String(release.tag_name || '');
  const releaseUrl = String(release.html_url || '');
  if (!parseVersion(latestVersion) || !releaseUrl.startsWith('https://github.com/')) {
    throw new Error('GitHub release response is invalid');
  }

  return {
    currentVersion,
    latestVersion,
    releaseUrl,
    updateAvailable: compareVersions(latestVersion, currentVersion) > 0
  };
}

const FIRMWARE_ASSETS = {
  rp2354b: 'Beef-Board-RP2354B.uf2',
  at90: 'Beef-Board-AT90USB1286.hex'
};
async function checkForFirmwareUpdate(fetchImpl, board, currentVersion) {
  if (!Object.hasOwn(FIRMWARE_ASSETS, board) || !parseVersion(currentVersion)) throw new Error('Invalid firmware update request');
  const response = await fetchImpl(RELEASE_API, { signal: AbortSignal.timeout(10000), headers: { Accept: 'application/vnd.github+json', 'User-Agent': 'Beef-Board-Configurator' } });
  if (!response.ok) throw new Error(`Firmware release check failed (${response.status})`);
  const release = await response.json();
  const releaseUrl = String(release.html_url || '');
  const prefix = 'https://github.com/okapi2dx/Beef-Board-RP2354B/';
  if (!releaseUrl.startsWith(prefix + 'releases/tag/')) throw new Error('Invalid firmware release URL');
  const assets = Array.isArray(release.assets) ? release.assets : [];
  const metadata = assets.find(asset => asset.name === 'firmware-versions.json');
  if (!metadata) return null;
  if (typeof metadata.browser_download_url !== 'string' || !metadata.browser_download_url.startsWith(prefix + 'releases/download/')) throw new Error('Invalid firmware metadata URL');
  const manifestResponse = await fetchImpl(metadata.browser_download_url, { signal: AbortSignal.timeout(10000) });
  if (!manifestResponse.ok) throw new Error('Firmware metadata download failed');
  const manifest = await manifestResponse.json();
  if (manifest.schemaVersion !== 1) throw new Error('Unsupported firmware metadata');
  const entry = manifest.firmwares?.[board];
  if (!entry) return null;
  if (!parseVersion(entry.version) || entry.asset !== FIRMWARE_ASSETS[board]) throw new Error('Invalid firmware metadata');
  if (!assets.some(asset => asset.name === entry.asset && asset.state === 'uploaded')) return null;
  return { board, currentVersion, latestVersion: entry.version, releaseUrl, updateAvailable: compareVersions(entry.version, currentVersion) > 0 };
}
module.exports = { RELEASE_API, parseVersion, compareVersions, checkForUpdate, checkForFirmwareUpdate };
