const { test } = require('node:test');
const assert = require('node:assert/strict');
const { trusted, allowedDevice } = require('./policy.cjs');
test('only bundled origin is trusted', () => {
  assert.equal(trusted('https://beef.local/'), true);
  for (const url of ['https://beef.local.evil.test', 'http://beef.local', 'file:///test', 'garbage']) assert.equal(trusted(url), false);
});
test('only supported Beef HID and exact DFU IDs are accepted', () => {
  assert.equal(allowedDevice('hid', { vendorId: 0xfeed, productId: 0x0000, name: 'BEEF BOARD' }), true);
  assert.equal(allowedDevice('hid', { vendorId: 0xfeed, productId: 0x0001, productName: 'BEEF BOARD' }), false);
  assert.equal(allowedDevice('hid', { vendorId: 0xfeed, productId: 0x0002, productName: 'BEEF BOARD' }), false);
  assert.equal(allowedDevice('hid', { vendorId: 0xfeed, productId: 0x0003, productName: 'BEEF BOARD RP2354B' }), true);
  for (const productId of [0x1018, 0x8048]) assert.equal(allowedDevice('hid', { vendorId: 0x1ccf, productId, productName: 'BEEF BOARD' }), true);
  assert.equal(allowedDevice('hid', { vendorId: 0x1ccf, productId: 0x1018, productName: 'beatmania IIDX controller entry model' }), true);
  assert.equal(allowedDevice('hid', { vendorId: 0x1ccf, productId: 0x1018, name: 'USB input device', productName: 'BEEF BOARD' }), true);
  assert.equal(allowedDevice('hid', { vendorId: 0x1ccf, productId: 0x8048, productName: 'beatmania IIDX controller premium model' }), true);
  assert.equal(allowedDevice('hid', { vendorId: 0x1ccf, productId: 0x101c, productName: 'BEEF BOARD' }), false);
  assert.equal(allowedDevice('hid', { vendorId: 0x1ccf, productId: 0x8048, productName: 'KONAMI' }), false);
  assert.equal(allowedDevice('usb', { vendorId: 0x03eb, productId: 0x2ffb }), true);
  assert.equal(allowedDevice('usb', { vendorId: 0x0483, productId: 0xdf11 }), false);
  assert.equal(allowedDevice('usb', { vendorId: 0x03eb, productId: 0x2ff4 }), false);
  assert.equal(allowedDevice('usb', { vendorId: 0x0483, productId: 0xdf12 }), false);
  assert.equal(allowedDevice('hid', null), false);
});
