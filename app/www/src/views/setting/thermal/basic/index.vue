<template>
  <div class="thermal-basic-page h-full overflow-hidden bg-white px-[20px] pt-[16px] flex flex-col">
    <div class="thermal-basic-content">
      <div class="thermal-preview-card">
        <div class="thermal-video" :style="previewStyle">
          <AutoContext :width="previewSize.width" :height="previewSize.height">
            <WebRTCVideo :streamId="101" />
          </AutoContext>
        </div>
        <div class="h-[32px] flex items-center px-[10px]">
          <button class="toolbar-btn" :disabled="calibrating" @click="handleCalibrate">
            <img src="@/resource/images/calibrate.svg" class="w-[18px] h-[18px]" />
          </button>
        </div>
      </div>

      <n-form
        ref="formRef"
        :model="form"
        class="thermal-basic-form"
        label-placement="left"
        label-width="160"
        :show-feedback="false"
      >
        <n-form-item :label="$t('thermal.thermalCorrectionMode')">
          <n-radio-group v-model:value="form.shutterMode">
            <n-radio :value="0">{{ $t('thermal.autoCorrection') }}</n-radio>
            <n-radio :value="1">{{ $t('thermal.manualCorrection') }}</n-radio>
          </n-radio-group>
        </n-form-item>

        <n-form-item :label="$t('thermal.correctionInterval')">
          <n-input-number
            v-model:value="form.shutterInterval"
            :min="1"
            :step="1"
            :precision="0"
            :show-button="false"
            class="w-[260px]"
          >
            <template #suffix>{{ $t('thermal.minuteUnit') }}</template>
          </n-input-number>
        </n-form-item>

        <n-form-item :label="measurementRangeLabel">
          <n-select v-model:value="form.rangeId" :options="rangeOptions" class="w-[260px]" />
        </n-form-item>

        <div class="thermal-section-title">{{ $t('thermal.temperatureParamAdjust') }}</div>

        <n-form-item :label="$t('thermal.emissivity')">
          <div class="field-with-hint">
            <n-input-number
              v-model:value="form.emissivity"
              :min="0.01"
              :max="1.00"
              :step="0.01"
              :precision="2"
              :show-button="false"
              class="w-[260px]"
            />
            <span>{{ $t('thermal.range01To100') }}</span>
          </div>
        </n-form-item>

        <n-form-item :label="measureDistanceLabel">
          <n-input-number
            v-model:value="form.distance"
            :min="0"
            :step="0.1"
            :precision="2"
            :show-button="false"
            class="w-[260px]"
          />
        </n-form-item>

        <n-form-item :label="reflectionTempLabel">
          <n-input-number
            v-model:value="form.reflTemp"
            :step="0.1"
            :precision="2"
            :show-button="false"
            class="w-[260px]"
          />
        </n-form-item>

        <n-form-item :label="atmosphericTempLabel">
          <n-input-number
            v-model:value="form.atmTemp"
            :step="0.1"
            :precision="2"
            :show-button="false"
            class="w-[260px]"
          />
        </n-form-item>

        <n-form-item :label="$t('thermal.relativeHumidity')">
          <div class="field-with-hint">
            <n-input-number
              v-model:value="form.relHumidity"
              :min="0"
              :max="100"
              :step="1"
              :precision="2"
              :show-button="false"
              class="w-[260px]"
            />
            <span>{{ $t('thermal.range0To100Percent') }}</span>
          </div>
        </n-form-item>

        <n-form-item>
          <n-button @click="handleResetDefaults">
            {{ $t('common.restoreDefaults') }}
          </n-button>
        </n-form-item>
      </n-form>
    </div>

    <div class="action-bar separator-line-top mt-[24px]">
      <n-button type="primary" :loading="loading" @click="handleSave">{{ $t('common.save') }}</n-button>
      <n-button @click="handleCancel">{{ $t('common.cancel') }}</n-button>
    </div>
  </div>
</template>

<script lang="ts" setup>
  import { onMounted, ref, computed } from 'vue';
  import { useMessage } from 'naive-ui';
  import { useI18n } from 'vue-i18n';
  import { AutoContext } from '@/components/AutoContext';
  import { WebRTCVideo } from '@/components/WebRTCVideo';
  import { PREVIEW_VIDEO_SIZE } from '@/theme/designSetting';
  import { useThermalUnitStore } from '@data/stores/thermalUnit';
  import { ImageRotation } from '@/data/enums';
  import {
    convertKelvinToTempUnit,
    convertTempUnitToKelvin,
    convertMetersToDistanceUnit,
    convertDistanceUnitToMeters,
  } from '@data/models/thermal';
  import {
    getShutterMode,
    setShutterMode,
    getShutterTime,
    setShutterTime,
    calibrateThermal,
    getThermalRange,
    setThermalRange,
    getThermalParams,
    setThermalParams,
    getLensInfo,
    getImageFlip,
  } from '@/api';

  const { t } = useI18n();
  const message = useMessage();
  const thermalUnitStore = useThermalUnitStore();
  const formRef = ref();
  const loading = ref(false);
  const calibrating = ref(false);

  const rangeOptions = ref<Array<{ label: string; value: number }>>([]);

  /* 预览框宽高比跟随图像管理里的旋转设置（90°/270° 时互换宽高） */
  const rotation = ref<number>(ImageRotation.None);
  const previewSize = computed(() =>
    rotation.value === ImageRotation.R90 || rotation.value === ImageRotation.R270
      ? { width: PREVIEW_VIDEO_SIZE.height, height: PREVIEW_VIDEO_SIZE.width }
      : { width: PREVIEW_VIDEO_SIZE.width, height: PREVIEW_VIDEO_SIZE.height }
  );
  const previewStyle = computed(() => ({
    width: `${previewSize.value.width}px`,
    height: `${previewSize.value.height}px`,
  }));

  const form = ref({
    shutterMode: 0,
    shutterInterval: 10,
    rangeId: 0,
    emissivity: 0.95,
    distance: 1.0,
    reflTemp: 20.0,
    atmTemp: 20.0,
    relHumidity: 50,
  });

  const originalForm = ref({ ...form.value });

  /* 接口侧温度/距离为开尔文/米（Volga 契约），页面按配置单位展示 */
  const kelvinToDisplay = (k: number) =>
    Number(convertKelvinToTempUnit(k, thermalUnitStore.tempUnit).toFixed(2));
  const displayToKelvin = (v: number) =>
    Number(convertTempUnitToKelvin(v, thermalUnitStore.tempUnit).toFixed(2));
  const metersToDisplay = (m: number) =>
    Number(convertMetersToDistanceUnit(m, thermalUnitStore.distanceUnit).toFixed(2));
  const displayToMeters = (v: number) =>
    Number(convertDistanceUnitToMeters(v, thermalUnitStore.distanceUnit).toFixed(3));

  const labelWithUnit = (label: string, unit: string) =>
    t('common.labelWithUnit', { label, unit });
  const measurementRangeLabel = computed(() =>
    labelWithUnit(t('thermal.measurementRange'), thermalUnitStore.tempUnitLabel)
  );
  const measureDistanceLabel = computed(() =>
    labelWithUnit(t('thermal.measureDistance'), thermalUnitStore.distanceUnitLabel)
  );
  const reflectionTempLabel = computed(() =>
    labelWithUnit(t('thermal.reflectionTempC'), thermalUnitStore.tempUnitLabel)
  );
  const atmosphericTempLabel = computed(() =>
    labelWithUnit(t('thermal.atmosphericTempC'), thermalUnitStore.tempUnitLabel)
  );

  const fetchData = async () => {
    try {
      const [shutterModeRes, shutterTimeRes, rangeRes, paramsRes, lensRes, flipRes] =
        await Promise.all([
          getShutterMode().catch(() => null),
          getShutterTime().catch(() => null),
          getThermalRange().catch(() => null),
          getThermalParams().catch(() => null),
          getLensInfo().catch(() => null),
          getImageFlip().catch(() => null),
        ]);

      if (shutterModeRes) {
        form.value.shutterMode = shutterModeRes.mode ?? 0;
      }

      if (shutterTimeRes) {
        form.value.shutterInterval = shutterTimeRes.interval ?? 30;
      }

      if (rangeRes) {
        form.value.rangeId = rangeRes.range_id ?? 0;
      }

      if (paramsRes) {
        form.value.emissivity = paramsRes.emissivity ?? 0.95;
        form.value.distance = metersToDisplay(paramsRes.distance ?? 1.0);
        form.value.reflTemp = kelvinToDisplay(paramsRes.refl_temp ?? 293.15);
        form.value.atmTemp = kelvinToDisplay(paramsRes.atm_temp ?? 293.15);
        // 接口侧 rel_humidity 为 0~1 小数，页面按 0~100 百分比展示
        form.value.relHumidity = (paramsRes.rel_humidity ?? 0.5) * 100;
      }

      if (lensRes?.range_map && Array.isArray(lensRes.range_map)) {
        rangeOptions.value = lensRes.range_map.map((r, i) => ({
          label: `${kelvinToDisplay(r.range.min)} ~ ${kelvinToDisplay(r.range.max)}`,
          value: r.id ?? i,
        }));
        /* 设备量程 id 为 1 起始；当前值不在选项内时回落到第一档 */
        if (
          rangeOptions.value.length > 0 &&
          !rangeOptions.value.some((o) => o.value === form.value.rangeId)
        ) {
          form.value.rangeId = rangeOptions.value[0].value;
        }
      }

      if (flipRes) {
        rotation.value = flipRes.rotation ?? ImageRotation.None;
      }

      originalForm.value = { ...form.value };
    } catch {
      message.error(t('common.fetchFailed'));
    }
  };

  const handleCalibrate = async () => {
    calibrating.value = true;
    try {
      await calibrateThermal();
      message.success(t('thermal.correctionDone'));
    } catch {
      message.error(t('thermal.correctionFailed'));
    } finally {
      calibrating.value = false;
    }
  };

  const handleSave = async () => {
    loading.value = true;
    try {
      await Promise.all([
        setShutterMode({ mode: form.value.shutterMode }),
        setShutterTime({ interval: form.value.shutterInterval }),
        setThermalRange({ range_id: form.value.rangeId }),
        setThermalParams({
          emissivity: form.value.emissivity,
          distance: displayToMeters(form.value.distance),
          refl_temp: displayToKelvin(form.value.reflTemp),
          atm_temp: displayToKelvin(form.value.atmTemp),
          rel_humidity: form.value.relHumidity / 100, // 页面为百分比，接口侧为 0~1
        }),
      ]);
      originalForm.value = { ...form.value };
      message.success(t('common.success'));
    } catch {
      message.error(t('common.saveFailed'));
    } finally {
      loading.value = false;
    }
  };

  const handleCancel = () => {
    Object.assign(form.value, originalForm.value);
  };

  const handleResetDefaults = () => {
    form.value.shutterMode = 0;
    form.value.shutterInterval = 10;
    form.value.rangeId = rangeOptions.value[0]?.value ?? 1;
    form.value.emissivity = 0.95;
    form.value.distance = metersToDisplay(1.0);
    form.value.reflTemp = kelvinToDisplay(293.15);
    form.value.atmTemp = kelvinToDisplay(293.15);
    form.value.relHumidity = 50;
  };

  onMounted(async () => {
    await thermalUnitStore.fetchUnit();
    fetchData();
  });
</script>

<style scoped>
  .thermal-preview-card {
    flex: 0 0 auto;
    width: max-content;
    border: 1px solid #e5e7eb;
    background: #fff;
  }

  .thermal-video {
    position: relative;
    overflow: hidden;
    line-height: 0;
    background: transparent;
  }

  .thermal-video :deep(.auto-context),
  .thermal-video :deep(.webrtc-video-root) {
    position: absolute;
    inset: 0;
  }

  .thermal-video :deep(video) {
    object-fit: fill;
  }

  .toolbar-btn {
    display: flex;
    align-items: center;
    justify-content: center;
    width: 30px;
    height: 30px;
    padding: 0;
    cursor: pointer;
    background: #fff;
    border: 1px solid #dcdfe6;
    border-radius: 4px;
  }

  .toolbar-btn:hover:not(:disabled) {
    border-color: #b4b4b4;
  }

  .toolbar-btn:disabled {
    cursor: not-allowed;
    opacity: 0.6;
  }

  .thermal-basic-content {
    display: flex;
    align-items: flex-start;
    gap: 20px;
    overflow: auto;
  }

  .thermal-basic-form {
    width: 470px;
    flex: 0 0 470px;
  }

  .thermal-basic-form :deep(.n-form-item) {
    min-height: 32px;
    margin-bottom: 12px;
  }

  .thermal-basic-form :deep(.n-form-item-label) {
    min-height: 32px;
    white-space: normal;
  }

  .thermal-section-title {
    height: 32px;
    padding-left: 34px;
    margin-bottom: 12px;
    font-size: 14px;
    line-height: 32px;
    color: #333;
    background: #f7f8fa;
  }

  .field-with-hint {
    display: flex;
    flex-direction: column;
    gap: 4px;
  }

  .field-with-hint span {
    font-size: 12px;
    line-height: 16px;
    color: #9ca3af;
  }
</style>
