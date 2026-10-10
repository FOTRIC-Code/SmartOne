<template>
  <div class="p-[20px] max-w-[520px]">
    <n-form label-placement="top" :show-require-mark="false">
      <n-form-item :label="$t('smart.enabled')">
        <n-switch v-model:value="form.enabled" />
      </n-form-item>
      <n-form-item :label="$t('smart.noRepeat')">
        <n-input-number v-model:value="form.no_repeat_minutes" :min="0" :max="120" class="w-full" />
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
      <n-form-item :label="$t('smart.recordDuration')">
        <n-radio-group v-model:value="form.record_duration_sec" :disabled="!triggerForm.trigger_record_enabled">
          <n-radio :value="30">{{ $t('smart.sec30') }}</n-radio>
          <n-radio :value="60">{{ $t('smart.sec60') }}</n-radio>
        </n-radio-group>
      </n-form-item>
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
    getIntrusionAlarm,
    setAlarmTrigger,
    setIntrusionAlarm,
    type AlarmTrigger,
    type IntrusionAlarm,
  } from '@/api';
  import { normalizeAlarmRecordDuration, normalizeAlarmSensitivity } from '@/alarm-contract';
  import AlarmLinkageFields from '@/views/setting/smart/components/AlarmLinkageFields.vue';

  const messageApi = useMessage();
  const { t } = useI18n();
  const saving = ref(false);
  const loading = ref(true);
  const form = reactive<IntrusionAlarm>({
    enabled: false,
    no_repeat_minutes: 5,
    sensitivity: 1,
    record_duration_sec: 30,
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
        getIntrusionAlarm(),
        getAlarmTrigger(2),
      ]);
      Object.assign(form, {
        enabled: !!data?.enabled,
        no_repeat_minutes: data?.no_repeat_minutes ?? 5,
        sensitivity: normalizeAlarmSensitivity(data?.sensitivity),
        record_duration_sec: normalizeAlarmRecordDuration(data?.record_duration_sec),
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
      if (!Number.isInteger(form.no_repeat_minutes) || form.no_repeat_minutes < 0) {
        throw new Error(t('common.saveFailed'));
      }
      await Promise.all([
        setIntrusionAlarm({ ...form }),
        setAlarmTrigger(2, { type: 2, ...triggerForm }),
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
