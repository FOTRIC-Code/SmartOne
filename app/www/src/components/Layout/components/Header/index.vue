<template>
  <div class="layout-main flex items-center separator-line-bottom" :style="{ height: `var(--header-height)` }">
    <HeaderLogo />
    <div class="header-menu" :style="{ height: `var(--header-height)` }">
      <AsideMenu :inverted="getInverted" mode="horizontal" />
    </div>
    <div class="flex-1"></div>
    <LocaleSwitch class="mr-[16px]" />
    <NetworkStatus class="mr-[16px]" />
  </div>
</template>

<script lang="ts" setup>
  import { AsideMenu } from '@/components/Layout/components/Menu';
  import HeaderLogo from './HeaderLogo.vue';
  import LocaleSwitch from './LocaleSwitch.vue';
  import NetworkStatus from './NetworkStatus.vue';
  import { computed, reactive, unref } from 'vue';
  import { useUserStore } from '@/data/stores/user';
  import { useProjectSetting } from '@/data/stores/projectSetting';

  const userStore = useUserStore();
  const { navMode, navTheme, headerSetting, crumbsSetting } = useProjectSetting();
  const props = defineProps(['inverted']);
  const { name } = userStore?.info || {};

  const state = reactive({
    username: name ?? '',
    fullscreenIcon: 'FullscreenOutlined',
    navMode,
    navTheme,
    headerSetting,
    crumbsSetting,
  });
  const getInverted = computed(() => {
    return ['light', 'header-dark'].includes(unref(navTheme)) ? props.inverted : !props.inverted;
  });
</script>

<style lang="less" scoped>
  .header-menu {
    display: flex;
    align-items: stretch;

    :deep(.n-menu.n-menu--horizontal) {
      display: flex;
      align-items: stretch;

      .n-menu-item {
        width: 160px;
        height: 100% !important;
        margin: 0;
      }

      .n-menu-item-content {
        display: flex;
        align-items: center;
        justify-content: center;
        width: 100%;
        height: 100% !important;
        padding: 0;
        border-radius: 0;

        &::before {
          display: none;
        }
      }

      .n-menu-item-content--selected {
        background: var(--color-primary) !important;
        color: var(--color-card-bg) !important;

        .n-menu-item-content__icon,
        .n-menu-item-content-header,
        .n-menu-item-content-header a {
          color: var(--color-card-bg) !important;
        }
      }

      .n-menu-item-content:hover {
        background: transparent;
      }
    }
  }
</style>
