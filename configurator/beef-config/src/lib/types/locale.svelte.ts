export type Locale = 'ja' | 'en';

class LocaleState {
  current = $state<Locale>('ja');
}

export const locale = new LocaleState();

export function initializeLocale(): void {
  const saved = localStorage.getItem('beef-language');
  locale.current = saved === 'en' || saved === 'ja' ? saved : 'ja';
  document.documentElement.lang = locale.current;
}

export function setLocale(value: Locale): void {
  locale.current = value;
  localStorage.setItem('beef-language', value);
  document.documentElement.lang = value;
}

export function tr(ja: string, en: string): string {
  return locale.current === 'ja' ? ja : en;
}
