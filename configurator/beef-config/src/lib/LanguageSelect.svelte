<script lang="ts">
  import { Label } from '$lib/components/ui/label';
  import * as Select from '$lib/components/ui/select';
  import { locale, setLocale, tr, type Locale } from '$lib/types/locale.svelte';
</script>

<div class="flex items-center gap-2">
  <Label for="language">{tr('言語', 'Language')}</Label>
  <Select.Root type="single" value={locale.current} onValueChange={(value) => value && setLocale(value as Locale)}>
    <Select.Trigger id="language" class="w-[130px]">{locale.current === 'ja' ? '日本語' : 'English'}</Select.Trigger>
    <Select.Content>
      <Select.Item value="ja">日本語</Select.Item>
      <Select.Item value="en">English</Select.Item>
    </Select.Content>
  </Select.Root>
</div>
