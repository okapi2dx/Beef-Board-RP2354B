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

module.exports = { RELEASE_API, parseVersion, compareVersions, checkForUpdate };
