<template>
  <div class="text-[14px] font-semibold mt-[6px] mb-[8px]">{{ $t('smart.linkage') }}</div>
  <n-form-item :label="$t('smart.triggerFtp')">
    <n-switch
      :value="ftpEnabled"
      @update:value="$emit('update:ftpEnabled', $event)"
    />
  </n-form-item>
  <n-form-item :label="$t('smart.triggerWebhook')">
    <n-switch
      :value="webhookEnabled"
      @update:value="$emit('update:webhookEnabled', $event)"
    />
  </n-form-item>
  <n-form-item :label="$t('smart.triggerRecord')">
    <n-switch
      :value="recordEnabled"
      @update:value="$emit('update:recordEnabled', $event)"
    />
  </n-form-item>
</template>

<script lang="ts" setup>
  defineProps<{
    ftpEnabled: boolean;
    webhookEnabled: boolean;
    recordEnabled: boolean;
  }>();

  defineEmits<{
    (event: 'update:ftpEnabled', value: boolean): void;
    (event: 'update:webhookEnabled', value: boolean): void;
    (event: 'update:recordEnabled', value: boolean): void;
  }>();
</script>
