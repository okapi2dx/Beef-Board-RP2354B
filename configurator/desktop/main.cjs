const { app, BrowserWindow, session, dialog, shell, net, ipcMain } = require('electron');
const path = require('node:path');
const fs = require('node:fs');
const os = require('node:os');
const { pathToFileURL } = require('node:url');
const { ORIGIN, trusted, allowedDevice } = require('./policy.cjs');
const { waitForHidDevice } = require('./hid-chooser.cjs');
const { createFlasher } = require('./native-flash.cjs');
const { checkForUpdate, checkForFirmwareUpdate } = require('./update-check.cjs');

const startupLogPath = path.join(os.tmpdir(), 'Beef Board Configurator-startup.log');
function startupLog(message) {
  try {
    fs.appendFileSync(startupLogPath, `[${new Date().toISOString()}] ${message}\n`, 'utf8');
  } catch { /* Logging must never block startup. */ }
}
startupLog(`process start pid=${process.pid} packaged=${app.isPackaged} exec=${process.execPath}`);
process.on('uncaughtException', error => {
  startupLog(`uncaughtException: ${error?.stack || error}`);
  try { dialog.showErrorBox('起動エラー', String(error)); } catch { /* best effort */ }
  app.quit();
});
process.on('unhandledRejection', error => {
  startupLog(`unhandledRejection: ${error?.stack || error}`);
});
process.on('exit', code => startupLog(`process exit code=${code}`));

let window;
let activateWhenReady = false;
const sleep = ms => new Promise(resolve => setTimeout(resolve, ms));

function activateMainWindow() {
  if (!window || window.isDestroyed()) {
    activateWhenReady = true;
    startupLog('window activation queued');
    return;
  }
  activateWhenReady = false;
  if (window.isMinimized()) window.restore();
  window.show();
  window.focus();
  startupLog('window activated');
}

if (process.env.BEEF_TEST_USER_DATA) app.setPath('userData', process.env.BEEF_TEST_USER_DATA);
const hasInstanceLock = process.env.BEEF_TEST_USER_DATA ? true : app.requestSingleInstanceLock();
startupLog(`single-instance lock=${hasInstanceLock}`);
if (!hasInstanceLock) {
  // The primary instance receives the second-instance event and is responsible
  // for showing its window. Exit immediately so a portable secondary process
  // never lingers while Electron is still becoming ready.
  startupLog('secondary instance exiting');
  app.quit();
} else {
  app.on('second-instance', () => {
    startupLog('second-instance received');
    activateMainWindow();
  });
  app.whenReady().then(() => {
    startupLog('app ready');
    return start();
  }).then(() => startupLog('start completed')).catch(error => {
    startupLog(`startup failure: ${error?.stack || error}`);
    dialog.showErrorBox('起動エラー', String(error));
    app.quit();
  });
}
app.on('window-all-closed', () => app.quit());

async function loadUiWithRetry(targetUrl, attempts = 3) {
  let lastError;
  for (let attempt = 1; attempt <= attempts; attempt++) {
    try {
      startupLog(`UI load attempt ${attempt}/${attempts}`);
      await window.loadURL(targetUrl);
      startupLog(`UI load attempt ${attempt} succeeded`);
      return;
    } catch (error) {
      lastError = error;
      startupLog(`UI load attempt ${attempt} failed: ${error?.stack || error}`);
      if (attempt < attempts) await sleep(500);
    }
  }
  throw lastError || new Error('UI load failed');
}

async function start() {
  startupLog('start entered');
  const flasher = createFlasher(app.isPackaged ? path.join(process.resourcesPath, 'avrdude') : path.join(__dirname, '../beef-tool/avrdude'));
  const authorize = event => {
    if (event.sender !== window?.webContents || event.senderFrame !== window.webContents.mainFrame || !trusted(event.senderFrame.url)) throw new Error('Unauthorized');
  };
  ipcMain.handle('beef:select-firmware', async event => {
    authorize(event);
    if (flasher.busy) throw new Error('書き込み中です。');
    const result = await dialog.showOpenDialog(window, { title: 'BEEF BOARD用ファームウェアを選択', properties: ['openFile'], filters: [{ name: 'Intel HEX', extensions: ['hex'] }] });
    return flasher.select(result.canceled ? null : result.filePaths[0]);
  });
  ipcMain.handle('beef:flash-firmware', async event => {
    authorize(event);
    return flasher.flash(text => { if (!window.webContents.isDestroyed()) window.webContents.send('beef:flash-log', text); });
  });
  ipcMain.handle('beef:restart-dfu', async event => {
    authorize(event);
    return flasher.restartDfu(text => { if (!window.webContents.isDestroyed()) window.webContents.send('beef:flash-log', text); });
  });
  ipcMain.handle('beef:authorize-hid-identity', async (event, identity) => {
    authorize(event);
    const supported = [[0xfeed, 0x0000], [0xfeed, 0x0003], [0x1ccf, 0x1018], [0x1ccf, 0x8048]];
    if (!identity || !supported.some(([vendorId, productId]) =>
      identity.vendorId === vendorId && identity.productId === productId)) throw new Error('Unsupported HID identity');
    // WebHID requires a user gesture for requestDevice(). The firmware restart
    // completes after the original mode-selection gesture has expired.
    const code = `navigator.hid.requestDevice({filters:[{vendorId:${identity.vendorId},productId:${identity.productId},usagePage:65515,usage:1}]}).then(devices=>devices.length)`;
    return window.webContents.executeJavaScript(code, true);
  });
  ipcMain.handle('beef:get-version', async event => {
    authorize(event);
    return app.getVersion();
  });
  ipcMain.handle('beef:check-update', async event => {
    authorize(event);
    try {
      return await checkForUpdate((url, options) => net.fetch(url, options), app.getVersion());
    } catch (error) {
      console.warn('Update check failed:', error);
      return null;
    }
  });
  ipcMain.handle('beef:check-firmware-update', async (event, board, currentVersion) => {
    authorize(event);
    try {
      return await checkForFirmwareUpdate((url, options) => net.fetch(url, options), board, currentVersion);
    } catch (error) {
      console.warn('Firmware update check failed:', error);
      return null;
    }
  });
  ipcMain.handle('beef:open-update', async (event, url) => {
    authorize(event);
    if (typeof url !== 'string' || !url.startsWith('https://github.com/okapi2dx/Beef-Board-RP2354B/')) throw new Error('Invalid update URL');
    await shell.openExternal(url);
  });
  const ses = session.fromPartition('persist:beef-board');
  const root = app.isPackaged ? path.join(__dirname, 'ui') : path.join(__dirname, '../beef-config/build');
  // A local HTTPS origin preserves WebHID/WebUSB without opening a listening port.
  await ses.protocol.handle('https', request => {
    if (!trusted(request.url)) return new Response('Forbidden', { status: 403 });
    let name;
    try { name = decodeURIComponent(new URL(request.url).pathname); }
    catch { return new Response('Bad request', { status: 400 }); }
    const file = path.resolve(root, '.' + name);
    if (file !== root && !file.startsWith(root + path.sep)) return new Response('Forbidden', { status: 403 });
    const target = name === '/' ? path.join(root, fs.existsSync(path.join(root, 'index.html')) ? 'index.html' : '404.html') : file;
    if (!fs.existsSync(target) || !fs.statSync(target).isFile()) return new Response('Not found', { status: 404 });
    return net.fetch(pathToFileURL(target).href);
  });
  ses.setPermissionCheckHandler((contents, permission, origin) =>
    contents === window?.webContents && trusted(origin) && ['hid', 'usb'].includes(permission));
  ses.setPermissionRequestHandler((contents, permission, callback) =>
    callback(contents === window?.webContents && trusted(contents.getURL()) && ['hid', 'usb'].includes(permission)));
  ses.setDevicePermissionHandler(details => trusted(details.origin) && allowedDevice(details.deviceType, details.device));
  for (const type of ['hid', 'usb']) {
    ses.on(`select-${type}-device`, async (event, details, callback) => {
      event.preventDefault();
      if (!trusted(details.frame?.url)) return callback('');
      const devices = details.deviceList.filter(device => allowedDevice(type, device));
      if (!devices.length && type === 'usb') {
        callback('');
        await dialog.showMessageBox(window, { type: 'info', title: '機器が見つかりません',
          message: 'DFU機器が見つかりません。起動モードとWinUSBドライバーを確認して、再接続してください。' });
        return;
      }
      // The new VID/PID may appear after requestDevice starts. Keep this HID
      // chooser alive briefly so Electron can deliver hid-device-added.
      if (!devices.length && type === 'hid') {
        waitForHidDevice(ses, callback, device => allowedDevice('hid', device));
        return;
      }
      if (!devices.length) return callback('');
      if (devices.length === 1) return callback(devices[0].deviceId);
      try {
        const result = await dialog.showMessageBox(window, { title: '接続する機器', message: 'BEEF BOARDを選択してください。',
          buttons: [...devices.map((d, i) => `${i + 1}: ${d.name || d.productName || 'AT90USB1286 DFU'} ${d.serialNumber || ''}`), 'キャンセル'],
          cancelId: devices.length, defaultId: 0 });
        callback(devices[result.response]?.deviceId || '');
      } catch { callback(''); }
    });
  }
  startupLog('creating BrowserWindow');
  window = new BrowserWindow({ width: 980, height: 740, minWidth: 720, minHeight: 560, show: false,
    title: 'Beef Board Configurator', autoHideMenuBar: true,
    webPreferences: { session: ses, preload: path.join(__dirname, 'preload.cjs'), nodeIntegration: false, contextIsolation: true, sandbox: true } });

  let rendererRecoveryCount = 0;
  let startupShowTimer = setTimeout(() => {
    if (!window || window.isDestroyed() || window.isVisible()) return;
    startupLog('startup visibility fallback fired');
    activateMainWindow();
  }, 4000);

  const clearStartupShowTimer = () => {
    if (!startupShowTimer) return;
    clearTimeout(startupShowTimer);
    startupShowTimer = null;
  };

  window.once('ready-to-show', () => {
    startupLog('window ready-to-show');
    clearStartupShowTimer();
    activateMainWindow();
  });
  window.webContents.on('did-finish-load', () => {
    startupLog('renderer did-finish-load');
    clearStartupShowTimer();
    rendererRecoveryCount = 0;
    activateMainWindow();
  });
  window.webContents.on('did-fail-load', (_event, errorCode, errorDescription, validatedURL, isMainFrame) => {
    startupLog(`did-fail-load main=${isMainFrame} code=${errorCode} url=${validatedURL} error=${errorDescription}`);
  });
  window.webContents.on('render-process-gone', (_event, details) => {
    startupLog(`render-process-gone reason=${details.reason} exitCode=${details.exitCode}`);
    if (details.reason === 'clean-exit' || !window || window.isDestroyed()) return;
    if (rendererRecoveryCount >= 2) {
      try {
        dialog.showErrorBox(
          '表示プロセスの復旧に失敗しました',
          `アプリの表示プロセスが繰り返し終了しました。\n\n起動ログ: ${startupLogPath}`
        );
      } catch { /* best effort */ }
      return;
    }
    rendererRecoveryCount++;
    setTimeout(() => {
      if (!window || window.isDestroyed()) return;
      startupLog(`renderer recovery attempt ${rendererRecoveryCount}`);
      void loadUiWithRetry(ORIGIN + '/', 2).then(() => activateMainWindow()).catch(error => {
        startupLog(`renderer recovery failed: ${error?.stack || error}`);
      });
    }, 300);
  });
  window.on('unresponsive', () => {
    startupLog('window unresponsive');
    activateMainWindow();
  });
  window.on('responsive', () => startupLog('window responsive'));
  window.on('closed', () => {
    clearStartupShowTimer();
    window = undefined;
  });
  window.on('close', event => { if (flasher.busy) event.preventDefault(); });
  app.on('before-quit', event => { if (flasher.busy) event.preventDefault(); });
  window.removeMenu();

  const external = url => { if (url.startsWith('https://github.com/') || url.startsWith('https://zadig.akeo.ie/')) void shell.openExternal(url); };
  window.webContents.setWindowOpenHandler(({ url }) => { external(url); return { action: 'deny' }; });
  window.webContents.on('will-navigate', (event, url) => { if (!trusted(url)) { event.preventDefault(); external(url); } });
  window.webContents.on('will-prevent-unload', () => {
    // Honor the firmware screen's close guard; never force-close a flashing session.
    void dialog.showMessageBox(window, { type: 'warning', message: '書き込み中です。完了するまでアプリを閉じないでください。' });
  });

  startupLog('loading UI');
  await loadUiWithRetry(ORIGIN + '/');
  startupLog('UI loaded');
  clearStartupShowTimer();
  activateMainWindow();
}
