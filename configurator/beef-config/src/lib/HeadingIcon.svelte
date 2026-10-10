<script lang="ts">
  import { Disc3, Sun, Palette, SlidersHorizontal, Keyboard, ArrowLeftRight, Gamepad2, Activity, Cpu, List } from '@lucide/svelte';
  const icons = { turntable: Disc3, led: Sun, rgb: Palette, adjust: SlidersHorizontal, keys: Keyboard, remap: ArrowLeftRight, monitor: Gamepad2, chatter: Activity, firmware: Cpu, board: Cpu, log: List };
  let { kind }: { kind: keyof typeof icons } = $props();
  const Icon = $derived(icons[kind]);
</script>
<span class="panel-heading-icon" aria-hidden="true"><Icon size={15} strokeWidth={1.8} /></span>
