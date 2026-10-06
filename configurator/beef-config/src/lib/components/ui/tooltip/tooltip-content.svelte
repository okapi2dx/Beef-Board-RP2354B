<script lang="ts">
	import { Tooltip as TooltipPrimitive } from "bits-ui";
	import { cn } from "$lib/utils.js";

	let {
		ref = $bindable(null),
		class: className,
		sideOffset = 0,
		side = "top",
		children,
		arrowClasses,
		...restProps
	}: TooltipPrimitive.ContentProps & {
		arrowClasses?: string;
	} = $props();
</script>

<TooltipPrimitive.Portal>
	<TooltipPrimitive.Content
		bind:ref
		data-slot="tooltip-content"
		{sideOffset}
		{side}
		class={cn(
			"animate-in fade-in-0 zoom-in-95 data-[state=closed]:animate-out data-[state=closed]:fade-out-0 data-[state=closed]:zoom-out-95 data-[side=bottom]:slide-in-from-top-2 data-[side=left]:slide-in-from-right-2 data-[side=right]:slide-in-from-left-2 data-[side=top]:slide-in-from-bottom-2 origin-(--bits-tooltip-content-transform-origin) z-50 w-fit text-balance rounded-md px-3 py-1.5 text-xs",
			className
		)}
		{...restProps}
	>
		{@render children?.()}
		<TooltipPrimitive.Arrow>
			{#snippet child({ props })}
				<div
					data-slot="tooltip-arrow"
					class={cn(
						"z-50 size-2.5 rotate-45 rounded-[2px]",
						side === "top" && "translate-x-1/2 translate-y-[calc(-50%+2px)]",
						side === "bottom" && "-translate-x-1/2 -translate-y-[calc(-50%+1px)]",
						side === "right" && "translate-x-[calc(50%+2px)] translate-y-1/2",
						side === "left" && "-translate-y-[calc(50%-3px)]",
						arrowClasses
					)}
					{...props}
				></div>
			{/snippet}
		</TooltipPrimitive.Arrow>
	</TooltipPrimitive.Content>
</TooltipPrimitive.Portal>

<style>
	:global([data-slot="tooltip-content"]) {
		background: var(--tooltip-background);
		color: var(--tooltip-foreground);
		border: 1px solid var(--tooltip-border);
		box-shadow: var(--tooltip-shadow);
	}
	:global([data-slot="tooltip-arrow"]) {
		background: var(--tooltip-background);
	}
</style>
