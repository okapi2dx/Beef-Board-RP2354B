const { app, BrowserWindow, session, net, nativeTheme } = require('electron');
const path = require('node:path');
const fs = require('node:fs');
const { pathToFileURL } = require('node:url');
const origin = 'https://rp-emulator.local';
app.setPath('userData', process.env.BEEF_RP_EMULATOR_USER_DATA || path.join(app.getPath('appData'), 'Beef Board RP Emulator'));
if (!app.requestSingleInstanceLock()) app.quit();
else app.whenReady().then(async () => {
  nativeTheme.themeSource = 'dark';
  const root = path.join(__dirname, 'ui');
  const ses = session.fromPartition('persist:rp-emulator');
  ses.setPermissionCheckHandler(() => false);
  ses.setPermissionRequestHandler((_wc, _permission, callback) => callback(false));
  ses.setDevicePermissionHandler(() => false);
  ses.on('select-hid-device', (event, _details, callback) => { event.preventDefault(); callback(''); });
  ses.on('select-usb-device', (event, _details, callback) => { event.preventDefault(); callback(''); });
  await ses.protocol.handle('https', request => {
    const url = new URL(request.url);
    if (url.origin !== origin) return new Response('Unavailable in emulator', { status: 403 });
    let name;
    try { name = decodeURIComponent(url.pathname); } catch { return new Response('Bad path', { status: 400 }); }
    const file = path.resolve(root, '.' + (name === '/' ? '/index.html' : name));
    if (!file.startsWith(root + path.sep) || !fs.existsSync(file) || !fs.statSync(file).isFile()) return new Response('Not found', { status: 404 });
    return net.fetch(pathToFileURL(file).href);
  });
  const window = new BrowserWindow({ width: 980, height: 740, minWidth: 720, minHeight: 560,
    title: 'Beef Board RP2354B Emulator', show: false,
    webPreferences: { contextIsolation: true, nodeIntegration: false, sandbox: true, partition: 'persist:rp-emulator' } });
  window.removeMenu();
  app.on('second-instance', () => { if (window.isMinimized()) window.restore(); window.show(); window.focus(); });
  window.webContents.setWindowOpenHandler(() => ({ action: 'deny' }));
  window.webContents.on('will-navigate', (event, url) => { if (new URL(url).origin !== origin) event.preventDefault(); });
  await window.loadURL(origin + '/');
  window.show();
}).catch(error => { console.error(error); app.quit(); });
app.on('window-all-closed', () => app.quit());
