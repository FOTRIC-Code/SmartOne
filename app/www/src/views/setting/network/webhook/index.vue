<template>
  <div class="p-[20px] h-full flex flex-col">
    <n-form
      ref="formRef"
      class="w-[500px]"
      :model="formValue"
      :rules="formRules"
      :show-require-mark="false"
      label-placement="left"
      label-width="auto"
    >
      <n-form-item :label="$t('setting.webhookUrl')" path="url">
        <n-input v-model:value="formValue.url" placeholder="https://example.com/webhook" />
      </n-form-item>
    </n-form>

    <div class="action-bar separator-line-top">
      <n-button type="primary" :loading="saveLoading" @click="handleSubmit">{{ $t('common.save') }}</n-button>
      <n-button @click="handleCancel">{{ $t('common.cancel') }}</n-button>
    </div>
  </div>
</template>

<script lang="ts" setup>
  import { computed, onMounted, ref } from 'vue';
  import { type FormRules } from 'naive-ui';
  import { useI18n } from 'vue-i18n';
  import { getSystemWebhook, setSystemWebhook } from '@/api';

  const { t } = useI18n();

  const saveLoading = ref(false);
  const formRef = ref();

  const formValue = ref({
    url: '',
  });

  let originalData: typeof formValue.value | null = null;

  const formRules = computed<FormRules>(() => ({
    url: {
      required: true,
      message: t('setting.webhookUrl'),
      trigger: ['input', 'blur'],
      validator(_rule, value: string) {
        if (!value) {
          return new Error(t('setting.webhookUrl'));
        }
        if (!/^https?:\/\//i.test(value)) {
          return new Error(t('smart.webhookUrlInvalid'));
        }
        return true;
      },
    },
  }));

  const handleSubmit = async () => {
    saveLoading.value = true;
    try {
      await formRef.value?.validate();
    } catch {
      saveLoading.value = false;
      return;
    }
    try {
      await setSystemWebhook({ url: formValue.value.url });
      window['$message'].success(t('common.success'));
      originalData = { ...formValue.value };
    } catch (e) {
      console.error(e);
      window['$message'].error(t('common.saveFailed'));
    }
    saveLoading.value = false;
  };

  const handleCancel = () => {
    if (originalData) {
      formValue.value = { ...originalData };
    } else {
      fetchWebhook();
    }
  };

  async function fetchWebhook() {
    try {
      const res: any = await getSystemWebhook();
      if (res) {
        formValue.value.url = res.url ?? '';
        originalData = { ...formValue.value };
      }
    } catch (e) {
      console.error(e);
    }
  }

  onMounted(() => {
    fetchWebhook();
  });
</script>
