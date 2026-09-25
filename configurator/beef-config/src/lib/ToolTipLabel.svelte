<script lang="ts">
  import { Label } from '$lib/components/ui/label';
  import HelpText from '$lib/HelpText.svelte';
  let { forId, label, children }: { forId: string; label: string; children?: import('svelte').Snippet } = $props();
</script>
<div class="flex items-center gap-2">
  <Label for={forId}>{label}</Label>
  <HelpText {label}>{@render children?.()}</HelpText>
</div>
