<template>
  <NConfigProvider
    :locale="_locale"
    :theme="getDarkTheme"
    :theme-overrides="getThemeOverrides"
    :date-locale="_dateZhCN"
  >
    <AppProvider>
      <RouterView />
    </AppProvider>
  </NConfigProvider>
</template>

<script lang="ts" setup>
  import { computed, watchEffect } from 'vue';
  import { zhCN, dateZhCN, enUS, dateEnUS, darkTheme } from 'naive-ui';
  import { AppProvider } from '@/components/Application';
  import { useDesignSettingStore } from '@/data/stores/designSetting';
  import { storeToRefs } from 'pinia';
  import { generateNaiveOverrides, themeTokens } from '@/theme/designSetting';
  import { applyThemeVariables } from '@/theme/applyThemeVariables';
  import { lighten } from '@/utils/index';

  const designStore = useDesignSettingStore();
  const { locale } = storeToRefs(designStore);

  const _locale = computed(() => {
    return locale.value === 'zh-CN' ? zhCN : enUS;
  });
  const _dateZhCN = computed(() => {
    return locale.value === 'zh-CN' ? dateZhCN : dateEnUS;
  });

  const getThemeOverrides = computed(() => {
    const tokens = { ...themeTokens };
    tokens.colors = {
      ...tokens.colors,
      primary: designStore.appTheme,
      primaryHover: lighten(designStore.appTheme, 6),
      primaryPressed: lighten(designStore.appTheme, 6),
    };
    return generateNaiveOverrides(tokens);
  });

  const getDarkTheme = computed(() => (designStore.darkTheme ? darkTheme : undefined));

  watchEffect(() => {
    const tokens = { ...themeTokens };
    tokens.colors = {
      ...tokens.colors,
      primary: designStore.appTheme,
      primaryHover: lighten(designStore.appTheme, 6),
      primaryPressed: lighten(designStore.appTheme, 6),
    };
    applyThemeVariables(tokens);
  });
</script>
