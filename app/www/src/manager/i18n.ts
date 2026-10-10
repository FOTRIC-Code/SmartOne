import { LOCALE_I18N } from '@/data/storageKeys';
import { createI18n } from 'vue-i18n';

const modules = import.meta.glob<{ default: Record<string, unknown> }>(
  '/src/resource/i18n/**/*.ts',
  { eager: true }
);

const messages: Record<string, Record<string, Record<string, unknown>>> = {};

for (const [path, mod] of Object.entries(modules)) {
  const segments = path.replace(/\\/g, '/').split('/');
  const lang = segments[segments.length - 2];
  const moduleName = segments[segments.length - 1].replace(/\.[^.]+$/, '');
  if (!messages[lang]) messages[lang] = {};
  messages[lang][moduleName] = { ...(mod.default || {}) } as Record<string, unknown>;
}

/** 按浏览器/系统语言：中文 → zh-CN，其它 → en-US */
export function detectBrowserLocale(): 'zh-CN' | 'en-US' {
  const candidates = [
    navigator.language,
    ...(Array.isArray(navigator.languages) ? navigator.languages : []),
  ]
    .filter(Boolean)
    .map((lang) => String(lang).toLowerCase());

  const primary = candidates[0] || '';
  return primary.startsWith('zh') ? 'zh-CN' : 'en-US';
}

/** 已保存语言优先（含登录页手动切换）；否则按系统语言 */
export function resolveLocale(): string {
  const stored = sessionStorage.getItem(LOCALE_I18N);
  if (stored === 'zh-CN' || stored === 'en-US') return stored;
  const locale = detectBrowserLocale();
  sessionStorage.setItem(LOCALE_I18N, locale);
  return locale;
}

const i18n = createI18n({
  legacy: false,
  globalInjection: true,
  locale: resolveLocale(),
  fallbackLocale: 'en-US',
  sync: true,
  messages,
});

/** 路由 meta.title 存 i18n key（如 route.preview），展示时再翻译 */
export function translateRouteTitle(title: unknown): string {
  if (typeof title !== 'string' || !title) return '';
  if (!title.includes('.')) return title;
  const te = i18n.global.te(title);
  if (!te) {
    // 兼容偶发未就绪：按最后一段从 route 模块取
    const shortKey = title.includes('.') ? title.slice(title.indexOf('.') + 1) : title;
    const locale = String(i18n.global.locale.value);
    const fromModule = (messages as any)?.[locale]?.route?.[shortKey];
    if (typeof fromModule === 'string') return fromModule;
  }
  return String(i18n.global.t(title as any));
}

export default i18n;
