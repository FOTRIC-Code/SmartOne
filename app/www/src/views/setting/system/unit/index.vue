<template>
  <div class="p-[20px]">
    <n-form :model="formValue" label-placement="left" label-width="auto" class="w-[500px]">
      <n-form-item :label="$t('thermal.temperatureUnit')">
        <n-radio-group v-model:value="formValue.temp_unit">
          <n-space>
            <n-radio :value="0">°C</n-radio>
            <n-radio :value="1">K</n-radio>
            <n-radio :value="2">°F</n-radio>
          </n-space>
        </n-radio-group>
      </n-form-item>
      <n-form-item :label="$t('thermal.distanceUnit')">
        <n-radio-group v-model:value="formValue.distance_unit">
          <n-space>
            <n-radio :value="0">{{ $t('thermal.meter') }}</n-radio>
            <n-radio :value="1">{{ $t('thermal.foot') }}</n-radio>
          </n-space>
        </n-radio-group>
      </n-form-item>
    </n-form>

    <div class="action-bar separator-line-top">
      <n-button type="primary" :loading="loading" @click="handleSave">{{ $t('common.save') }}</n-button>
      <n-button @click="handleCancel">{{ $t('common.cancel') }}</n-button>
    </div>
  </div>
</template>

<script lang="ts" setup>
  import { getThermalUnit, setThermalUnit } from '@/api';
  import { useThermalUnitStore } from '@data/stores/thermalUnit';
  import { useMessage } from 'naive-ui';
  import { onMounted, ref } from 'vue';
  import { useI18n } from 'vue-i18n';

  const { t } = useI18n();
  const message = useMessage();
  const thermalUnitStore = useThermalUnitStore();
  const loading = ref(false);

  const formValue = ref({
    temp_unit: 0,
    distance_unit: 0,
  });

  const originalData = ref({ ...formValue.value });

  const fetchData = async () => {
    try {
      const res = await getThermalUnit();
      formValue.value.temp_unit = res?.temp_unit ?? 0;
      formValue.value.distance_unit = res?.distance_unit ?? 0;
      thermalUnitStore.setUnit(formValue.value);
      originalData.value = { ...formValue.value };
    } catch {
      message.error(t('common.fetchFailed'));
    }
  };

  const handleSave = async () => {
    loading.value = true;
    try {
      await setThermalUnit({ ...formValue.value });
      thermalUnitStore.setUnit(formValue.value);
      originalData.value = { ...formValue.value };
      message.success(t('common.success'));
    } catch {
      message.error(t('common.saveFailed'));
    } finally {
      loading.value = false;
    }
  };

  const handleCancel = () => {
    Object.assign(formValue.value, originalData.value);
  };

  onMounted(fetchData);
</script>
