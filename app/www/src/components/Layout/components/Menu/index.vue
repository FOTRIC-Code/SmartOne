<template>
  <NMenu
    :key="menuKey"
    :options="menus"
    :inverted="inverted"
    :mode="mode"
    :collapsed="collapsed"
    :collapsed-width="themeTokens.layout.sidebarCollapsedWidth"
    :collapsed-icon-size="20"
    :indent="24"
    :expanded-keys="openKeys"
    :value="getSelectedKeys"
    @update:value="clickMenuItem"
    @update:expanded-keys="menuExpanded"
  />
</template>

<script lang="ts">
  import { defineComponent, ref, onMounted, reactive, computed, watch, toRefs, unref } from 'vue';
  import { useRoute, useRouter } from 'vue-router';
  import { useI18n } from 'vue-i18n';
  import { useAsyncRouteStore } from '@/data/stores/asyncRoute';
  import { useDesignSettingStore } from '@/data/stores/designSetting';
  import { generatorMenu, generatorMenuMix } from '@/utils';
  import { useProjectSettingStore } from '@/data/stores/projectSetting';
  import { useProjectSetting } from '@/data/stores/projectSetting';
  import { themeTokens } from '@/theme/designSetting';
  import { translateRouteTitle } from '@/manager/i18n';

  /** 只保留 NMenu 需要的字段，避免 route.meta.title 等原始 key 被误显示 */
  function toNaiveMenuOptions(items: any[] | null): any[] {
    if (!items?.length) return [];
    return items.map((item) => {
      const titleKey = item.meta?.title ?? item.title ?? item.label;
      const option: Record<string, unknown> = {
        key: item.key ?? item.name,
        label: translateRouteTitle(titleKey),
      };
      if (item.icon) option.icon = item.icon;
      if (item.children?.length) {
        option.children = toNaiveMenuOptions(item.children);
      }
      return option;
    });
  }

  export default defineComponent({
    name: 'AppMenu',
    components: {},
    props: {
      mode: {
        type: String,
        default: 'vertical',
      },
      collapsed: {
        type: Boolean,
      },
      location: {
        type: String,
        default: 'left',
      },
    },
    emits: ['update:collapsed', 'clickMenuItem'],
    setup(props, { emit }) {
      const currentRoute = useRoute();
      const router = useRouter();
      const { locale } = useI18n({ useScope: 'global' });
      const designStore = useDesignSettingStore();
      const asyncRouteStore = useAsyncRouteStore();
      const settingStore = useProjectSettingStore();
      const menus = ref<any[]>([]);
      const selectedKeys = ref<string>(currentRoute.name as string);
      const headerMenuSelectKey = ref<string>('');

      const { navMode } = useProjectSetting();

      const matched = currentRoute.matched;
      const getOpenKeys = matched && matched.length ? matched.map((item) => item.name) : [];

      const state = reactive({
        openKeys: getOpenKeys,
      });

      const menuKey = computed(
        () => `${String(locale.value)}-${String(designStore.locale)}`,
      );

      const inverted = computed(() => {
        return ['dark', 'header-dark'].includes(settingStore.navTheme);
      });

      const getSelectedKeys = computed(() => {
        let location = props.location;
        return location === 'left' || (location === 'header' && unref(navMode) === 'horizontal')
          ? unref(selectedKeys)
          : unref(headerMenuSelectKey);
      });

      watch(
        () => settingStore.menuSetting.mixMenu,
        () => {
          updateMenu();
          if (props.collapsed) {
            emit('update:collapsed', !props.collapsed);
          }
        }
      );

      watch(
        () => currentRoute.fullPath,
        () => {
          updateMenu();
        }
      );

      watch(
        () => asyncRouteStore.menus,
        () => {
          updateMenu();
        },
        { deep: true }
      );

      watch([locale, () => designStore.locale], () => {
        updateMenu();
      });

      function updateSelectedKeys() {
        const matched = currentRoute.matched;
        state.openKeys = matched.map((item) => item.name);
        const activeMenu: string = (currentRoute.meta?.activeMenu as string) || '';
        selectedKeys.value = activeMenu ? (activeMenu as string) : (currentRoute.name as string);
      }

      function updateMenu() {
        let raw: any[] | null = null;
        if (!settingStore.menuSetting.mixMenu) {
          raw = generatorMenu(asyncRouteStore.menus);
        } else {
          const firstRouteName: string = (currentRoute.matched[0].name as string) || '';
          raw = generatorMenuMix(asyncRouteStore.menus, firstRouteName, props.location);
          const activeMenu: string = currentRoute?.matched[0].meta?.activeMenu as string;
          headerMenuSelectKey.value = (activeMenu ? activeMenu : firstRouteName) || '';
        }
        menus.value = toNaiveMenuOptions(raw);
        updateSelectedKeys();
      }

      function clickMenuItem(key: string) {
        if (/http(s)?:/.test(key)) {
          window.open(key);
        } else {
          router.push({ name: key });
        }
        emit('clickMenuItem' as any, key);
      }

      function menuExpanded(openKeys: string[]) {
        if (!openKeys) return;
        const latestOpenKey = openKeys.find((key) => state.openKeys.indexOf(key) === -1);
        const isExistChildren = findChildrenLen(latestOpenKey as string);
        state.openKeys = isExistChildren ? (latestOpenKey ? [latestOpenKey] : []) : openKeys;
      }

      function findChildrenLen(key: string) {
        if (!key) return false;
        const subRouteChildren: string[] = [];
        for (const { children, key } of unref(menus)) {
          if (children && children.length) {
            subRouteChildren.push(key as string);
          }
        }
        return subRouteChildren.includes(key);
      }

      onMounted(() => {
        updateMenu();
      });

      return {
        ...toRefs(state),
        inverted,
        menus,
        menuKey,
        selectedKeys,
        headerMenuSelectKey,
        getSelectedKeys,
        clickMenuItem,
        menuExpanded,
        themeTokens,
        locale,
      };
    },
  });
</script>
