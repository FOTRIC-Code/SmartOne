<template>
  <div class="p-[20px]">
    <n-form
      ref="formRef"
      :model="formValue"
      label-placement="left"
      label-width="auto"
      class="w-[500px]"
    >
      <n-form-item :label="$t('setting.timezone')" path="time_zone">
        <n-select v-model:value="formValue.time_zone" :options="timezoneOptions" />
      </n-form-item>

      <n-form-item :label="$t('setting.ntpSync')" path="time_mode">
        <n-switch
          :value="formValue.time_mode === 1"
          @update:value="handleNtpSyncUpdate"
        />
      </n-form-item>

      <n-form-item :label="$t('setting.ntpHostname')" path="host_name">
        <n-input v-model:value="formValue.host_name" :disabled="formValue.time_mode !== 1" />
      </n-form-item>
      <n-form-item :label="$t('setting.NTPPort')" path="port_no">
        <n-input-number v-model:value="formValue.port_no" :disabled="formValue.time_mode !== 1" :min="1" :max="65535" class="w-full" />
      </n-form-item>
      <n-form-item :label="$t('setting.syncInterval')" path="syn_interval">
        <n-input-number v-model:value="formValue.syn_interval" :disabled="formValue.time_mode !== 1" :min="1" class="w-full">
          <template #suffix>{{ $t('setting.second') }}</template>
        </n-input-number>
      </n-form-item>

      <n-form-item :label="$t('setting.manualCheck')">
        <n-switch
          :value="formValue.time_mode === 0"
          @update:value="handleManualSyncUpdate"
        />
      </n-form-item>

      <n-form-item :label="$t('setting.localTime')" path="local_time">
        <n-date-picker
          v-model:value="formValue.local_time"
          :disabled="formValue.time_mode !== 0"
          type="datetime"
          class="w-full"
          clearable
        />
      </n-form-item>
    </n-form>

    <div class="action-bar separator-line-top">
      <n-button type="primary" :loading="loading" @click="handleSave">{{ $t('common.save') }}</n-button>
      <n-button @click="handleCancel">{{ $t('common.cancel') }}</n-button>
    </div>
  </div>
</template>

<script lang="ts" setup>
import { getSystemTime, setSystemTime, getNtpServer, setNtpServer } from '@/api';
import timezoneZhCN from '@/resource/assets/json/timezone_zh-cn.json';
import timezoneEnUS from '@/resource/assets/json/timezone_en-us.json';
import { useMessage } from 'naive-ui';
import { computed, onMounted, ref } from 'vue';
import { useI18n } from 'vue-i18n';

const { t, locale } = useI18n();
const message = useMessage();
const formRef = ref();
const loading = ref(false);

const timezoneOptions = computed(() =>
  locale.value.startsWith('zh') ? timezoneZhCN : timezoneEnUS
);

const formValue = ref({
  time_mode: 0,
  local_time: null as number | null,
  time_zone: '',
  host_name: '',
  port_no: 123,
  syn_interval: 60,
});

const originalData = ref({ ...formValue.value });
/** 设备系统时区，进入页面与 NTP 校时时使用 */
const systemTimezone = ref('');

const padDateTimeUnit = (value: number) => String(value).padStart(2, '0');

const getComputerLocalTimestamp = () => new Date().getTime();

const formatComputerLocalDateTime = (timestamp = getComputerLocalTimestamp()) => {
  const date = new Date(timestamp);
  return `${date.getFullYear()}-${padDateTimeUnit(date.getMonth() + 1)}-${padDateTimeUnit(
    date.getDate()
  )} ${padDateTimeUnit(date.getHours())}:${padDateTimeUnit(date.getMinutes())}:${padDateTimeUnit(
    date.getSeconds()
  )}`;
};

const updateLocalTimeFromComputer = () => {
  formValue.value.local_time = getComputerLocalTimestamp();
};

const updateTimezoneFromSystem = () => {
  if (systemTimezone.value) {
    formValue.value.time_zone = systemTimezone.value;
  }
};

const parseLocalTime = (localTime: string): number => {
  const normalized = localTime.replace(/-/g, '/');
  return new Date(normalized).getTime();
};

const fetchData = async () => {
  const timeRes: any = await getSystemTime();
  systemTimezone.value = timeRes.time_zone || '';
  formValue.value.time_mode = timeRes.time_mode ?? 0;
  formValue.value.time_zone = systemTimezone.value;
  formValue.value.local_time = timeRes.local_time
    ? parseLocalTime(timeRes.local_time)
    : getComputerLocalTimestamp();

  const ntpRes: any = await getNtpServer(1);
  formValue.value.host_name = ntpRes.host_name ?? '';
  formValue.value.port_no = ntpRes.port_no ?? 123;
  formValue.value.syn_interval = ntpRes.syn_interval ?? 60;

  originalData.value = { ...formValue.value };
};

const handleNtpSyncUpdate = (val: boolean) => {
  formValue.value.time_mode = val ? 1 : 0;
  if (!val) {
    updateLocalTimeFromComputer();
  }
};

const handleManualSyncUpdate = (val: boolean) => {
  formValue.value.time_mode = val ? 0 : 1;
  if (val) {
    updateLocalTimeFromComputer();
  }
};

const handleSave = async () => {
  loading.value = true;
  try {
    const localTime = formatComputerLocalDateTime(
      formValue.value.local_time ?? getComputerLocalTimestamp()
    );

    await setSystemTime({
      time_mode: formValue.value.time_mode,
      local_time: localTime,
      time_zone: formValue.value.time_zone,
    });

    await setNtpServer({
      host_name: formValue.value.host_name,
      port_no: formValue.value.port_no,
      syn_interval: formValue.value.syn_interval,
    });

    systemTimezone.value = formValue.value.time_zone;
    originalData.value = { ...formValue.value };
    message.success(t('common.success'));
  } catch (e) {
    message.error(t('common.saveFailed'));
  } finally {
    loading.value = false;
  }
};

const handleCancel = () => {
  Object.assign(formValue.value, originalData.value);
};

onMounted(() => {
  fetchData();
});
</script>
