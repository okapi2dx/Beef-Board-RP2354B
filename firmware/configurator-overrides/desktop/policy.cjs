const ORIGIN = 'https://beef.local';
function trusted(url) {
  try { return new URL(url).origin === ORIGIN; } catch { return false; }
}
function allowedDevice(type, device) {
  if (!device) return false;
  if (type === 'hid') {
    const name = device.name ?? device.productName;
    if (device.vendorId === 0xfeed && device.productId === 0x0000) return name === 'BEEF BOARD';
    if (device.vendorId === 0xfeed && device.productId === 0x0003) return ['BEEF BOARD RP2354B', 'Beef Board RP2354B'].includes(name);
    if (device.vendorId !== 0x1ccf) return false;
    if (device.productId === 0x1018) return ['BEEF BOARD', 'beatmania IIDX controller entry model'].includes(name);
    if (device.productId === 0x8048) return ['BEEF BOARD', 'beatmania IIDX controller premium model'].includes(name);
    return false;
  }
  if (type === 'usb') {
    return (device.vendorId === 0x03eb && device.productId === 0x2ffb) ||
      (device.vendorId === 0x0483 && device.productId === 0xdf11);
  }
  return false;
}
module.exports = { ORIGIN, trusted, allowedDevice };
