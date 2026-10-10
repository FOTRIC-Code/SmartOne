<template>
  <n-layout class="layout" :position="fixedMenu" has-sider>
    <n-layout :inverted="inverted">
      <n-layout-header :inverted="getHeaderInverted" :position="fixedHeader">
        <PageHeader v-model:collapsed="collapsed" :inverted="inverted" />
      </n-layout-header>
      <div
        class="flex-1 overflow-hidden"
        :class="{ 'layout-default-background': getDarkTheme === false }"
      >
        <MainView />
      </div>
    </n-layout>
  </n-layout>
</template>

<script lang="ts" setup>
  import { ref, unref, computed, onMounted } from 'vue';
  import { MainView } from './components/Main';
  import { PageHeader } from './components/Header';
  import { useProjectSetting } from '@/data/stores/projectSetting';
  import { useDesignSetting } from '@/data/stores/designSetting';
  import { useProjectSettingStore } from '@/data/stores/projectSetting';

  const { getDarkTheme } = useDesignSetting();
  const { navTheme, headerSetting, menuSetting } = useProjectSetting();

  const settingStore = useProjectSettingStore();
  const collapsed = ref<boolean>(false);
  const { mobileWidth } = unref(menuSetting);

  const isMobile = computed<boolean>({
    get: () => settingStore.getIsMobile,
    set: (val) => settingStore.setIsMobile(val),
  });

  const fixedHeader = computed(() => {
    const { fixed } = unref(headerSetting);
    return fixed ? 'absolute' : 'static';
  });

  const fixedMenu = computed(() => {
    const { fixed } = unref(headerSetting);
    return fixed ? 'absolute' : 'static';
  });

  const inverted = computed(() => {
    return ['dark', 'header-dark'].includes(unref(navTheme));
  });

  const getHeaderInverted = computed(() => {
    return ['light', 'header-dark'].includes(unref(navTheme)) ? unref(inverted) : !unref(inverted);
  });

  const checkMobileMode = () => {
    isMobile.value = document.body.clientWidth <= mobileWidth;
    collapsed.value = false;
  };

  const watchWidth = () => {
    collapsed.value = document.body.clientWidth <= 950;
    checkMobileMode();
  };

  onMounted(() => {
    checkMobileMode();
    window.addEventListener('resize', watchWidth);
  });
</script>

<style lang="less">
  .layout-side-drawer {
    background-color: rgb(0, 20, 40);

    .layout-sider {
      min-height: 100vh;
      position: relative;
      z-index: 13;
      transition: all 0.2s ease-in-out;
    }
  }
  .n-layout-scroll-container {
    display: flex;
    flex-direction: column;
  }
</style>
<style lang="less" scoped>
  .layout {
    display: flex;
    flex-direction: row;
    flex: auto;

    &-default-background {
      background: var(--color-page-bg);
    }

    .layout-sider {
      min-height: 100vh;
      position: relative;
      z-index: 13;
      transition: all 0.2s ease-in-out;
    }

    .layout-sider-fix {
      position: fixed;
      top: 0;
      left: 0;
    }

    .ant-layout {
      overflow: hidden;
    }

    .layout-right-fix {
      overflow-x: hidden;
      padding-left: var(--sidebar-width);
      min-height: 100vh;
      transition: all 0.2s ease-in-out;
    }

    .layout-content {
      flex: auto;
    }

    .n-layout-header.n-layout-header--absolute-positioned {
      z-index: 11;
      position: relative;
    }

    .n-layout-footer {
      background: none;
    }
  }

  .layout-content-main {
    margin: 0 var(--content-margin) var(--content-margin);
    position: relative;
    padding-top: var(--header-height);
  }

  .layout-content-main-fix {
    padding-top: var(--header-height);
  }

  .fluid-header {
    padding-top: 0;
  }

  .main-view-fix {
    padding-top: calc(var(--tab-bar-height) + 12px);
  }

  .noMultiTabs {
    padding-top: 0;
  }
</style>
