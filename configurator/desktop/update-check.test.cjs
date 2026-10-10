const { test } = require('node:test');
const assert = require('node:assert/strict');
const { RELEASE_API, parseVersion, compareVersions, checkForUpdate } = require('./update-check.cjs');

test('update checker targets the RP2354B release repository', () => {
  assert.equal(RELEASE_API, 'https://api.github.com/repos/okapi2dx/Beef-Board-RP2354B/releases/latest');
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
      html_url: 'https://github.com/okapi2dx/Beef-Board-RP2354B/releases/tag/V1.01'
    })
  }), '1.0.0');
  assert.deepEqual(result, {
    currentVersion: '1.0.0',
    latestVersion: 'V1.01',
    releaseUrl: 'https://github.com/okapi2dx/Beef-Board-RP2354B/releases/tag/V1.01',
    updateAvailable: true
  });
});

const { checkForFirmwareUpdate } = require('./update-check.cjs');
const releaseUrl = 'https://github.com/okapi2dx/Beef-Board-RP2354B/releases/tag/V2.00';
function firmwareFetch(versions = {rp2354b: '1.1.0', at90: '1.0.0'}, options = {}) {
  const names = {rp2354b: 'Beef-Board-RP2354B.uf2', at90: 'Beef-Board-AT90USB1286.hex'};
  const assets = Object.values(names).filter(n => n !== options.missing).map(name => ({name, state:'uploaded'}));
  if (!options.noManifest) assets.push({name:'firmware-versions.json',browser_download_url:options.url || 'https://github.com/okapi2dx/Beef-Board-RP2354B/releases/download/V2.00/firmware-versions.json'});
  return async url => ({ok:true,json:async()=>url===RELEASE_API ? {tag_name:'V2.00',html_url:releaseUrl,assets} : {schemaVersion:1,firmwares:Object.fromEntries(Object.entries(versions).map(([board,version])=>[board,{version,asset:names[board]}]))}});
}
test('firmware versions are independent of application tag and board', async () => {
  assert.equal((await checkForFirmwareUpdate(firmwareFetch(), 'rp2354b','1.0.0')).updateAvailable,true);
  assert.equal((await checkForFirmwareUpdate(firmwareFetch(), 'at90','1.0.0')).updateAvailable,false);
  assert.equal((await checkForFirmwareUpdate(firmwareFetch(), 'rp2354b','1.2.0')).updateAvailable,false);
  assert.equal((await checkForFirmwareUpdate(firmwareFetch(), 'rp2354b','1.1.0')).updateAvailable,false);
});
test('firmware does not notify without matching metadata and available binary', async () => {
  assert.equal(await checkForFirmwareUpdate(firmwareFetch({}, {noManifest:true}), 'rp2354b','1.0.0'),null);
  assert.equal(await checkForFirmwareUpdate(firmwareFetch(), 'at90','invalid').catch(()=>null),null);
  assert.equal(await checkForFirmwareUpdate(firmwareFetch({},{}), 'at90','1.0.0'),null);
  assert.equal(await checkForFirmwareUpdate(firmwareFetch(undefined,{missing:'Beef-Board-RP2354B.uf2'}), 'rp2354b','1.0.0'),null);
});
test('firmware metadata rejects unexpected URLs, unsupported boards and invalid versions', async () => {
  await assert.rejects(checkForFirmwareUpdate(firmwareFetch(undefined,{url:'https://example.com/firmware.json'}),'rp2354b','1.0.0'));
  await assert.rejects(checkForFirmwareUpdate(firmwareFetch(), 'unknown','1.0.0'));
  await assert.rejects(checkForFirmwareUpdate(firmwareFetch({rp2354b:'invalid'}),'rp2354b','1.0.0'));
});
