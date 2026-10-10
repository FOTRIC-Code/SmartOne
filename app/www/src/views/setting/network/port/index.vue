<template>
  <div class="p-[20px] h-full flex flex-col">
      <n-form
        ref="formRef"
        class="w-[500px]"
        :model="formValue"
        :show-require-mark="false"
        label-placement="left"
        label-width="auto"
      >
        <n-form-item
          :label="$t('setting.httpPort')"
          path="http_port_no"
          :rule="[{ validator: validatorPort, trigger: ['input', 'blur'] }]"
        >
          <n-input-number
            v-model:value="formValue.http_port_no"
            :min="1"
            :max="65535"
            :show-button="false"
            class="w-full"
            :placeholder="$t('placeholder.enterHTTPPort')"
          />
        </n-form-item>
        <n-form-item
          :label="$t('setting.rtspPort')"
          path="rtsp_port_no"
          :rule="[{ validator: validatorPort, trigger: ['input', 'blur'] }]"
        >
          <n-input-number
            v-model:value="formValue.rtsp_port_no"
            :min="1"
            :max="65535"
            :show-button="false"
            class="w-full"
            :placeholder="$t('placeholder.enterRTSPPort')"
          />
        </n-form-item>
      </n-form>

    <div class="action-bar separator-line-top">
      <n-button type="primary" :loading="loading" @click="handleSubmit">{{ $t('common.save') }}</n-button>
      <n-button @click="handleCancel">{{ $t('common.cancel') }}</n-button>
    </div>
  </div>
</template>

<script lang="ts" setup>
  import { onMounted, ref } from 'vue';
  import { type FormItemRule } from 'naive-ui';
  import { useI18n } from 'vue-i18n';
  import { getPort, setPort } from '@/api';

  const { t } = useI18n();

  const loading = ref(false);
  const formRef = ref();

  const formValue = ref({
    http_port_no: 80,
    rtsp_port_no: 554,
    rtp_port_no: 8001,
  });

  let originalData: typeof formValue.value | null = null;

  const validatorPort = (_rule: FormItemRule, value: number) => {
    if (value === null || value === undefined) {
      return new Error(t('placeholder.enterPort'));
    }
    if (!Number.isInteger(value) || value < 1 || value > 65535) {
      return new Error(t('placeholder.portRange'));
    }
    return true;
  };

  const handleSubmit = async () => {
    loading.value = true;
    try {
      await formRef.value?.validate();
    } catch {
      loading.value = false;
      return;
    }
    try {
      await setPort({
        http_port_no: formValue.value.http_port_no,
        rtsp_port_no: formValue.value.rtsp_port_no,
        rtp_port_no: formValue.value.rtp_port_no,
      });
      window['$message'].success(t('common.success'));
      originalData = { ...formValue.value };
    } catch (e) {
      console.error(e);
      window['$message'].error(t('common.saveFailed'));
    }
    loading.value = false;
  };

  const handleCancel = () => {
    if (originalData) {
      formValue.value = { ...originalData };
    } else {
      fetchPort();
    }
  };

  async function fetchPort() {
    try {
      const res: any = await getPort();
      if (res) {
        formValue.value.http_port_no = res.http_port_no ?? 80;
        formValue.value.rtsp_port_no = res.rtsp_port_no ?? 554;
        formValue.value.rtp_port_no = res.rtp_port_no ?? 8001;
        originalData = { ...formValue.value };
      }
    } catch (e) {
      console.error(e);
    }
  }

  onMounted(() => {
    fetchPort();
  });
</script>
