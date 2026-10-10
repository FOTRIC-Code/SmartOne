<template>
  <div class="p-[20px]">
    <n-form
      ref="formRef"
      :model="formValue"
      label-placement="left"
      label-width="auto"
      class="w-[600px]"
    >
      <n-form-item :label="$t('setting.deviceName')" path="name">
        <n-input
          :value="formValue.name"
          @update:value="(value) => (formValue.name = sanitizeFolderName(value))"
        />
      </n-form-item>
      <n-form-item :label="$t('setting.deviceModel')" path="model">
        <n-input v-model:value="formValue.model" disabled />
      </n-form-item>
      <n-form-item :label="$t('setting.specModel')" path="spec">
        <n-input v-model:value="formValue.spec" disabled />
      </n-form-item>
      <n-form-item :label="$t('setting.deviceSerialNumber')" path="serial_no">
        <n-input v-model:value="formValue.serial_no" disabled />
      </n-form-item>
      <n-form-item :label="$t('setting.firmwareName')" path="firmware_name">
        <n-input v-model:value="formValue.firmware_name" disabled />
      </n-form-item>
      <n-form-item :label="$t('setting.firmwareVersion')" path="firmware_version">
        <n-input v-model:value="formValue.firmware_version" disabled />
      </n-form-item>
    </n-form>
    <div class="action-bar separator-line-top">
      <n-button type="primary" :loading="loading" @click="handleSave">{{ $t('common.save') }}</n-button>
      <n-button @click="handleCancel">{{ $t('common.cancel') }}</n-button>
    </div>
  </div>
</template>

<script lang="ts" setup>
import { getDeviceInfo, setDeviceName } from '@/api';
import { sanitizeFolderName } from '@/utils/filename';
import { useMessage } from 'naive-ui';
import { onMounted, ref } from 'vue';
import { useI18n } from 'vue-i18n';

const { t } = useI18n();
const message = useMessage();
const formRef = ref();
const loading = ref(false);

const formValue = ref({
  name: '',
  model: '',
  spec: '',
  serial_no: '',
  firmware_name: '',
  firmware_version: '',
});

const originalName = ref('');

const fetchData = async () => {
  const res: any = await getDeviceInfo();
  const name = sanitizeFolderName(res.name ?? '');
  formValue.value = {
    name,
    model: res.model ?? '',
    spec: res.spec ?? '',
    serial_no: res.serial_no ?? '',
    firmware_name: res.firmware_name ?? '',
    firmware_version: res.firmware_version ?? '',
  };
  originalName.value = name;
};

const handleSave = async () => {
  formValue.value.name = sanitizeFolderName(formValue.value.name);
  loading.value = true;
  try {
    await setDeviceName(formValue.value.name);
    originalName.value = formValue.value.name;
    message.success(t('setting.deviceNameSaved'));
  } catch (e) {
    message.error(t('common.saveFailed'));
  } finally {
    loading.value = false;
  }
};

const handleCancel = () => {
  formValue.value.name = originalName.value;
};

onMounted(() => {
  fetchData();
});
</script>
