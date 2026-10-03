<script lang="ts">
  import { Label } from '$lib/components/ui/label';
  import HelpText from '$lib/HelpText.svelte';
  let { forId, label, children, wrap = false }: { forId: string; label: string; children?: import('svelte').Snippet; wrap?: boolean } = $props();
</script>
<div class="flex items-center gap-2">
  <Label for={forId}>{label}</Label>
  <HelpText {label} {wrap}>{@render children?.()}</HelpText>
</div>
