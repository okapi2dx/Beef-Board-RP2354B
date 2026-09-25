import { DfuDevice } from "$lib/types/dfu";
import { Command, detectDevice, readBoardInfo, sendCommand, waitForReconnection } from "$lib/types/hid";
import type { BoardInfo } from "$lib/types/board";

class AppState {
  disableConfigTab = $state(false);
  device = $state<HIDDevice | null>(null);
  boardInfo = $state<BoardInfo | null>(null);
  error = $state<string | undefined>();
  connecting = $state(false);
  dfuReconnectAvailable = $state(false);
}

export const appState = new AppState();

export async function onDisconnect(event?: HIDConnectionEvent): Promise<void> {
  // Ignore a late disconnect event from the old USB instance after a restart.
  // Otherwise it can tear down the newly re-enumerated/reconnected device.
  if (event?.device && appState.device && event.device !== appState.device) return;

  navigator.hid.removeEventListener('disconnect', onDisconnect);
  const device = appState.device;
  appState.device = null;
  appState.boardInfo = null;
  try { await device?.close(); } catch { /* Device may already be unplugged. */ }
  appState.disableConfigTab = false;
  appState.error = undefined;
}

async function finishConnection(device: HIDDevice | null): Promise<void> {
  appState.device = device;
  appState.boardInfo = device ? await readBoardInfo(device) : null;
  if (!device) return;
  appState.dfuReconnectAvailable = false;
  navigator.hid.addEventListener('disconnect', onDisconnect);
  appState.error = undefined;
}

export async function connectDevice(): Promise<void> {
  if (appState.connecting) return;
  appState.connecting = true;
  try {
    await finishConnection(await detectDevice());
  } catch (err) {
    await onDisconnect();
    appState.error = `Error communicating with device: ${err}`;
  } finally {
    appState.connecting = false;
  }
}

export async function reconnectDfuDevice(): Promise<void> {
  if (appState.connecting) return;

  appState.connecting = true;
  appState.error = undefined;
  let webUsbError: unknown;
  let nativeExitCode: number | null = null;

  try {
    // Prefer WebUSB while the reconnect click still carries a user gesture.
    // This works when the Atmel DFU interface is bound to WinUSB.
    try {
      const dfu = await DfuDevice.connect();
      if (dfu) {
        await dfu.startApplication();
        await waitForReconnection(20000);
        appState.dfuReconnectAvailable = false;
        return;
      }
    } catch (err) {
      webUsbError = err;
    }

    // Fall back to the bundled patched AVRDUDE. This covers the legacy
    // beef-tool/libusb driver path that Chromium cannot claim through WebUSB.
    if (window.beefNative) {
      const result = await window.beefNative.restartDfu();
      nativeExitCode = result.exitCode;
      if (result.success) {
        await waitForReconnection(20000);
        appState.dfuReconnectAvailable = false;
        return;
      }

      // START_APP intentionally disconnects USB immediately. Some AVRDUDE/
      // driver combinations can therefore return a non-zero exit code even
      // though the controller already reached normal HID mode.
      try {
        await waitForReconnection(6000);
        appState.dfuReconnectAvailable = false;
        return;
      } catch {
        // Neither DFU path produced a reconnect; report the useful details.
      }
    }

    const webUsbDetail = webUsbError ? `; WebUSB: ${webUsbError}` : '';
    const nativeDetail = window.beefNative
      ? `; AVRDUDE終了コード: ${nativeExitCode ?? 'unknown'}`
      : '';
    throw new Error(`DFUデバイスを通常モードへ切り替えられませんでした${webUsbDetail}${nativeDetail}`);
  } catch (err) {
    await onDisconnect();
    appState.error = `Error communicating with device: ${err}`;
  } finally {
    appState.connecting = false;
  }
}

export async function reconnectDevice(): Promise<void> {
  if (appState.connecting) return;
  if (!appState.device) {
    await connectDevice();
    return;
  }

  appState.connecting = true;
  appState.error = undefined;
  try {
    // The controller can disappear from USB before Chromium resolves the
    // SET_REPORT promise. A send error here can therefore mean the restart
    // command already succeeded, so always continue into re-enumeration.
    try {
      await sendCommand(Command.Restart);
    } catch {
      // Expected when the USB restart races the host-side control transfer.
    }
    await waitForReconnection(20000);
  } catch (err) {
    await onDisconnect();
    appState.error = `Error communicating with device: ${err}`;
  } finally {
    appState.connecting = false;
  }
}
