<script lang="ts">
  import { onMount } from 'svelte';
  import MemoryMap from 'nrf-intel-hex';
  import { Button } from '$lib/components/ui/button';
  import { Input } from '$lib/components/ui/input';
  import { Label } from '$lib/components/ui/label';
  import { Progress } from '$lib/components/ui/progress';
  import * as AlertDialog from '$lib/components/ui/alert-dialog';
  import { ATMEL_MAX_TRANSFER_SIZE, DfuDevice } from '$lib/types/dfu';
  import { Command, readFirmwareInfo, sendCommand, type FirmwareInfo } from '$lib/types/hid';
  import { appState, onDisconnect } from '$lib/types/state.svelte';
  import { formatDisplayVersion } from '$lib/types/version';

  let firmwareInfo = $state<FirmwareInfo | null>(null);
  let device: DfuDevice | undefined = $state();
  let firmware: MemoryMap | undefined = $state();
  let filename = $state('');
  let byteCount = $state(0);
  let parsing = $state(false);
  let busy = $state(false);
  let finished = $state(false);
  let progress = $state(0);
  let message = $state('HEXファイルを選択してください。');
  let selection = 0;
  let confirmOpen = $state(false);

  $effect(() => {
    const current = appState.device;
    if (current) void readFirmwareInfo().then(info => {
      if (appState.device === current) firmwareInfo = info;
    }).catch(err => { if (appState.device === current) appState.error = `${err}`; });
    else firmwareInfo = null;
  });

  onMount(() => {
    const disconnected = (event: USBConnectionEvent) => {
      if (event.device !== device?.device) return;
      device = undefined;
      finished = false;
      appState.error = '書き込みモードの機器が切断されました。接続し直して再試行してください。';
    };
    navigator.usb.addEventListener('disconnect', disconnected);
    return () => {
      navigator.usb.removeEventListener('disconnect', disconnected);
      void device?.device.close().catch(() => {});
    };
  });

  async function selectFile(event: Event) {
    const token = ++selection;
    firmware = undefined; filename = ''; byteCount = 0; finished = false; progress = 0;
    appState.error = undefined;
    const file = (event.target as HTMLInputElement).files?.[0];
    if (!file) { parsing = false; return; }
    parsing = true;
    try {
      const text = await file.text();
      if (token !== selection) return;
      const parsed = MemoryMap.fromHex(text, ATMEL_MAX_TRANSFER_SIZE);
      let count = 0;
      for (const [address, bytes] of parsed) {
        if (address < 0 || address + bytes.length > 0x10000) throw new Error('64 KiB以上のアドレスはこの書き込み方式では未対応です。');
        count += bytes.length;
      }
      if (!count) throw new Error('書き込みデータがありません。');
      firmware = parsed; filename = file.name; byteCount = count;
      message = 'ファイル確認済み。書き込みモードの機器に接続してください。';
    } catch (err) { if (token === selection) appState.error = `HEXファイルを読み込めません: ${err}`; }
    finally { if (token === selection) parsing = false; }
  }

  async function bootloader() {
    busy = true;
    appState.disableConfigTab = true;
    navigator.hid.removeEventListener('disconnect', onDisconnect);
    try {
      await sendCommand(Command.Bootloader);
      await onDisconnect();
      appState.disableConfigTab = true;
      message = '書き込みモードへ切り替えました。数秒後に「書き込みモードの機器に接続」を押してください。';
    } catch (err) {
      appState.disableConfigTab = false;
      navigator.hid.addEventListener('disconnect', onDisconnect);
      appState.error = `書き込みモードに切り替えられません: ${err}`;
    } finally { busy = false; }
  }

  async function connect() {
    busy = true; appState.error = undefined;
    try {
      const connected = await DfuDevice.connect();
      if (!connected) { message = '接続されていません。機器を確認して再試行してください。'; return; }
      device = connected;
      appState.disableConfigTab = true;
      connected.on('message', value => message = value);
      connected.on('progress', value => progress = value);
      message = '書き込みモードの機器に接続しました。';
    } catch (err) { appState.error = `接続できません: ${err}`; }
    finally { busy = false; }
  }

  async function flash() {
    confirmOpen = false;
    if (!device || !firmware || busy || parsing) return;
    busy = true; finished = false; progress = 0; appState.error = undefined;
    const blockClose = (event: BeforeUnloadEvent) => { event.preventDefault(); event.returnValue = ''; };
    window.addEventListener('beforeunload', blockClose);
    try {
      await device.downloadFirmware(firmware);
      finished = true; message = '書き込みが完了しました。';
    } catch (err) { appState.error = `書き込みに失敗しました。接続を確認し、再試行してください: ${err}`; }
    finally { busy = false; window.removeEventListener('beforeunload', blockClose); }
  }

  async function restart() {
    if (!device || busy) return;
    busy = true;
    try {
      const current = device;
      await current.startApplication();
      device = undefined;
      await current.device.close().catch(() => {});
      await onDisconnect();
      finished = false;
      message = '通常モードで再起動しました。Configタブから接続してください。';
    } catch (err) { appState.error = `再起動できません。USBを接続し直してください: ${err}`; }
    finally { busy = false; }
  }
</script>

<div class="space-y-5">
  <h2 class="text-xl font-bold">ファームウェア書き込み</h2>
  {#if firmwareInfo}<p>Firmware: <strong>{firmwareInfo.version ? formatDisplayVersion(firmwareInfo.version) : 'version unavailable'}</strong></p>{/if}
  <div class="space-y-2">
    <Label for="firmware-file">1. BEEF BOARD用ファームウェア（.hex）を選択</Label>
    <Input id="firmware-file" type="file" accept=".hex" onchange={selectFile} disabled={busy} />
    {#if parsing}<p>ファイルを確認中…</p>{/if}
    {#if filename}<p data-testid="selected-firmware">選択済み: {filename}（{byteCount.toLocaleString()} バイト）</p>{/if}
  </div>
  <div class="space-y-2">
    <h3 class="font-semibold">2. 書き込みモードへ接続</h3>
    {#if appState.device && !device}
      <Button variant="outline" class="reset-action-button" onclick={bootloader} disabled={busy}>接続中の基板を書き込みモードにする</Button>
    {/if}
    {#if !device}
      <Button variant="outline" class="reset-action-button" onclick={connect} disabled={busy}>書き込みモードの機器に接続</Button>
      <p class="text-sm text-muted-foreground">すでに書き込みモードの場合は直接接続できます。WindowsではDFU機器（03EB:2FFB）にWinUSBが必要です。</p>
      <p class="text-sm"><a class="underline" href="https://zadig.akeo.ie/">ドライバー設定（Zadig）</a>：通常のHID機器のドライバーは変更しないでください。</p>
    {:else}<p>接続済み: AT90USB1286 DFU</p>{/if}
  </div>
  <div class="space-y-2">
    <h3 class="font-semibold">3. 選択したファームウェアを書き込み</h3>
    <AlertDialog.Root bind:open={confirmOpen}>
      <AlertDialog.Trigger>
        {#snippet child({ props })}<Button {...props} variant="outline" class="reset-action-button" disabled={!firmware || !device || busy || parsing || finished}>書き込み開始</Button>{/snippet}
      </AlertDialog.Trigger>
      <AlertDialog.Content>
        <AlertDialog.Header>
          <AlertDialog.Title>ファームウェアを書き込みますか？</AlertDialog.Title>
          <AlertDialog.Description>{filename} を接続中の基板に書き込みます。現在のファームウェアは消去されます。BEEF BOARD用のファイルであることを確認してください。</AlertDialog.Description>
        </AlertDialog.Header>
        <AlertDialog.Footer>
          <AlertDialog.Cancel>キャンセル</AlertDialog.Cancel>
          <AlertDialog.Action class="reset-action-button" onclick={flash}>このファイルを書き込む</AlertDialog.Action>
        </AlertDialog.Footer>
      </AlertDialog.Content>
    </AlertDialog.Root>
    <p role="status">{message}</p>
    <Progress value={progress} max={1} />
    {#if busy}<p class="font-semibold">処理中です。USBを抜かず、アプリを閉じないでください。</p>{/if}
    {#if finished}<Button variant="outline" class="reset-action-button" onclick={restart} disabled={busy}>通常モードで再起動</Button>{/if}
  </div>
</div>
