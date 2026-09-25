<script lang="ts">
  import { readDiagnostics, parseDiagnostics, ReportId } from '$lib/types/hid';
  import { appState } from '$lib/types/state.svelte';
  import { tr } from '$lib/types/locale.svelte';
  let { supported, digital, active, streaming = false }: { supported: boolean; digital: boolean; active: boolean; streaming?: boolean } = $props();
  let value = $state<number | null>(null);
  let sensorAB = $state<number | null>(null);
  const phaseA = $derived(sensorAB === null ? null : (sensorAB >> 1) & 1);
  const phaseB = $derived(sensorAB === null ? null : sensorAB & 1);
  $effect(() => {
    if (!active || !supported) return;
    let disposed = false;
    let timer: ReturnType<typeof setTimeout> | undefined;
    const device = appState.device;
    if (!device) return;

    if (streaming) {
      const onInput = (event: HIDInputReportEvent) => {
        if (event.reportId !== ReportId.DiagnosticsStream) return;
        try {
          const report = parseDiagnostics(event.data);
          value = report.output;
          sensorAB = report.sensorAB;
        } catch { /* Ignore malformed packets. */ }
      };
      device.addEventListener('inputreport', onInput);
      return () => device.removeEventListener('inputreport', onInput);
    }

    async function poll() {
      if (disposed || !supported || !device || appState.device !== device) return;
      try {
        const report = await readDiagnostics();
        if (disposed || appState.device !== device) return;
        // Firmware output after deadzone, hold extrapolation and delay.
        value = report.output;
        sensorAB = report.sensorAB;
      } catch {
        if (disposed) return;
        // Config writes can briefly overlap a diagnostics read. Keep the last
        // value visible and resume polling after the write has completed.
        if (appState.device === device) timer = setTimeout(poll, 100);
        return;
      }
      timer = setTimeout(poll, 33);
    }
    void poll();
    return () => { disposed = true; clearTimeout(timer); };
  });
</script>

<section class="mb-5 rounded-lg border p-3" aria-label={tr('ターンテーブルテスト', 'Turntable Test')}>
  <div class="mb-3 flex items-center justify-between gap-3">
    <span class="font-medium">{tr('ターンテーブルテスト', 'Turntable Test')}</span>
    <div class="flex items-center gap-4">
      <output data-testid="turntable-phases" class="font-mono text-sm tabular-nums">
        <span class="text-muted-foreground">{tr('A/B相', 'A/B phase')}</span>
        {phaseA ?? '—'}/{phaseB ?? '—'}
      </output>
      <output data-testid="turntable-value" class="font-mono text-lg tabular-nums">{value ?? '—'} <span class="text-sm text-muted-foreground">/ 255</span></output>
    </div>
  </div>
  <div role="meter" aria-label={tr('設定反映後のX軸', 'X axis after all settings')} aria-valuemin={0} aria-valuemax={255} aria-valuenow={value ?? undefined} aria-valuetext={value === null ? tr('未取得', 'Unavailable') : String(value)} class="relative mx-2 h-2 rounded-full bg-muted">
    <div class="h-full rounded-full bg-primary" style:width={`${(value ?? 0) / 255 * 100}%`}></div>
    {#if value !== null}<div class="absolute top-1/2 h-4 w-4 -translate-x-1/2 -translate-y-1/2 rounded-full border-2 border-background bg-primary shadow" style:left={`${value / 255 * 100}%`}></div>{/if}
  </div>
  <div class="mt-2 flex justify-between text-xs text-muted-foreground"><span>0</span><span>128</span><span>255</span></div>
  <div class="mt-2 text-sm text-muted-foreground">
    <p>{tr('アナログ設定では、各設定を反映したX軸値を表示します。', 'In analog mode, this shows the X-axis value after applying the settings.')}</p>
    <p>{tr('停止後は、設定した保持時間だけ最後の方向へ入力を続けます。', 'After rotation stops, input continues in the last direction for the configured hold time.')}</p>
    <p>{tr('デジタル設定では、X軸ではなくTT-/TT+をゲームへ送ります。', 'In digital mode, TT-/TT+ is sent to the game instead of the X-axis.')}</p>
  </div>
  {#if digital}<p class="mt-1 text-sm text-muted-foreground">{tr('デジタル設定中は、確認用のアナログX軸値を表示します。ゲームへ送るデジタル入力とは異なります。', 'In digital mode, this displays the analog X-axis preview, not the digital input sent to the game.')}</p>{/if}
  {#if !supported}<p class="mt-2 text-sm">{tr('表示するにはファームウェアV1.00以降へ更新してください。', 'Update to firmware V1.00 or later to enable this display.')}</p>{/if}
</section>


