const { test } = require('node:test');
const assert = require('node:assert/strict');
const { RELEASE_API, parseVersion, compareVersions, checkForUpdate } = require('./update-check.cjs');

test('update checker targets the Configurator repository', () => {
  assert.equal(RELEASE_API, 'https://api.github.com/repos/okapi2dx/Beef-Board-Configurator/releases/latest');
});

test('version comparison accepts the V1.00 release naming scheme', () => {
  assert.equal(compareVersions('V1.01', '1.0.0'), 1);
  assert.equal(compareVersions('V1.00', '1.0.0'), 0);
  assert.equal(compareVersions('V1.00.1', '1.0.0'), 1);
  assert.deepEqual(parseVersion('V1.00'), [1, 0]);
});

test('update result uses the running app version', async () => {
  const result = await checkForUpdate(async () => ({
    ok: true,
    json: async () => ({
      tag_name: 'V1.01',
      html_url: 'https://github.com/okapi2dx/Beef-Board-Configurator/releases/tag/V1.01'
    })
  }), '1.0.0');
  assert.deepEqual(result, {
    currentVersion: '1.0.0',
    latestVersion: 'V1.01',
    releaseUrl: 'https://github.com/okapi2dx/Beef-Board-Configurator/releases/tag/V1.01',
    updateAvailable: true
  });
});
