<script lang="ts">
  import { Button } from '$lib/components/ui/button';
  import { Command, sendCommand } from '$lib/types/hid';
  import { appState } from '$lib/types/state.svelte';
  import { isRp2354bBoard } from '$lib/types/board';
  import { tr } from '$lib/types/locale.svelte';

  let entering = $state(false);
  let bootRequested = $state(false);
  let error = $state('');

  async function enterBootsel(): Promise<void> {
    if (!appState.device || !isRp2354bBoard(appState.boardInfo) || entering) return;
    entering = true;
    error = '';
    try {
      await sendCommand(Command.Bootloader);
      bootRequested = true;
    } catch (cause) {
      error = String(cause);
    } finally {
      entering = false;
    }
  }
</script>

<section class="space-y-5">
  <h2 class="text-xl font-semibold">RP2354B</h2>
  <p>{tr('RP2354Bは内蔵ROMのBOOTSELモードでUF2を書き込みます。', 'RP2354B uses the built-in ROM BOOTSEL mode to flash a UF2 file.')}</p>

  {#if appState.device && isRp2354bBoard(appState.boardInfo)}
    <Button onclick={enterBootsel} disabled={entering || bootRequested}>
      {tr('BOOTSELモードへ', 'Enter BOOTSEL mode')}
    </Button>
  {/if}

  {#if bootRequested}
    <p>{tr('USBドライブが表示されたらUF2をコピーしてください。', 'When the USB drive appears, copy the UF2 file to it.')}</p>
  {/if}

  <p>{tr('ボードが未接続の場合はJ61を短絡し、J60でリセットしてBOOTSELモードに入れます。', 'If the board is disconnected, short J61 and reset with J60 to enter BOOTSEL mode.')}</p>
  <a class="underline" href="./firmware/Beef-Board-RP2354B.uf2" download="Beef-Board-RP2354B.uf2">
    {tr('RP2354B用UF2をダウンロード', 'Download RP2354B UF2')}
  </a>

  {#if error}<p class="text-destructive">{error}</p>{/if}
</section>
