<template>
  <div class="setting-page h-full flex">
    <div
      class="setting-menu bg-white flex flex-col separator-line-right"
      :style="{ width: `${sidebarWidth}px` }"
    >
      <div
        v-for="item in settingRoutes"
        :key="item.name"
        class="h-[48px] menu-list separator-line-bottom flex items-center px-[16px] cursor-pointer gap-[8px]"
        :class="{ 'active-menu': item.name === activeMenuName }"
        @click="handleClickMenu(item)"
      >
        <n-icon :size="16">
          <component :is="getMenuIcon(item.meta?.icon as string)" />
        </n-icon>
        <span class="text-[14px]">{{ menuTitle(item.meta?.title) }}</span>
      </div>
    </div>
    <div class="flex-1 overflow-hidden">
      <RouterView />
    </div>
  </div>
</template>

<script lang="ts" setup>
  import { computed } from 'vue';
  import { settingRoutes } from '@/routes';
  import { useRoute, useRouter } from 'vue-router';
  import { useI18n } from 'vue-i18n';
  import { translateRouteTitle } from '@/manager/i18n';
  import {
    DesktopOutline,
    FlameOutline,
    GlobeOutline,
    ImageOutline,
    ThermometerOutline,
  } from '@vicons/ionicons5';
  import { themeTokens } from '@/theme/designSetting';

  const sidebarWidth = themeTokens.layout.sidebarWidth;
  const { locale } = useI18n({ useScope: 'global' });
  const route = useRoute();
  const router = useRouter();

  const activeMenuName = computed(() => {
    return route.meta.parentMenu || route.name;
  });

  const menuTitle = (title: unknown) => {
    void locale.value;
    return translateRouteTitle(title);
  };

  const iconMap: Record<string, any> = {
    SystemOutlined: DesktopOutline,
    NetworkOutlined: GlobeOutline,
    ThermalOutlined: ThermometerOutline,
    FlameOutlined: FlameOutline,
    ImageOutlined: ImageOutline,
  };

  const getMenuIcon = (iconName: string) => {
    return iconMap[iconName] || DesktopOutline;
  };

  const handleClickMenu = (item: any) => {
    router.push({ name: item.name });
  };
</script>

<style lang="less" scoped>
  .menu-list {
    color: var(--color-text-muted);
    transition: all 0.2s;

    &:hover {
      color: var(--color-primary);
      background: rgba(15, 118, 110, 0.06);
    }

    &.active-menu {
      color: var(--color-primary);
      background: rgba(15, 118, 110, 0.1);
      font-weight: 600;
    }
  }
</style>
