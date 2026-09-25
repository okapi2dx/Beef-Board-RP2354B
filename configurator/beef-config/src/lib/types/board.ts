export const BEEF_CONFIG_USAGE_PAGE = 0xffeb;
export const BEEF_CONFIG_USAGE = 0x01;

// Keep legacy identities unchanged. Board Info report 6 identifies the MCU.
export const RP2354B_BEEF_VENDOR_ID = 0xfeed;
export const RP2354B_BEEF_PRODUCT_ID = 0x0003;

export enum BoardType {
  AT90USB1286 = 'AT90USB1286',
  RP2354B = 'RP2354B',
  Unsupported = 'Unsupported'
}

export enum BootloaderType {
  AtmelFlipDfu = 'Atmel FLIP DFU',
  BeefHid = 'Beef HID Bootloader',
  RpRom = 'RP2350 ROM BOOTSEL',
  Unknown = 'Unknown'
}

export enum BoardCapability {
  Diagnostics = 1 << 0,
  Turntable = 1 << 1,
  Ws2812 = 1 << 2,
  CenterBar = 1 << 3,
  Uart = 1 << 4,
  BootloaderCommand = 1 << 5
}

export type BoardInfo = {
  boardType: BoardType;
  mcu: string;
  protocolVersion: number;
  hardwareRevision: string | null;
  capabilities: number;
  bootloader: BootloaderType;
  source: 'board-info-report' | 'usb-identity';
};

export const BOARD_INFO_MAGIC = [0x42, 0x45, 0x45, 0x46] as const; // "BEEF"
export const BOARD_INFO_PROTOCOL_VERSION = 1;

export type UsbIdentity = { vendorId: number; productId: number };

export function inferBoardInfo(device: UsbIdentity): BoardInfo {
  if (device.vendorId === RP2354B_BEEF_VENDOR_ID && device.productId === RP2354B_BEEF_PRODUCT_ID) {
    return {
      boardType: BoardType.RP2354B,
      mcu: 'RP2354B',
      protocolVersion: 0,
      hardwareRevision: null,
      capabilities: 0,
      bootloader: BootloaderType.RpRom,
      source: 'usb-identity'
    };
  }
  if (device.vendorId === 0xfeed && (device.productId === 0x0001 || device.productId === 0x0002)) {
    return {
      boardType: BoardType.Unsupported,
      mcu: 'Unsupported',
      protocolVersion: 0,
      hardwareRevision: null,
      capabilities: 0,
      bootloader: BootloaderType.BeefHid,
      source: 'usb-identity'
    };
  }

  return {
    boardType: BoardType.AT90USB1286,
    mcu: 'AT90USB1286',
    protocolVersion: 0,
    hardwareRevision: null,
    capabilities: 0,
    bootloader: BootloaderType.AtmelFlipDfu,
    source: 'usb-identity'
  };
}

export function parseBoardInfo(data: DataView): BoardInfo | null {
  // Payload after the report ID:
  // 0..3  ASCII "BEEF"
  // 4     protocol version
  // 5     board type: 1=AT90USB1286, 3=RP2354B; others are unsupported
  // 6     hardware revision major
  // 7     hardware revision minor
  // 8..9  capability bitmask (LE)
  // 10    bootloader: 1=Atmel FLIP DFU, 3=Beef HID, 4=RP2350 ROM BOOTSEL
  if (data.byteLength < 11) return null;
  for (let i = 0; i < BOARD_INFO_MAGIC.length; i++) {
    if (data.getUint8(i) !== BOARD_INFO_MAGIC[i]) return null;
  }

  const boardCode = data.getUint8(5);
  const boardType = boardCode === 1 ? BoardType.AT90USB1286
    : boardCode === 3 ? BoardType.RP2354B : BoardType.Unsupported;

  const bootloaderCode = data.getUint8(10);
  const bootloader = bootloaderCode === 1
    ? BootloaderType.AtmelFlipDfu
    : bootloaderCode === 3
      ? BootloaderType.BeefHid
      : bootloaderCode === 4
        ? BootloaderType.RpRom
        : BootloaderType.Unknown;

  return {
    boardType,
    mcu: boardType,
    protocolVersion: data.getUint8(4),
    hardwareRevision: `${data.getUint8(6)}.${data.getUint8(7)}`,
    capabilities: data.getUint16(8, true),
    bootloader,
    source: 'board-info-report'
  };
}

export function isUnsupportedBoard(info: BoardInfo | null | undefined): boolean {
  return info?.boardType === BoardType.Unsupported;
}

export function isRp2354bBoard(info: BoardInfo | null | undefined): boolean {
  return info?.boardType === BoardType.RP2354B;
}

export function defaultUsbIdentity(info: BoardInfo | null | undefined): UsbIdentity {
  if (isRp2354bBoard(info)) return { vendorId: RP2354B_BEEF_VENDOR_ID, productId: RP2354B_BEEF_PRODUCT_ID };
  if (isUnsupportedBoard(info)) throw new Error('Unsupported board');
  return { vendorId: 0xfeed, productId: 0x0000 };
}
