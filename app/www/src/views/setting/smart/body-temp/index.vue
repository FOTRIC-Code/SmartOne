<template>
  <div class="p-[20px] max-w-[520px]">
    <n-form label-placement="top" :show-require-mark="false">
      <n-form-item :label="$t('smart.enabled')">
        <n-switch v-model:value="form.enabled" />
      </n-form-item>
      <n-form-item :label="$t('smart.sensitivity')">
        <n-radio-group v-model:value="form.sensitivity">
          <n-radio :value="0">{{ $t('smart.sensitivityLow') }}</n-radio>
          <n-radio :value="1">{{ $t('smart.sensitivityMid') }}</n-radio>
          <n-radio :value="2">{{ $t('smart.sensitivityHigh') }}</n-radio>
        </n-radio-group>
      </n-form-item>
      <alarm-linkage-fields
        v-model:ftp-enabled="triggerForm.trigger_ftp_enabled"
        v-model:webhook-enabled="triggerForm.trigger_webhook_enabled"
        v-model:record-enabled="triggerForm.trigger_record_enabled"
      />
    </n-form>
    <div class="flex gap-[12px] mt-[8px]">
      <n-button type="primary" :loading="saving" :disabled="loading" @click="save">{{ $t('common.save') }}</n-button>
      <n-button :disabled="loading || saving" @click="load">{{ $t('common.cancel') }}</n-button>
    </div>
  </div>
</template>

<script lang="ts" setup>
  import { onMounted, reactive, ref } from 'vue';
  import { useMessage } from 'naive-ui';
  import { useI18n } from 'vue-i18n';
  import {
    getAlarmTrigger,
    getBodyTempAlarm,
    setAlarmTrigger,
    setBodyTempAlarm,
    type AlarmTrigger,
    type BodyTempAlarm,
  } from '@/api';
  import { normalizeAlarmSensitivity } from '@/alarm-contract';
  import AlarmLinkageFields from '@/views/setting/smart/components/AlarmLinkageFields.vue';

  const messageApi = useMessage();
  const { t } = useI18n();
  const saving = ref(false);
  const loading = ref(true);
  const form = reactive<BodyTempAlarm>({
    enabled: false,
    sensitivity: 1,
  });
  const triggerForm = reactive<AlarmTrigger>({
    trigger_ftp_enabled: false,
    trigger_webhook_enabled: false,
    trigger_record_enabled: true,
  });

  async function load() {
    loading.value = true;
    try {
      const [data, trigger] = await Promise.all([
        getBodyTempAlarm(),
        getAlarmTrigger(4),
      ]);
      Object.assign(form, {
        enabled: !!data?.enabled,
        sensitivity: normalizeAlarmSensitivity(data?.sensitivity),
      });
      Object.assign(triggerForm, {
        trigger_ftp_enabled: !!trigger?.trigger_ftp_enabled,
        trigger_webhook_enabled: !!trigger?.trigger_webhook_enabled,
        trigger_record_enabled: trigger?.trigger_record_enabled !== false,
      });
    } catch (e: any) {
      messageApi.error(e?.message || t('common.fetchFailed'));
    } finally {
      loading.value = false;
    }
  }

  async function save() {
    saving.value = true;
    try {
      await Promise.all([
        setBodyTempAlarm({ ...form }),
        setAlarmTrigger(4, { type: 4, ...triggerForm }),
      ]);
      messageApi.success(t('common.success'));
    } catch (e: any) {
      messageApi.error(e?.message || t('common.saveFailed'));
    } finally {
      saving.value = false;
    }
  }

  onMounted(load);
</script>
