<script lang="ts">
	import { Label } from '$lib/components/ui/label';
	import { Switch } from '$lib/components/ui/switch';
	import HelpText from '$lib/HelpText.svelte';

	interface Props {
		label: string;
		checked: boolean;
		onchange?: (checked: boolean) => void;
		children?: import('svelte').Snippet;
		helpWrap?: boolean;
		disabled?: boolean;
	}

	let { label, checked = $bindable(), onchange, children, helpWrap = false, disabled = false }: Props = $props();
</script>

<div class="mb-4 flex items-center justify-between gap-4 rounded-lg bg-muted/40 px-3 py-3">
	<div class="flex min-w-0 items-center gap-2">
		<Label for={label} class="min-w-0 leading-relaxed">{label}</Label>
		{#if children}<HelpText {label} wrap={helpWrap} sideOffset={8}>{@render children()}</HelpText>{/if}
	</div>
	<div class="flex shrink-0 items-center gap-2">
		<span aria-hidden="true" class="text-xs font-medium text-muted-foreground">{checked ? 'ON' : 'OFF'}</span>
		<Switch id={label} {disabled} bind:checked onCheckedChange={onchange} />
	</div>
</div>
