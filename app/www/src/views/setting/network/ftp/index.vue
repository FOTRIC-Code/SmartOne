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
      <n-form-item :label="$t('setting.ftpProtocol')" path="use_ssl">
        <n-select v-model:value="formValue.use_ssl" :options="protocolOptions" />
      </n-form-item>
      <n-form-item :label="$t('setting.serverAddress')" path="ip">
        <n-input v-model:value="formValue.ip" :placeholder="$t('placeholder.enterServerAddress')" />
      </n-form-item>
      <n-form-item :label="$t('setting.portNumber')" path="port_no">
        <n-input-number
          v-model:value="formValue.port_no"
          :min="1"
          :max="65535"
          :show-button="false"
          class="w-full"
          :placeholder="$t('placeholder.enterPort')"
        />
      </n-form-item>
      <n-form-item :label="$t('setting.anonymousAccess')" path="enable_annoy">
        <n-switch v-model:value="formValue.enable_annoy" />
      </n-form-item>
      <n-form-item :label="$t('common.username')" path="user_name">
        <n-input
          v-model:value="formValue.user_name"
          :disabled="formValue.enable_annoy"
          :placeholder="$t('placeholder.account')"
        />
      </n-form-item>
      <n-form-item :label="$t('setting.password')" path="pwd">
        <n-input
          v-model:value="formValue.pwd"
          type="password"
          show-password-on="click"
          :disabled="formValue.enable_annoy"
          :placeholder="$t('placeholder.enterPassword')"
        />
      </n-form-item>
      <n-form-item :label="$t('setting.confirmPassword')" path="pwd_confirm">
        <n-input
          v-model:value="formValue.pwd_confirm"
          type="password"
          show-password-on="click"
          :disabled="formValue.enable_annoy"
          :placeholder="$t('placeholder.reenterPassword')"
        />
      </n-form-item>
      <n-form-item :label="$t('setting.pathTemplate')" path="path_template">
        <div class="path-template-editor w-full">
          <n-input
            :value="formValue.path_template"
            placeholder="{DEVICE_NAME}/{DEVICE_SERIAL_NO}/{yyyyMMddHHmmssfff}.jpg"
            @update:value="handlePathTemplateInput"
          />
          <div class="path-template-options">
            <button
              v-for="option in pathTemplateOptions"
              :key="option"
              type="button"
              class="path-template-card"
              :class="{ 'is-selected': isPathTemplateSelected(option) }"
              @click="togglePathTemplate(option)"
            >
              {{ option }}
            </button>
          </div>
        </div>
      </n-form-item>
    </n-form>

    <div class="action-bar separator-line-top">
      <n-button type="primary" :loading="saveLoading" @click="handleSubmit">{{ $t('common.save') }}</n-button>
      <n-button :loading="testLoading" @click="handleTestConnection">{{ $t('common.testConnection') }}</n-button>
      <n-button @click="handleCancel">{{ $t('common.cancel') }}</n-button>
    </div>
  </div>
</template>

<script lang="ts" setup>
  import { computed, nextTick, onMounted, ref } from 'vue';
  import { type FormRules } from 'naive-ui';
  import { useI18n } from 'vue-i18n';
  import { getSystemFtp, setSystemFtp, testSystemFtp } from '@/api';

  const { t } = useI18n();

  const saveLoading = ref(false);
  const testLoading = ref(false);
  const formRef = ref();

  const protocolOptions = computed(() => [
    { label: 'FTP', value: false },
    { label: t('setting.sftpDesc'), value: true },
  ]);

  const PATH_TEMPLATE_FILE = '{yyyyMMddHHmmssfff}.jpg';
  const PATH_TEMPLATE_DATE_TOKEN = '{yyyyMMddHHmmssfff}';

  const pathTemplateOptions = [
    '{DEVICE_NAME}',
    '{DEVICE_SERIAL_NO}',
    '{DEVICE_IP}',
    '{CHANNEL_NAME}',
    '{CHANNEL_ID}',
  ];

  const ALLOWED_PATH_TOKENS = new Set([...pathTemplateOptions, PATH_TEMPLATE_DATE_TOKEN]);

  const formValue = ref({
    use_ssl: false,
    ip: '',
    port_no: 21,
    user_name: '',
    pwd: '',
    pwd_confirm: '',
    enable_annoy: false,
    path_template: '',
  });

  let originalData: typeof formValue.value | null = null;

  const stripPathTemplateFile = (path: string) => {
    let base = path.trim().replace(/\\/g, '/');
    if (base.endsWith(PATH_TEMPLATE_FILE)) {
      base = base.slice(0, -PATH_TEMPLATE_FILE.length);
    }
    return base.replace(/\/+$/, '');
  };

  const withPathTemplateFile = (base: string) => {
    const normalized = base.replace(/\\/g, '/').replace(/\/{2,}/g, '/').replace(/^\/+|\/+$/g, '');
    return normalized ? `${normalized}/${PATH_TEMPLATE_FILE}` : PATH_TEMPLATE_FILE;
  };

  const getPathSegments = (path: string) => {
    const base = stripPathTemplateFile(path).replace(/\\/g, '/');
    return base ? base.split('/').filter(Boolean) : [];
  };

  const isPresetTokenSegment = (segment: string) => pathTemplateOptions.includes(segment);

  const isPathTemplateSelected = (option: string) =>
    getPathSegments(formValue.value.path_template).includes(option);

  const getLockedPresetTokens = (path: string) =>
    getPathSegments(path).filter((segment) => isPresetTokenSegment(segment));

  const listBraceTokens = (path: string) => path.match(/\{[^}]*\}/g) ?? [];

  const hasInvalidPathTokens = (path: string) => {
    const opens = (path.match(/\{/g) ?? []).length;
    const closes = (path.match(/\}/g) ?? []).length;
    if (opens !== closes) return true;
    return listBraceTokens(path).some((token) => !ALLOWED_PATH_TOKENS.has(token));
  };

  const normalizePathTemplate = (path: string) => {
    const unique: string[] = [];
    const seenPresets = new Set<string>();

    for (const segment of getPathSegments(path)) {
      if (isPresetTokenSegment(segment)) {
        if (seenPresets.has(segment)) continue;
        seenPresets.add(segment);
        unique.push(segment);
        continue;
      }
      if (hasInvalidPathTokens(segment) || pathTemplateOptions.some((token) => segment.includes(token))) {
        continue;
      }
      unique.push(segment);
    }

    return withPathTemplateFile(unique.join('/'));
  };

  const isValidPathTemplateInput = (path: string, lockedTokens: string[]) => {
    if (hasInvalidPathTokens(path)) return false;
    if (!path.endsWith(PATH_TEMPLATE_FILE)) return false;
    if (path.split(PATH_TEMPLATE_FILE).length - 1 !== 1) return false;

    const segments = getPathSegments(path);
    const presentPresets = segments.filter((segment) => isPresetTokenSegment(segment));

    if (lockedTokens.some((token) => !presentPresets.includes(token))) return false;

    for (const segment of segments) {
      if (isPresetTokenSegment(segment)) continue;
      if (pathTemplateOptions.some((token) => segment.includes(token))) return false;
      if (segment.includes(PATH_TEMPLATE_DATE_TOKEN) || segment.includes('{') || segment.includes('}')) {
        return false;
      }
    }

    return true;
  };

  const handlePathTemplateInput = (value: string) => {
    const prev = formValue.value.path_template;
    const next = value.replace(/\\/g, '/');
    const lockedTokens = getLockedPresetTokens(prev);

    if (!isValidPathTemplateInput(next, lockedTokens)) {
      formValue.value.path_template = next;
      nextTick(() => {
        formValue.value.path_template = prev;
      });
      return;
    }

    formValue.value.path_template = next;
  };

  const formRules = computed<FormRules>(() => ({
    ip: { required: true, message: t('placeholder.enterServerAddress'), trigger: ['input', 'blur'] },
    port_no: {
      required: true,
      type: 'number',
      message: t('placeholder.enterPort'),
      trigger: ['input', 'blur'],
    },
    user_name: formValue.value.enable_annoy
      ? {}
      : { required: true, message: t('placeholder.account'), trigger: ['input', 'blur'] },
    pwd: formValue.value.enable_annoy
      ? {}
      : { required: true, message: t('placeholder.enterPassword'), trigger: ['input', 'blur'] },
    pwd_confirm: formValue.value.enable_annoy
      ? []
      : [
          { required: true, message: t('placeholder.reenterPassword'), trigger: ['input', 'blur'] },
          {
            validator(_rule, value: string) {
              if (value !== formValue.value.pwd) {
                return new Error(t('common.passwordMismatch'));
              }
              return true;
            },
            trigger: ['input', 'blur'],
          },
        ],
  }));

  const getFtpPayload = () => ({
    use_ssl: formValue.value.use_ssl,
    ip: formValue.value.ip,
    port_no: formValue.value.port_no,
    user_name: formValue.value.user_name,
    pwd: formValue.value.pwd,
    enable_annoy: formValue.value.enable_annoy,
    path_template: normalizePathTemplate(formValue.value.path_template),
  });

  const togglePathTemplate = (template: string) => {
    const segments = getPathSegments(formValue.value.path_template);
    const index = segments.indexOf(template);
    if (index >= 0) {
      segments.splice(index, 1);
    } else {
      segments.push(template);
    }
    formValue.value.path_template = withPathTemplateFile(segments.join('/'));
  };

  const handleSubmit = async () => {
    saveLoading.value = true;
    try {
      await formRef.value?.validate();
    } catch {
      saveLoading.value = false;
      return;
    }
    try {
      const payload = getFtpPayload();
      await setSystemFtp(payload);
      formValue.value.path_template = payload.path_template;
      window['$message'].success(t('common.success'));
      originalData = { ...formValue.value };
    } catch (e) {
      console.error(e);
      window['$message'].error(t('common.saveFailed'));
    }
    saveLoading.value = false;
  };

  const handleTestConnection = async () => {
    testLoading.value = true;
    try {
      await formRef.value?.validate();
    } catch {
      testLoading.value = false;
      return;
    }
    try {
      await testSystemFtp(getFtpPayload());
      window['$message'].success(t('common.connectionSuccess'));
    } catch (e) {
      console.error(e);
      window['$message'].error(t('common.connectionFailed'));
    }
    testLoading.value = false;
  };

  const handleCancel = () => {
    if (originalData) {
      formValue.value = { ...originalData };
    } else {
      fetchFtp();
    }
  };

  async function fetchFtp() {
    try {
      const res: any = await getSystemFtp();
      if (res) {
        formValue.value.use_ssl = !!res.use_ssl;
        formValue.value.ip = res.ip ?? '';
        formValue.value.port_no = res.port_no ?? 21;
        formValue.value.user_name = res.user_name ?? '';
        formValue.value.pwd = res.pwd ?? '';
        formValue.value.pwd_confirm = res.pwd ?? '';
        formValue.value.enable_annoy = !!res.enable_annoy;
        formValue.value.path_template = normalizePathTemplate(res.path_template ?? '');
        originalData = { ...formValue.value };
      }
    } catch (e) {
      console.error(e);
    }
  }

  onMounted(() => {
    fetchFtp();
  });
</script>

<style scoped>
  .path-template-editor {
    display: flex;
    flex-direction: column;
    gap: 8px;
  }

  .path-template-options {
    display: flex;
    flex-wrap: wrap;
    gap: 8px;
  }

  .path-template-card {
    height: 28px;
    padding: 0 10px;
    color: var(--color-primary);
    background: #f4f8ff;
    border: 1px solid var(--color-primary);
    border-radius: 4px;
    cursor: pointer;
  }

  .path-template-card:hover {
    background: #eaf3ff;
  }

  .path-template-card.is-selected {
    color: #fff;
    background: var(--color-primary);
  }

  .path-template-card.is-selected:hover {
    opacity: 0.9;
  }
</style>
