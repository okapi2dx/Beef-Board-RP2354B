function waitForHidDevice(ses, callback, isAllowed, timeoutMs = 1700) {
  let settled = false;
  const finish = id => {
    if (settled) return;
    settled = true;
    clearTimeout(timer);
    ses.removeListener('hid-device-added', onAdded);
    callback(id);
  };
  const onAdded = (_event, device) => {
    if (device.deviceId && isAllowed(device)) finish(device.deviceId);
  };
  ses.on('hid-device-added', onAdded);
  const timer = setTimeout(() => finish(''), timeoutMs);
}

module.exports = { waitForHidDevice };
