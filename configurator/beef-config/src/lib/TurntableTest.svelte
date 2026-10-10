<script lang="ts">
  import { readDiagnostics, parseDiagnostics, ReportId } from '$lib/types/hid';
  import { appState } from '$lib/types/state.svelte';
  import { tr } from '$lib/types/locale.svelte';
  import HelpText from '$lib/HelpText.svelte';
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

<section class="mb-5 rounded-lg border p-3" aria-label={tr('X軸', 'X axis')}>
  <div class="mb-3 flex items-center justify-between gap-3">
    <div class="flex items-center gap-2">
      <span class="font-medium">{tr('X軸', 'X axis')}</span>
      <HelpText label={tr('X軸', 'X axis')} wrap>
        <p>{tr('感度・デッドゾーンなどを反映したX軸値です。', 'Shows the X-axis value after sensitivity, deadzone and other settings.')}</p>
        <p>{tr('停止後も、設定した保持時間は入力が続きます。', 'Input continues after stopping for the configured hold time.')}</p>
        <p>{digital ? tr('デジタル設定中は確認用の値です。ゲームにはB13/B14を送ります。', 'In digital mode this is a preview; B13/B14 are sent to the game.') : tr('値は0～255で、端に達すると反対側へ戻ります。', 'Values wrap from one end of the 0–255 range to the other.')}</p>
      </HelpText>
    </div>
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
  <div class="axis-scale mx-2 mt-2" aria-hidden="true">
    {#each Array.from({ length: 52 }, (_, i) => i * 5) as tick}
      <span class="axis-tick" class:major={tick % 10 === 0 || tick === 255} style:left={`${tick / 255 * 100}%`}></span>
    {/each}
    {#each [0, 32, 64, 96, 128, 160, 192, 224, 255] as tick}
      <span class="axis-label" class:first={tick === 0} class:last={tick === 255} style:left={`${tick / 255 * 100}%`}>{tick}</span>
    {/each}
  </div>
  {#if !supported}<p class="mt-2 text-sm">{tr('表示するにはファームウェアV1.00以降へ更新してください。', 'Update to firmware V1.00 or later to enable this display.')}</p>{/if}
</section>

<style>
  .axis-scale { position: relative; height: 28px; color: var(--muted-foreground); }
  .axis-tick { position: absolute; top: 0; height: 4px; border-left: 1px solid currentColor; opacity: .55; }
  .axis-tick.major { height: 7px; opacity: .85; }
  .axis-label { position: absolute; top: 10px; transform: translateX(-50%); font-size: 10px; line-height: 1; font-variant-numeric: tabular-nums; }
  .axis-label.first { transform: none; }
  .axis-label.last { transform: translateX(-100%); }
  @media (max-height: 620px) {
    section { padding: 8px; }
    section > .mb-3 { margin-bottom: 8px; }
    .axis-scale { height: 24px; }
  }
</style>


