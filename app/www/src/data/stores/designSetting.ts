import { defineStore } from 'pinia';
import { computed } from 'vue';
import designSetting, { themeTokens, type ThemeTokens } from '@/theme/designSetting';
import i18n, { resolveLocale } from '@/manager/i18n';
import { LOCALE_I18N } from '@/data/storageKeys';
import { lighten } from '@/utils/index';

const { darkTheme, appTheme, appThemeList } = designSetting;

interface DesignSettingState {
  darkTheme: boolean;
  appTheme: string;
  appThemeList: string[];
  locale: any;
}

export const useDesignSettingStore = defineStore({
  id: 'app-design-setting',
  state: (): DesignSettingState => ({
    darkTheme,
    appTheme,
    appThemeList,
    locale: resolveLocale(),
  }),
  getters: {
    getDarkTheme(): boolean {
      return this.darkTheme;
    },
    getAppTheme(): string {
      return this.appTheme;
    },
    getAppThemeList(): string[] {
      return this.appThemeList;
    },
    getThemeTokens(): ThemeTokens {
      return {
        ...themeTokens,
        colors: {
          ...themeTokens.colors,
          primary: this.appTheme,
          primaryHover: lighten(this.appTheme, 6),
          primaryPressed: lighten(this.appTheme, 6),
        },
      };
    },
  },
  actions: {
    setLocale(value): void {
      if (!value) return;
      this.locale = value;
      sessionStorage.setItem(LOCALE_I18N, value);
      // vue-i18n legacy:false 下 locale 为 Ref
      const i18nLocale = i18n.global.locale as { value: string } | string;
      if (typeof i18nLocale === 'object' && i18nLocale) {
        i18nLocale.value = value;
      } else {
        (i18n.global as any).locale = value;
      }
    },
  },
});

/**
 * 在 setup 内使用，返回设计设置相关的 computed 引用集合
 */
export function useDesignSetting() {
  const designStore = useDesignSettingStore();

  const getDarkTheme = computed(() => designStore.darkTheme);
  const getAppTheme = computed(() => designStore.appTheme);
  const getAppThemeList = computed(() => designStore.appThemeList);

  return {
    getDarkTheme,
    getAppTheme,
    getAppThemeList,
  };
}
