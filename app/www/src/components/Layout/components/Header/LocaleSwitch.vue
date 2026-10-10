<template>
  <n-popover trigger="hover" :show-arrow="false" style="padding: 0" placement="bottom-end">
    <template #trigger>
      <n-button text :focusable="false" class="locale-trigger">
        <img src="@/resource/images/global.svg" class="w-[14px] h-[14px]" />
        <span class="ml-[6px] text-[14px] text-[var(--color-text-muted)]">{{ localeLabel }}</span>
        <n-icon size="14" class="ml-[4px] text-[var(--color-text-muted)]">
          <ChevronDownOutline />
        </n-icon>
      </n-button>
    </template>
    <div class="flex flex-col">
      <div class="py-[10px] px-[30px] separator-line-bottom">
        <n-button text :focusable="false" @click="handleChangeLocale('zh-CN')">
          {{ t('common.simpleChinese') }}
        </n-button>
      </div>
      <div class="py-[10px] px-[30px]">
        <n-button text :focusable="false" @click="handleChangeLocale('en-US')">
          {{ t('common.english') }}
        </n-button>
      </div>
    </div>
  </n-popover>
</template>

<script lang="ts" setup>
  import { computed } from 'vue';
  import { useI18n } from 'vue-i18n';
  import { ChevronDownOutline } from '@vicons/ionicons5';
  import { useDesignSettingStore } from '@/data/stores/designSetting';

  const { t } = useI18n();
  const designSettingStore = useDesignSettingStore();

  const localeLabel = computed(() =>
    designSettingStore.locale === 'zh-CN' ? t('common.simpleChinese') : t('common.english'),
  );

  const handleChangeLocale = (value: string) => {
    designSettingStore.setLocale(value);
  };
</script>

<style lang="less" scoped>
  .locale-trigger {
    display: flex;
    align-items: center;
  }
</style>
