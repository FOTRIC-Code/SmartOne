<template>
  <div class="h-full overflow-hidden bg-white px-[24px] pt-[16px]">
    <div>
      <div class="flex items-start gap-[56px]">
        <div class="image-preview-card border border-[#E5E7EB] bg-white">
          <div class="image-preview-video overflow-hidden bg-transparent" :style="previewStyle">
            <AutoContext :width="previewSize.width" :height="previewSize.height">
              <WebRTCVideo :streamId="101" />
            </AutoContext>
          </div>
        </div>

        <n-form
          ref="formRef"
          :model="form"
          class="image-display-form w-[390px]"
          label-placement="left"
          label-width="112"
          :show-feedback="false"
        >
          <n-form-item :label="$t('image.mirrorImage')">
            <n-switch v-model:value="mirror" />
          </n-form-item>

          <n-form-item :label="$t('image.imageFlip')">
            <n-switch v-model:value="flip" />
          </n-form-item>

          <n-form-item :label="$t('image.imageRotation')">
            <n-select v-model:value="form.rotation" :options="rotationOptions" class="w-[254px]" />
          </n-form-item>

          <n-form-item :label="$t('image.imageDetailEnhance')">
            <n-switch v-model:value="form.dde_enable" />
          </n-form-item>

          <n-form-item :label="$t('image.detailEnhanceLevel')">
            <div class="flex items-center gap-[16px]">
              <n-slider
                v-model:value="form.dde_value"
                :min="0"
                :max="100"
                :step="1"
                :disabled="!form.dde_enable"
                class="w-[132px]"
              />
              <n-input-number
                v-model:value="form.dde_value"
                :min="0"
                :max="100"
                :step="1"
                :disabled="!form.dde_enable"
                button-placement="both"
                class="w-[116px]"
                size="small"
              />
            </div>
          </n-form-item>

          <n-form-item :label="$t('image.highTempEqualize')">
            <n-switch v-model:value="form.colorDistAuto" />
          </n-form-item>

          <n-form-item :label="$t('image.colorPalette')">
            <n-select
              v-model:value="form.paletteRefNo"
              :options="paletteOptions"
              class="w-[254px]"
            />
          </n-form-item>

          <n-form-item :label="$t('image.temperatureMode')">
            <n-radio-group
              :value="form.gainMode"
              @update:value="handleGainModeChange"
            >
              <n-radio :value="TempWidthMode.Auto">{{ $t('image.autoTempWidth') }}</n-radio>
              <n-radio :value="TempWidthMode.Fixed">{{ $t('image.manualTempWidth') }}</n-radio>
            </n-radio-group>
          </n-form-item>

          <n-form-item :label="adjustTemperatureLabel">
            <div class="flex items-center gap-[8px]">
              <n-input-number
                :value="form.gainMin"
                :step="0.1"
                :precision="2"
                :max="form.gainMax"
                :disabled="form.gainMode !== TempWidthMode.Fixed"
                button-placement="both"
                class="w-[160px]"
                size="small"
                @update:value="handleGainMinChange"
              />
              <span class="text-gray-400">~</span>
              <n-input-number
                :value="form.gainMax"
                :step="0.1"
                :precision="2"
                :min="form.gainMin"
                :disabled="form.gainMode !== TempWidthMode.Fixed"
                button-placement="both"
                class="w-[160px]"
                size="small"
                @update:value="handleGainMaxChange"
              />
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
  </div>
</template>

<script lang="ts" setup>
  import { onMounted, ref, computed } from 'vue';
  import { useI18n } from 'vue-i18n';
  import { useMessage } from 'naive-ui';
  import { AutoContext } from '@/components/AutoContext';
  import { WebRTCVideo } from '@/components/WebRTCVideo';
  import { PREVIEW_VIDEO_SIZE } from '@/theme/designSetting';
  import {
    getImageParams,
    setImageParams,
    getImageFlip,
    setImageFlip,
    getPalettes,
    getPalette,
    setPalette,
    getGainMode,
    setGainMode,
    getColorDist,
    setColorDist,
  } from '@/api';
  import { TempWidthMode, ColorDistType, ImageRotation } from '@/data/enums';
  import { useThermalUnitStore } from '@data/stores/thermalUnit';

  const { t } = useI18n();
  const message = useMessage();
  const thermalUnitStore = useThermalUnitStore();
  const formRef = ref();
  const loading = ref(false);

  /* Net SDK 预置调色板编号（1~16），设备列表获取失败时兜底 */
  const FALLBACK_PALETTES: Array<{ label: string; value: number }> = [
    { label: 'Iron', value: 1 },
    { label: 'Grey', value: 2 },
    { label: 'Iron10', value: 3 },
    { label: 'Rainbow', value: 4 },
    { label: 'Grey10', value: 5 },
    { label: 'GreyRed', value: 6 },
    { label: 'MidGrey', value: 7 },
    { label: 'Yellow', value: 8 },
    { label: 'Rain', value: 9 },
    { label: 'Rain10', value: 10 },
    { label: 'Blue', value: 11 },
    { label: 'GlowBow', value: 12 },
    { label: 'Medical', value: 13 },
    { label: 'MidGreen', value: 14 },
    { label: 'Prism', value: 15 },
    { label: 'Medical10', value: 16 },
  ];

  const paletteOptions = ref<Array<{ label: string; value: number }>>([...FALLBACK_PALETTES]);

  const rotationOptions = computed(() => [
    { label: t('image.rotationNone'), value: ImageRotation.None },
    { label: t('image.rotation90'), value: ImageRotation.R90 },
    { label: t('image.rotation180'), value: ImageRotation.R180 },
    { label: t('image.rotation270'), value: ImageRotation.R270 },
  ]);

  const labelWithUnit = (label: string, unit: string) =>
    t('common.labelWithUnit', { label, unit });
  const adjustTemperatureLabel = computed(() =>
    labelWithUnit(t('image.adjustTemperature'), thermalUnitStore.tempUnitLabel)
  );

  /* 预览框宽高比跟随旋转值实时切换（90°/270° 时互换宽高） */
  const previewSize = computed(() =>
    form.value.rotation === ImageRotation.R90 || form.value.rotation === ImageRotation.R270
      ? { width: PREVIEW_VIDEO_SIZE.height, height: PREVIEW_VIDEO_SIZE.width }
      : { width: PREVIEW_VIDEO_SIZE.width, height: PREVIEW_VIDEO_SIZE.height }
  );
  const previewStyle = computed(() => ({
    width: `${previewSize.value.width}px`,
    height: `${previewSize.value.height}px`,
  }));

  const form = ref({
    brightness: 50,
    contrast: 50,
    dde_enable: false,
    dde_value: 50,
    rotation: ImageRotation.None as number,
    colorDistAuto: false,
    paletteRefNo: 1,
    gainMode: TempWidthMode.Auto as number,
    gainMin: 0,
    gainMax: 100,
  });

  const flipMode = ref(0);

  const mirror = computed({
    get: () => flipMode.value === 2 || flipMode.value === 3,
    set: (val: boolean) => {
      const hasFlip = flipMode.value === 1 || flipMode.value === 3;
      if (val && hasFlip) flipMode.value = 3;
      else if (val) flipMode.value = 2;
      else if (hasFlip) flipMode.value = 1;
      else flipMode.value = 0;
    },
  });

  const flip = computed({
    get: () => flipMode.value === 1 || flipMode.value === 3,
    set: (val: boolean) => {
      const hasMirror = flipMode.value === 2 || flipMode.value === 3;
      if (val && hasMirror) flipMode.value = 3;
      else if (val) flipMode.value = 1;
      else if (hasMirror) flipMode.value = 2;
      else flipMode.value = 0;
    },
  });

  const originalForm = ref({ ...form.value });
  const originalFlipMode = ref(0);

  const normalizeTempWidthMode = (mode: unknown) =>
    Number(mode) === TempWidthMode.Fixed ? TempWidthMode.Fixed : TempWidthMode.Auto;

  const applyGainMinMax = async () => {
    try {
      const gainRes = await getGainMode();
      if (gainRes) {
        form.value.gainMin = gainRes.min ?? 0;
        form.value.gainMax = gainRes.max ?? 100;
      }
    } catch {
      message.error(t('common.fetchFailed'));
    }
  };

  const handleGainModeChange = async (mode: number) => {
    const next = normalizeTempWidthMode(mode);
    const prev = form.value.gainMode;
    form.value.gainMode = next;
    if (next === TempWidthMode.Auto && prev !== TempWidthMode.Auto) {
      await applyGainMinMax();
    }
  };

  const handleGainMinChange = (value: number | null) => {
    if (value == null) return;
    form.value.gainMin = Math.min(value, form.value.gainMax);
  };

  const handleGainMaxChange = (value: number | null) => {
    if (value == null) return;
    form.value.gainMax = Math.max(value, form.value.gainMin);
  };

  const fetchData = async () => {
    try {
      const [imgRes, flipRes, paletteRes, gainRes, colorDistRes, palettesRes] = await Promise.all([
        getImageParams().catch(() => null),
        getImageFlip().catch(() => null),
        getPalette().catch(() => null),
        getGainMode().catch(() => null),
        getColorDist().catch(() => null),
        getPalettes().catch(() => null),
      ]);

      if (imgRes) {
        form.value.brightness = imgRes.brightness ?? 50;
        form.value.contrast = imgRes.contrast ?? 50;
        form.value.dde_enable = imgRes.dde_enable ?? false;
        form.value.dde_value = imgRes.dde_value ?? 50;
      }

      if (flipRes) {
        flipMode.value = flipRes.mode ?? 0;
        form.value.rotation = flipRes.rotation ?? ImageRotation.None;
      }

      if (paletteRes) {
        form.value.paletteRefNo = paletteRes.ref_no ?? 1;
      }

      if (gainRes) {
        form.value.gainMode = normalizeTempWidthMode(gainRes.mode);
        form.value.gainMin = gainRes.min ?? 0;
        form.value.gainMax = gainRes.max ?? 100;
      }

      if (colorDistRes) {
        form.value.colorDistAuto = colorDistRes.type === ColorDistType.TTwb;
      }

      if (palettesRes && Array.isArray(palettesRes) && palettesRes.length) {
        paletteOptions.value = palettesRes.map((p, i) => ({
          label: p.name || FALLBACK_PALETTES.find((f) => f.value === p.ref_no)?.label || `Palette ${p.ref_no ?? i}`,
          value: p.ref_no ?? i,
        }));
      }

      originalForm.value = { ...form.value };
      originalFlipMode.value = flipMode.value;
    } catch {
      message.error(t('common.fetchFailed'));
    }
  };

  const handleSave = async () => {
    if (
      form.value.gainMode === TempWidthMode.Fixed &&
      (form.value.gainMin ?? 0) > (form.value.gainMax ?? 0)
    ) {
      message.error(t('image.manualTempWidthRangeInvalid'));
      return;
    }

    loading.value = true;
    try {
      const saves: Promise<unknown>[] = [
        /* brightness/contrast 页面不出，透传读取原值 */
        setImageParams({
          brightness: form.value.brightness,
          contrast: form.value.contrast,
          dde_enable: form.value.dde_enable,
          dde_value: form.value.dde_value,
        }),
        setImageFlip({ mode: flipMode.value, rotation: form.value.rotation }),
        setPalette({
          ref_no: form.value.paletteRefNo,
          colors: [],
          color_count: 0,
          inverted: false,
        }),
        setGainMode({
          mode: form.value.gainMode,
          max: form.value.gainMax ?? 100,
          min: form.value.gainMin ?? 0,
          fixedSpan: form.value.gainMode === TempWidthMode.Fixed,
          span:
            form.value.gainMode === TempWidthMode.Fixed
              ? (form.value.gainMax ?? 100) - (form.value.gainMin ?? 0)
              : 0,
        }),
        setColorDist({
          type: form.value.colorDistAuto ? ColorDistType.TTwb : ColorDistType.AdLinear,
        }),
      ];
      await Promise.all(saves);
      originalForm.value = { ...form.value };
      originalFlipMode.value = flipMode.value;
      message.success(t('common.success'));
    } catch {
      message.error(t('common.saveFailed'));
    } finally {
      loading.value = false;
    }
  };

  const handleCancel = () => {
    Object.assign(form.value, originalForm.value);
    flipMode.value = originalFlipMode.value;
  };

  const handleResetDefaults = () => {
    form.value.brightness = 50;
    form.value.contrast = 50;
    form.value.dde_enable = false;
    form.value.dde_value = 50;
    form.value.rotation = ImageRotation.None;
    form.value.colorDistAuto = false;
    form.value.paletteRefNo = paletteOptions.value[0]?.value ?? 1;
    form.value.gainMode = TempWidthMode.Auto;
    form.value.gainMin = 0;
    form.value.gainMax = 100;
    flipMode.value = 0;
  };

  onMounted(async () => {
    await thermalUnitStore.fetchUnit();
    fetchData();
  });
</script>

<style scoped>
  .image-preview-card {
    width: max-content;
  }

  .image-preview-video {
    position: relative;
    line-height: 0;
  }

  .image-preview-video :deep(.auto-context),
  .image-preview-video :deep(.webrtc-video-root) {
    position: absolute;
    inset: 0;
  }

  .image-preview-video :deep(video) {
    object-fit: fill;
  }

  .image-display-form :deep(.n-form-item) {
    margin-bottom: 8px;
  }
</style>
