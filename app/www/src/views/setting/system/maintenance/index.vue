<template>
  <div class="p-[20px]">
    <!-- 重启 -->
    <div class="bg-[var(--color-page-bg)] px-[16px] py-[8px] font-medium">{{ $t('common.restart') }}</div>
    <div class="px-[16px] py-[16px] flex items-center gap-[16px]">
      <n-button class="btn-danger" @click="restartModalVisible = true">{{ $t('common.restart') }}</n-button>
      <span class="text-gray-500">{{ $t('common.restartDescribe') }}。</span>
    </div>

    <!-- 恢复出厂设置 -->
    <div class="bg-[var(--color-page-bg)] px-[16px] py-[8px] font-medium mt-[16px]">{{ $t('common.factoryReset') }}</div>
    <div class="px-[16px] py-[16px] flex items-center gap-[16px]">
      <n-button class="btn-danger" @click="resetModalVisible = true">{{ $t('common.factoryResetBtn') }}</n-button>
      <span class="text-gray-500">{{ $t('common.recoveryAllDescribe') }}。</span>
    </div>

    <!-- 升级固件 -->
    <div class="bg-[var(--color-page-bg)] px-[16px] py-[8px] font-medium mt-[16px]">{{ $t('setting.upgradeTitle') }}</div>
    <div class="px-[16px] py-[16px]">
      <div class="flex items-center gap-[12px]">
        <n-select
          v-model:value="fileType"
          :options="fileTypeOptions"
          class="w-[120px]"
        />
        <n-input
          v-model:value="fileName"
          readonly
          :placeholder="$t('placeholder.select')"
          class="w-[240px]"
        />
        <n-button @click="handleBrowse">{{ $t('common.browse') }}</n-button>
        <n-button :loading="upgrading" @click="handleUpgrade">{{ $t('common.upgrade') }}</n-button>
      </div>
      <div class="text-orange-500 text-[13px] mt-[12px]">
        {{ $t('setting.upgradeWarning') }}
      </div>
    </div>

    <!-- 重新配网（仅支持蓝牙 App） -->
    <div class="bg-[var(--color-page-bg)] px-[16px] py-[8px] font-medium mt-[16px]">{{ $t('setting.reprovisionTitle') }}</div>
    <div class="px-[16px] py-[16px]">
      <span class="text-gray-500">{{ $t('setting.reprovisionBleHint') }}</span>
    </div>

    <input
      ref="fileInputRef"
      type="file"
      class="hidden"
      :accept="acceptTypes"
      @change="handleFileChange"
    />

    <!-- 重启确认弹框 -->
    <n-modal
      v-model:show="restartModalVisible"
      :mask-closable="false"
      preset="card"
      class="maintenance-modal"
      :title="$t('setting.confirmRestart')"
      :style="modalStyle"
    >
      <div class="maintenance-modal-body">
        {{ $t('setting.confirmRestartContent') }}
      </div>
      <template #footer>
        <div class="maintenance-modal-footer">
          <n-button @click="restartModalVisible = false">{{ $t('common.cancel') }}</n-button>
          <n-button type="primary" :loading="restarting" @click="handleRestart">
            {{ $t('common.confirm') }}
          </n-button>
        </div>
      </template>
    </n-modal>

    <!-- 恢复出厂确认弹框 -->
    <n-modal
      v-model:show="resetModalVisible"
      :mask-closable="false"
      preset="card"
      class="maintenance-modal"
      :title="$t('setting.confirmFactoryReset')"
      :style="modalStyle"
    >
      <div class="maintenance-modal-body">
        {{ $t('setting.confirmFactoryResetContent') }}
      </div>
      <template #footer>
        <div class="maintenance-modal-footer">
          <n-button @click="resetModalVisible = false">{{ $t('common.cancel') }}</n-button>
          <n-button type="primary" :loading="resetting" @click="handleReset">
            {{ $t('common.confirm') }}
          </n-button>
        </div>
      </template>
    </n-modal>

    <!-- 升级确认弹框 -->
    <n-modal
      v-model:show="upgradeModalVisible"
      :mask-closable="false"
      preset="card"
      class="maintenance-modal"
      :title="$t('setting.confirmUpgrade')"
      :style="modalStyle"
    >
      <div class="maintenance-modal-body">
        <div>{{ $t('setting.upgradeFileLabel') }}: {{ fileName }}</div>
        <div>{{ $t('setting.upgradeFileSizeLabel') }}: {{ formattedFileSize }}</div>
        <div class="maintenance-modal-hints">
          <div>{{ $t('setting.upgradeUnavailableHint') }}</div>
          <div>{{ $t('setting.upgradeDoNotCloseHint') }}</div>
        </div>
      </div>
      <template #footer>
        <div class="maintenance-modal-footer">
          <n-button @click="upgradeModalVisible = false">{{ $t('common.cancel') }}</n-button>
          <n-button type="primary" :loading="upgrading" @click="confirmUpgrade">
            {{ $t('common.confirm') }}
          </n-button>
        </div>
      </template>
    </n-modal>
  </div>
</template>

<script lang="ts" setup>
import { restartDevice, resetDevice, upgradeDevice } from '@/api';
import { useMessage } from 'naive-ui';
import { computed, ref } from 'vue';
import { useI18n } from 'vue-i18n';

const { t } = useI18n();
const message = useMessage();

const modalStyle = { width: '420px' };

const fileType = ref('firmware');
const fileName = ref('');
const upgrading = ref(false);
const restarting = ref(false);
const resetting = ref(false);
const restartModalVisible = ref(false);
const resetModalVisible = ref(false);
const upgradeModalVisible = ref(false);
const fileInputRef = ref<HTMLInputElement>();
const selectedFile = ref<File | null>(null);

const fileTypeOptions = computed(() => [
  { label: t('setting.upgradeFile'), value: 'firmware' },
]);

const acceptTypes = '.bin,.img,.fw';

const formatFileSize = (bytes: number) => {
  if (bytes < 1024) return `${bytes} B`;
  if (bytes < 1024 * 1024) return `${(bytes / 1024).toFixed(1)} KB`;
  return `${(bytes / (1024 * 1024)).toFixed(1)} MB`;
};

const formattedFileSize = computed(() =>
  selectedFile.value ? formatFileSize(selectedFile.value.size) : ''
);

const handleRestart = async () => {
  restarting.value = true;
  try {
    await restartDevice();
    restartModalVisible.value = false;
    message.success(t('setting.deviceRestarting'));
  } catch {
    message.error(t('common.operationFailed'));
  } finally {
    restarting.value = false;
  }
};

const handleReset = async () => {
  resetting.value = true;
  try {
    await resetDevice();
    resetModalVisible.value = false;
    message.success(t('setting.deviceResetting'));
  } catch {
    message.error(t('common.operationFailed'));
  } finally {
    resetting.value = false;
  }
};

const handleBrowse = () => {
  fileInputRef.value?.click();
};

const handleFileChange = (e: Event) => {
  const input = e.target as HTMLInputElement;
  if (input.files && input.files.length > 0) {
    selectedFile.value = input.files[0];
    fileName.value = input.files[0].name;
  }
};

const handleUpgrade = () => {
  if (!selectedFile.value) {
    message.warning(t('setting.selectUpgradeFile'));
    return;
  }
  if (selectedFile.value.size === 0) {
    message.warning(t('setting.upgradeEmptyFile'));
    return;
  }
  upgradeModalVisible.value = true;
};

const confirmUpgrade = async () => {
  if (!selectedFile.value) return;

  upgrading.value = true;
  try {
    const formData = new FormData();
    formData.append('file', selectedFile.value);
    formData.append('type', 'firmware');
    await upgradeDevice(formData);
    upgradeModalVisible.value = false;
    message.success(t('setting.upgradeSuccess'));
  } catch {
    message.error(t('setting.upgradeFailed'));
  } finally {
    upgrading.value = false;
  }
};
</script>

<style lang="less" scoped>
.maintenance-modal-body {
  font-size: 14px;
  line-height: 22px;
  color: #333;
  min-height: 44px;
}

.maintenance-modal-hints {
  margin-top: 16px;
}

.maintenance-modal-footer {
  display: flex;
  justify-content: flex-end;
  gap: 8px;
}
</style>
