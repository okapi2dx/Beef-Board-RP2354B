<script lang="ts">
  import * as Tooltip from '$lib/components/ui/tooltip';
  import { tr } from '$lib/types/locale.svelte';
  let { children, label = '' }: { children: import('svelte').Snippet; label?: string } = $props();
</script>
<Tooltip.Provider delayDuration={150}>
  <Tooltip.Root>
    <Tooltip.Trigger
      class="inline-flex h-5 w-5 shrink-0 items-center justify-center rounded-full border border-primary/60 bg-primary/10 text-xs font-bold text-primary shadow-sm transition-colors hover:border-primary hover:bg-primary hover:text-primary-foreground focus-visible:outline-2 focus-visible:outline-offset-2 focus-visible:outline-primary"
      aria-label={tr(label + 'の説明', 'Help: ' + label)}
    >?</Tooltip.Trigger>
    <Tooltip.Content class="z-50 w-[min(28rem,calc(100vw-2rem))] max-w-none p-3 text-left text-sm leading-6">
      <div class="help-text-body">
        {@render children()}
      </div>
    </Tooltip.Content>
  </Tooltip.Root>
</Tooltip.Provider>

<style>
  :global(.help-text-body) {
    line-break: strict;
    word-break: normal;
    overflow-wrap: normal;
    text-wrap: nowrap;
  }
  :global(.help-text-body p) {
    margin: 0;
    white-space: nowrap;
  }
  :global(.help-text-body p + p) {
    margin-top: 0.5rem;
  }
</style>
