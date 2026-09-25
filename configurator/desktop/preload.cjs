const { contextBridge, ipcRenderer } = require('electron');
contextBridge.exposeInMainWorld('beefNative', {
  selectFirmware: () => ipcRenderer.invoke('beef:select-firmware'),
  flashFirmware: () => ipcRenderer.invoke('beef:flash-firmware'),
  restartDfu: () => ipcRenderer.invoke('beef:restart-dfu'),
  getVersion: () => ipcRenderer.invoke('beef:get-version'),
  checkForUpdates: () => ipcRenderer.invoke('beef:check-update'),
  openUpdate: url => ipcRenderer.invoke('beef:open-update', url),
  onLog: callback => {
    const listener = (_event, text) => callback(text);
    ipcRenderer.on('beef:flash-log', listener);
    return () => ipcRenderer.removeListener('beef:flash-log', listener);
  }
});
