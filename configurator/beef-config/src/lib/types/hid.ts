import { appState, onDisconnect } from '$lib/types/state.svelte';
import { featureReportPayload } from '$lib/types/feature-report';
import {
  BEEF_CONFIG_USAGE,
  BEEF_CONFIG_USAGE_PAGE,
  RP2354B_BEEF_PRODUCT_ID,
  RP2354B_BEEF_VENDOR_ID,
  inferBoardInfo,
  isUnsupportedBoard,
  parseBoardInfo,
  type BoardInfo
} from '$lib/types/board';

export enum ReportId {
  Config = 1,
  Command = 2,
  FirmwareVersion = 3,
  Diagnostics = 4,
  DiagnosticsStream = 5,
  BoardInfo = 6
}

export enum Command {
  Bootloader = 1,
  ResetConfig = 2,
  Restart = 3
}

export type Diagnostics = {
  buttons: number;
  sensorAB: number;
  raw: number;
  filtered: number;
  output: number;
  direction: number;
  transitions: number;
  intervalMs: number;
  ledLevels: number[];
};

export function parseDiagnostics(data: DataView): Diagnostics {
  return {
    buttons: data.getUint16(0, true), sensorAB: data.getUint8(2), raw: data.getUint8(3),
    filtered: data.getUint8(4), output: data.getUint8(5), direction: data.getInt8(6),
    transitions: data.getUint32(7, true), intervalMs: data.getUint16(11, true),
    // v30+ appends the 11 PWM output levels. Keep older diagnostics packets
    // readable so the monitor can still connect before a firmware update.
    ledLevels: data.byteLength >= 24 ? Array.from({ length: 11 }, (_, i) => data.getUint8(13 + i)) : [],
  };
}

export async function readDiagnostics(): Promise<Diagnostics> {
  if (!appState.device) throw new Error('Device not connected');
  const result = await appState.device.receiveFeatureReport(ReportId.Diagnostics);
  const data = featureReportPayload(result, ReportId.Diagnostics, [13, 24]);
  return parseDiagnostics(data);
}

const entryProductNames = new Set(['BEEF BOARD', 'beatmania IIDX controller entry model']);
const premiumProductNames = new Set(['BEEF BOARD', 'beatmania IIDX controller premium model']);
const rp2354bProductNames = new Set(['BEEF BOARD RP2354B', 'Beef Board RP2354B']);
const beefHidFilters: HIDDeviceFilter[] = [
  { vendorId: 0xfeed, productId: 0x0000, usagePage: BEEF_CONFIG_USAGE_PAGE, usage: BEEF_CONFIG_USAGE }, // Legacy/default
  { vendorId: RP2354B_BEEF_VENDOR_ID, productId: RP2354B_BEEF_PRODUCT_ID, usagePage: BEEF_CONFIG_USAGE_PAGE, usage: BEEF_CONFIG_USAGE }, // RP2354B
  { vendorId: 0x1ccf, productId: 0x1018, usagePage: BEEF_CONFIG_USAGE_PAGE, usage: BEEF_CONFIG_USAGE }, // IIDX Entry
  { vendorId: 0x1ccf, productId: 0x8048, usagePage: BEEF_CONFIG_USAGE_PAGE, usage: BEEF_CONFIG_USAGE }  // IIDX Premium
];

function hasConfigCollection(device: HIDDevice): boolean {
  return device.collections.some((collection) =>
    collection.usagePage === BEEF_CONFIG_USAGE_PAGE && collection.usage === BEEF_CONFIG_USAGE);
}

function isBeefDevice(device: HIDDevice): boolean {
  if (!hasConfigCollection(device)) return false;
  if (device.vendorId === 0xfeed && device.productId === 0x0000) return device.productName === 'BEEF BOARD';
  if (device.vendorId === RP2354B_BEEF_VENDOR_ID && device.productId === RP2354B_BEEF_PRODUCT_ID) {
    return rp2354bProductNames.has(device.productName);
  }
  if (device.vendorId !== 0x1ccf) return false;
  if (device.productId === 0x1018) return entryProductNames.has(device.productName);
  if (device.productId === 0x8048) return premiumProductNames.has(device.productName);
  return false;
}

export async function readBoardInfo(device: HIDDevice): Promise<BoardInfo> {
  const fallback = inferBoardInfo(device);

  // Report 6 distinguishes RP2354B and unsupported boards from legacy AT90
  // devices when the Entry/Premium USB identity is shared.
  for (let attempt = 0; attempt < 3; attempt++) {
    try {
      const result = await device.receiveFeatureReport(ReportId.BoardInfo);

      // WebHID normally returns only the report payload. Accept an older
      // Chromium/Electron layout with the report ID byte included as well.
      const payload = new DataView(result.buffer, result.byteOffset, result.byteLength);
      const direct = parseBoardInfo(payload);
      if (direct) return direct;

      if (result.byteLength > 11 && payload.getUint8(0) === ReportId.BoardInfo) {
        const withId = new DataView(result.buffer, result.byteOffset + 1, result.byteLength - 1);
        const parsed = parseBoardInfo(withId);
        if (parsed) return parsed;
      }
    } catch {
      // Existing AT90 firmware does not implement Report 6.
    }

    if (attempt < 2) await new Promise(resolve => setTimeout(resolve, 80));
  }

  return fallback;
}

export type HidDeviceIdentity = Pick<HIDDevice, 'vendorId' | 'productId'>;

async function openVerifiedConfigDevice(
  devices: HIDDevice[],
  expectedIdentity?: HidDeviceIdentity
): Promise<HIDDevice | null> {
  for (const device of devices.filter((candidate) =>
    isBeefDevice(candidate) &&
    (!expectedIdentity ||
      (candidate.vendorId === expectedIdentity.vendorId &&
        candidate.productId === expectedIdentity.productId))
  )) {
    try {
      if (!device.opened) await device.open();
      await device.receiveFeatureReport(ReportId.Config);
      if (isUnsupportedBoard(await readBoardInfo(device))) {
        throw new Error('Unsupported board');
      }
      return device;
    } catch {
      try { await device.close(); } catch { /* Try the next authorized interface. */ }
    }
  }
  return null;
}

export async function waitForReconnection(
  timeoutMs = 15000,
  expectedIdentity?: HidDeviceIdentity
): Promise<void> {
  await onDisconnect();
  const deadline = Date.now() + timeoutMs;
  const requestPermissionAt = Date.now() + 2000;
  let requestedNewIdentity = false;
  while (Date.now() < deadline) {
    let selectedDevice = await openVerifiedConfigDevice(
      await navigator.hid.getDevices(),
      expectedIdentity
    );
    // Changing controller mode also changes VID/PID. Chromium may therefore
    // treat the restarted board as a new WebHID identity even though Electron's
    // device policy allows it. Re-run the chooser once in the desktop app so
    // the new identity is granted without making the user reconnect the cable.
    if (!selectedDevice && !requestedNewIdentity && window.beefNative && Date.now() >= requestPermissionAt) {
      requestedNewIdentity = true;
      try {
        selectedDevice = await openVerifiedConfigDevice(
          await navigator.hid.requestDevice({ filters: expectedIdentity
            ? [{ ...expectedIdentity, usagePage: BEEF_CONFIG_USAGE_PAGE, usage: BEEF_CONFIG_USAGE }]
            : beefHidFilters }),
          expectedIdentity
        );
      } catch {
        // Keep polling: getDevices() may become available after enumeration.
      }
    }
    if (selectedDevice) {
      appState.device = selectedDevice;
      appState.boardInfo = await readBoardInfo(selectedDevice);
      navigator.hid.addEventListener('disconnect', onDisconnect);
      appState.error = undefined;
      return;
    }
    await new Promise((resolve) => setTimeout(resolve, 250));
  }
  throw new Error('Controller restarted, but automatic reconnection timed out');
}

export async function detectAuthorizedDevice(): Promise<HIDDevice | null> {
  return openVerifiedConfigDevice(await navigator.hid.getDevices());
}

export async function detectDevice(): Promise<HIDDevice | null> {
  const devices = await navigator.hid.requestDevice({
    filters: beefHidFilters
  });

  if (devices.length === 0) {
    return null;
  }

  const selectedDevice = await openVerifiedConfigDevice(devices);
  if (!selectedDevice) {
    throw new Error('No compatible BEEF BOARD configuration interface found');
  }
  return selectedDevice;
}

export type FirmwareInfo = {
  version: string | null;
};

export async function readFirmwareInfo(): Promise<FirmwareInfo> {
  if (!appState.device) {
    throw new Error('Device not connected');
  }

  try {
    const result = await appState.device.receiveFeatureReport(ReportId.FirmwareVersion);
    const data = featureReportPayload(result, ReportId.FirmwareVersion, [4, 7]);
    const hasSemanticVersion = data.byteLength >= 7;
    const version = hasSemanticVersion ? `${data.getUint8(4)}.${data.getUint8(5)}.${data.getUint8(6)}` : null;
    return { version };
  } catch (err) {
    throw new Error('Failed to read firmware information', { cause: err });
  }
}

export async function sendCommand(command: Command): Promise<void> {
  if (!appState.device) {
    throw new Error('Device not connected');
  }

  try {
    const device = appState.device;
    const { flushPendingConfigWrites } = await import('$lib/types/config.svelte');
    await flushPendingConfigWrites();
    if (device !== appState.device) throw new Error('Device changed before command');
    const commandData = new Uint8Array([command]);
    await device.sendFeatureReport(ReportId.Command, commandData);
  } catch (err) {
    throw new Error('Failed to send command', { cause: err });
  }
}
