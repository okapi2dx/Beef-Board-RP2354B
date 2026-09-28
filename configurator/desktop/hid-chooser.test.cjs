const { test } = require('node:test');
const assert = require('node:assert/strict');
const { EventEmitter } = require('node:events');
const { waitForHidDevice } = require('./hid-chooser.cjs');

test('HID chooser selects a supported device that appears after the request', async () => {
  const ses = new EventEmitter();
  const chosen = new Promise(resolve => waitForHidDevice(ses, resolve, device => device.vendorId === 0x1ccf, 50));
  ses.emit('hid-device-added', {}, { deviceId: 'unrelated', vendorId: 0x1234 });
  ses.emit('hid-device-added', {}, { deviceId: 'entry', vendorId: 0x1ccf });
  assert.equal(await chosen, 'entry');
  assert.equal(ses.listenerCount('hid-device-added'), 0);
});

test('HID chooser releases a request when the new device never appears', async () => {
  const ses = new EventEmitter();
  const chosen = new Promise(resolve => waitForHidDevice(ses, resolve, () => true, 5));
  assert.equal(await chosen, '');
  assert.equal(ses.listenerCount('hid-device-added'), 0);
});
