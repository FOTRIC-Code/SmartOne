<template>
  <div class="h-full w-full flex flex-col">
    <h2 class="setting-page-title">{{ sectionTitle }}</h2>
    <div class="tab bg-white pl-[40px] pr-[20px] h-[48px] flex items-end">
      <n-tabs type="line" animated v-model:value="tabActive" :key="String(locale)">
        <n-tab
          :name="item.name"
          :tab="translateRouteTitle(item.meta.title)"
          v-for="item in tabs"
          :key="`${String(locale)}-${String(item.name)}`"
          @click="handleClickTabItem(item)"
        />
      </n-tabs>
    </div>
    <div class="flex-1 overflow-hidden">
      <RouterView />
    </div>
  </div>
</template>

<script lang="ts" setup>
  import { settingRoutes } from '@/routes';
  import { computed, ref, watch } from 'vue';
  import { useRoute, useRouter } from 'vue-router';
  import { useI18n } from 'vue-i18n';
  import { translateRouteTitle } from '@/manager/i18n';

  const { locale } = useI18n({ useScope: 'global' });
  const route = useRoute();
  const router = useRouter();
  const tabActive = ref<string>(route.name as string);

  const parentRoute = computed(() => {
    const parentMenu = route.meta.parentMenu;
    return settingRoutes.find((v) => v.name === parentMenu);
  });

  const sectionTitle = computed(() => {
    void locale.value;
    return translateRouteTitle(parentRoute.value?.meta?.title);
  });

  const tabs = computed(() => parentRoute.value?.children || []);

  watch(
    () => route.name,
    (name) => {
      if (name) tabActive.value = name as string;
    }
  );

  const handleClickTabItem = (item: any) => {
    router.push({ name: item.name });
  };
</script>

<style lang="less" scoped>
  /* 与用户管理/媒体等页标题对齐：上 20px、左 40px、18px 加粗、行高 26px */
  .setting-page-title {
    margin: 0;
    padding: 20px 20px 0 40px;
    font-size: 18px;
    font-weight: 700;
    line-height: 26px;
    background: #fff;
  }
</style>
