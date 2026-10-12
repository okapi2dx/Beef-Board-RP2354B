const { contextBridge, ipcRenderer } = require('electron');
contextBridge.exposeInMainWorld('beefNative', {
  getWindowVisibility: () => ipcRenderer.invoke('beef:window-visible'),
  onWindowVisibility: callback => {
    const listener = (_event, visible) => callback(visible);
    ipcRenderer.on('beef:window-visibility', listener);
    return () => ipcRenderer.removeListener('beef:window-visibility', listener);
  },
  selectFirmware: () => ipcRenderer.invoke('beef:select-firmware'),
  flashFirmware: () => ipcRenderer.invoke('beef:flash-firmware'),
  restartDfu: () => ipcRenderer.invoke('beef:restart-dfu'),
  authorizeHidIdentity: identity => ipcRenderer.invoke('beef:authorize-hid-identity', identity),
  getVersion: () => ipcRenderer.invoke('beef:get-version'),
  checkForUpdates: () => ipcRenderer.invoke('beef:check-update'),
  checkForFirmwareUpdates: (board, version) => ipcRenderer.invoke('beef:check-firmware-update', board, version),
  openUpdate: url => ipcRenderer.invoke('beef:open-update', url),
  onLog: callback => {
    const listener = (_event, text) => callback(text);
    ipcRenderer.on('beef:flash-log', listener);
    return () => ipcRenderer.removeListener('beef:flash-log', listener);
  }
});
