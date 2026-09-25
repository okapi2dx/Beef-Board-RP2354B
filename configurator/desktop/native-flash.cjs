const fs = require('node:fs/promises');
const path = require('node:path');
const os = require('node:os');
const { spawn } = require('node:child_process');

function validateHex(text) {
  let base = 0, count = 0, eof = false;
  const used = new Set();
  for (const raw of text.trim().split(/\r?\n/)) {
    const line = raw.trim();
    if (!line) continue;
    if (eof || !/^:([0-9a-fA-F]{2})+$/.test(line)) throw new Error('HEXの形式が正しくありません。');
    const b = Buffer.from(line.slice(1), 'hex');
    if (b.length < 5 || b.length !== b[0] + 5 || (b.reduce((sum, v) => sum + v, 0) & 255)) throw new Error('HEXの長さまたはチェックサムが不正です。');
    const address = b.readUInt16BE(1), type = b[3], n = b[0];
    if (type === 0) {
      for (let i = 0; i < n; i++) {
        const at = base + address + i;
        if (at >= 0x20000 || used.has(at)) throw new Error('アドレスがAT90USB1286の範囲外、または重複しています。');
        used.add(at); count++;
      }
    } else if (type === 1 && n === 0 && address === 0) eof = true;
    else if ((type === 2 || type === 4) && n === 2 && address === 0) base = b.readUInt16BE(4) * (type === 2 ? 16 : 65536);
    else if ((type === 3 || type === 5) && n === 4 && address === 0) { /* Start address is not used by AVR. */ }
    else throw new Error('未対応または不正なHEXレコードです。');
  }
  if (!eof || !count) throw new Error('終了レコードまたは書き込みデータがありません。');
  return count;
}

function extractFirmwareInfo(text) {
  let base = 0;
  const image = new Map();
  for (const raw of text.trim().split(/\r?\n/)) {
    const line = raw.trim();
    if (!line || !line.startsWith(':')) continue;
    const b = Buffer.from(line.slice(1), 'hex');
    if (b.length < 5) continue;
    const address = b.readUInt16BE(1), type = b[3], n = b[0];
    if (type === 0) {
      for (let i = 0; i < n; i++) image.set(base + address + i, b[4 + i]);
    } else if ((type === 2 || type === 4) && n === 2 && address === 0) {
      base = b.readUInt16BE(4) * (type === 2 ? 16 : 65536);
    }
  }

  const magic = Buffer.from('BEEFVER1', 'ascii');
  for (const [address, value] of image) {
    if (value !== magic[0]) continue;
    let match = true;
    for (let i = 1; i < magic.length; i++) {
      if (image.get(address + i) !== magic[i]) { match = false; break; }
    }
    if (!match) continue;
    const h = magic.length;
    const hash = (
      image.get(address + h) |
      (image.get(address + h + 1) << 8) |
      (image.get(address + h + 2) << 16) |
      (image.get(address + h + 3) << 24)
    ) >>> 0;
    const major = image.get(address + h + 4);
    const minor = image.get(address + h + 5);
    const patch = image.get(address + h + 6);
    return {
      commitHash: hash.toString(16).padStart(8, '0'),
      version: [major, minor, patch].every(Number.isInteger) ? `${major}.${minor}.${patch}` : null
    };
  }
  return { commitHash: null, version: null };
}

function runProcess(spawnProcess, executable, args, options, onLog) {
  return new Promise((resolve, reject) => {
    const child = spawnProcess(executable, args, options);
    for (const stream of [child.stdout, child.stderr]) stream?.on('data', chunk => onLog(chunk.toString()));
    child.once('error', reject);
    child.once('close', code => resolve(code));
  });
}

function createFlasher(toolDir, spawnProcess = spawn) {
  let selected, busy = false;
  return {
    get busy() { return busy; },
    async select(file) {
      if (busy) throw new Error('書き込み中です。');
      selected = undefined;
      if (!file) return null;
      if ((await fs.stat(file)).size > 2 * 1024 * 1024) throw new Error('HEXファイルが大きすぎます。');
      const text = await fs.readFile(file, 'utf8');
      const bytes = validateHex(text);
      const firmwareInfo = extractFirmwareInfo(text);
      selected = { text, name: path.basename(file), bytes };
      return { name: selected.name, bytes, ...firmwareInfo };
    },
    async restartDfu(onLog) {
      if (busy) throw new Error('処理中です。');
      busy = true;
      try {
        const executable = path.join(toolDir, 'avrdude.exe');
        const options = { windowsHide: true, shell: false };
        const restartArgs = ['-C', path.join(toolDir, 'avrdude.conf'), '-c', 'flip1', '-p', 'usb1286', '-F', '-x', 'start_app'];
        const exitCode = await runProcess(spawnProcess, executable, restartArgs, options, onLog);
        return { success: exitCode === 0, exitCode };
      } finally {
        busy = false;
      }
    },
    async flash(onLog) {
      if (busy || !selected) throw new Error('書き込み中、またはファイル未選択です。');
      busy = true;
      let temporary;
      try {
        temporary = await fs.mkdtemp(path.join(os.tmpdir(), 'beef-flash-'));
        // Snapshot the validated file under a fixed name: user input never becomes a command argument.
        await fs.writeFile(path.join(temporary, 'firmware.hex'), selected.text, 'utf8');
        const executable = path.join(toolDir, 'avrdude.exe');
        const options = { cwd: temporary, windowsHide: true, shell: false };
        const writeArgs = ['-C', path.join(toolDir, 'avrdude.conf'), '-c', 'flip1', '-p', 'usb1286', '-U', 'flash:w:firmware.hex:i'];
        const exitCode = await runProcess(spawnProcess, executable, writeArgs, options, onLog);
        if (exitCode !== 0) return { success: false, exitCode, restartSuccess: false, restartExitCode: null };

        onLog('\n[INFO] Starting normal application...\n');
        const restartArgs = ['-C', path.join(toolDir, 'avrdude.conf'), '-c', 'flip1', '-p', 'usb1286', '-F', '-x', 'start_app'];
        const restartExitCode = await runProcess(spawnProcess, executable, restartArgs, options, onLog);
        return {
          success: true,
          exitCode,
          restartSuccess: restartExitCode === 0,
          restartExitCode
        };
      } finally {
        busy = false;
        if (temporary) {
          // Only the exact file and directory created above are removed; no recursive removal.
          await fs.unlink(path.join(temporary, 'firmware.hex')).catch(() => {});
          await fs.rmdir(temporary).catch(() => {});
        }
      }
    }
  };
}
module.exports = { createFlasher, validateHex, extractFirmwareInfo };
