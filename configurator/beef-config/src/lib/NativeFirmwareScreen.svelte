<script lang="ts">
  import { onMount } from 'svelte';
  import { Button } from '$lib/components/ui/button';
  import * as AlertDialog from '$lib/components/ui/alert-dialog';
  import { appState, onDisconnect } from '$lib/types/state.svelte';
  import { Command, sendCommand, waitForReconnection } from '$lib/types/hid';
  import { tr } from '$lib/types/locale.svelte';
  import { formatDisplayVersion } from '$lib/types/version';
  let selected = $state<{ name: string; bytes: number; commitHash: string | null; version: string | null } | null>(null);
  let busy = $state(false);
  let log = $state('');
  let message = $state('BEEF BOARD用のHEXファイルを選択してください。');
  let confirmOpen = $state(false);
  onMount(() => window.beefNative!.onLog(text => log = (log + text).slice(-100000)));
  async function select() {
    busy = true; selected = null; appState.error = undefined;
    try { selected = await window.beefNative!.selectFirmware(); }
    catch (err) { appState.error = `${err}`; }
    finally { busy = false; }
  }
  async function bootloader() {
    busy = true; appState.disableConfigTab = true;
    navigator.hid.removeEventListener('disconnect', onDisconnect);
    try {
      await sendCommand(Command.Bootloader);
      await onDisconnect();
      appState.dfuReconnectAvailable = true;
      message = '書き込みモードに切り替えました。数秒待って「書き込み開始」を押してください。';
    } catch (err) {
      navigator.hid.addEventListener('disconnect', onDisconnect);
      appState.error = `${err}`;
    } finally { busy = false; appState.disableConfigTab = false; }
  }
  async function flash() {
    confirmOpen = false;
    if (busy || !selected) return;
    busy = true; appState.disableConfigTab = true; log = ''; appState.error = undefined;
    appState.dfuReconnectAvailable = true;
    message = '書き込み中です。USBを抜かず、アプリを閉じないでください。';
    try {
      const result = await window.beefNative!.flashFirmware();
      if (!result.success) {
        message = `書き込みに失敗しました（終了コード: ${result.exitCode}）。下のログを確認してください。`;
        return;
      }
      if (!result.restartSuccess) {
        message = '書き込み・照合は完了しましたが、通常モードの起動に失敗しました。';
        appState.error = `START_APPに失敗しました（終了コード: ${result.restartExitCode ?? 'unknown'}）。`;
        return;
      }

      message = '書き込み・照合が完了しました。通常モードのUSB再接続を待っています…';
      await waitForReconnection(20000);
      appState.dfuReconnectAvailable = false;
      message = '書き込みが完了し、通常モードへ自動再接続しました。';
    } catch (err) {
      if (message.startsWith('書き込み・照合が完了しました')) {
        message = '書き込み・照合は完了しましたが、通常モードへの自動再接続に失敗しました。';
      } else if (!message.includes('失敗しました')) {
        message = '書き込みを実行できませんでした。';
      }
      appState.error = `${err}`;
    } finally { busy = false; appState.disableConfigTab = false; }
  }
</script>

<div class="space-y-5">
  <h2 class="text-xl font-bold">{tr('ファームウェア書き込み（beef-tool方式）', 'Firmware Flashing (beef-tool method)')}</h2>
  <p>{tr('旧beef-toolと同じ書き込みツールを内蔵しています。書き込み成功後は通常モードへ自動で再起動・再接続します。', 'The app includes the same flashing tool as beef-tool and automatically restarts and reconnects the board in normal mode after a successful flash.')}</p>
  <div class="space-y-2">
    <h3 class="font-semibold">{tr('1. ファームウェアを選択', '1. Select firmware')}</h3>
    <Button variant="outline" class="reset-action-button" onclick={select} disabled={busy}>{tr('HEXファイルを選択', 'Select HEX file')}</Button>
    {#if selected}
      <div class="space-y-1">
        <p>{tr('選択済み', 'Selected')}: {selected.name}（{selected.bytes.toLocaleString()} {tr('バイト', 'bytes')}）</p>
        {#if selected.version}<p>{tr('ファームウェア', 'Firmware')}: <strong>{formatDisplayVersion(selected.version)}</strong></p>{/if}
        <p>{tr('ハッシュ', 'Hash')}: <code>{selected.commitHash ? `0x${selected.commitHash}` : tr('情報なし', 'unavailable')}</code></p>
      </div>
    {/if}
  </div>
  <div class="space-y-2">
    <h3 class="font-semibold">{tr('2. 基板を書き込みモードにする', '2. Put the board in flashing mode')}</h3>
    {#if appState.device}<Button variant="outline" class="reset-action-button" onclick={bootloader} disabled={busy}>{tr('接続中の基板を書き込みモードにする', 'Switch connected board to flashing mode')}</Button>{/if}
    <p>{tr('通常接続できない場合は、B1＋B2を押しながらUSBを接続してください。すでに書き込みモードの場合は、そのまま次へ進めます。', 'If normal connection is unavailable, hold B1 + B2 while connecting USB. If already in flashing mode, continue.')}</p>
    <p class="text-sm text-muted-foreground">{tr('旧beef-toolで使っていたDFUドライバーを利用します。接続できない場合は下のログを確認してください。', 'This uses the DFU driver used by beef-tool. Check the log below if it cannot connect.')}</p>
    <p class="text-sm text-muted-foreground">{tr('書き込みモードから通常モードへ戻す場合は、画面上部の「再接続」を押してください。USBを抜き差しせず自動で再接続します。', 'To leave flashing mode, press Reconnect at the top of the window. The board returns to normal mode and reconnects automatically without unplugging USB.')}</p>
  </div>
  <div class="space-y-2">
    <h3 class="font-semibold">{tr('3. 書き込み', '3. Flash')}</h3>
    <AlertDialog.Root bind:open={confirmOpen}>
      <AlertDialog.Trigger>{#snippet child({ props })}<Button {...props} variant="outline" class="reset-action-button" disabled={busy || !selected}>{tr('書き込み開始', 'Start flashing')}</Button>{/snippet}</AlertDialog.Trigger>
      <AlertDialog.Content>
        <AlertDialog.Header>
          <AlertDialog.Title>{tr('ファームウェアを書き込みますか？', 'Flash firmware?')}</AlertDialog.Title>
          <AlertDialog.Description>{tr(`${selected?.name} をBEEF BOARDに書き込みます。現在のファームウェアは消去されます。`, `${selected?.name} will be flashed to BEEF BOARD. The current firmware will be erased.`)}</AlertDialog.Description>
        </AlertDialog.Header>
        <AlertDialog.Footer><AlertDialog.Cancel>{tr('キャンセル', 'Cancel')}</AlertDialog.Cancel><AlertDialog.Action class="reset-action-button" onclick={flash}>{tr('このファイルを書き込む', 'Flash this file')}</AlertDialog.Action></AlertDialog.Footer>
      </AlertDialog.Content>
    </AlertDialog.Root>
    <p role="status">{message}</p>
    <pre aria-label={tr('書き込みログ', 'Flashing log')} class="max-h-80 overflow-auto whitespace-pre-wrap rounded border bg-muted p-3 text-xs">{log || tr('実行すると、ここに書き込み・照合のログが表示されます。', 'Flashing and verification output will appear here.')}</pre>
  </div>
</div>
