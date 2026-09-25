<script lang="ts">
	import { Label } from '$lib/components/ui/label';
	import { Switch } from '$lib/components/ui/switch';
	import HelpText from '$lib/HelpText.svelte';

	interface Props {
		label: string;
		checked: boolean;
		children?: import('svelte').Snippet;
	}

	let { label, checked = $bindable(), children }: Props = $props();
</script>

<div class="mb-4 flex items-center justify-between gap-4 rounded-lg bg-muted/40 px-3 py-3">
	<div class="flex min-w-0 items-center gap-2">
		<Label for={label} class="min-w-0 leading-relaxed">{label}</Label>
		{#if children}<HelpText {label}>{@render children()}</HelpText>{/if}
	</div>
	<div class="flex shrink-0 items-center gap-2">
		<span aria-hidden="true" class="text-xs font-medium text-muted-foreground">{checked ? 'ON' : 'OFF'}</span>
		<Switch id={label} bind:checked />
	</div>
</div>
