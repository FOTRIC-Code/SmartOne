<template>
  <div class="network-tcp-ip-page h-full bg-white px-[28px] pt-[36px] flex flex-col">
    <n-alert v-if="readonlyAp" type="warning" :bordered="false" class="mb-[16px] max-w-[545px]">
      {{ $t('setting.tcpIpReadonlyHotspot') }}
    </n-alert>
    <n-form
      ref="formRef"
      class="network-tcp-ip-form w-[545px]"
      :model="formValue"
      :show-require-mark="false"
      label-placement="left"
      label-width="112"
      :disabled="readonlyAp"
    >
        <n-form-item :label="$t('setting.ipAddressAcquisitionMethod')">
          <n-radio-group v-model:value="formValue.dhcp">
            <n-radio :value="true">{{ $t('setting.automaticDhcp') }}</n-radio>
            <n-radio :value="false">{{ $t('setting.manualStaticIp') }}</n-radio>
          </n-radio-group>
        </n-form-item>
        <n-form-item
          :label="$t('setting.ipAddress')"
          path="ip"
          :rule="[{ validator: validatorIPv4, trigger: ['input', 'blur'] }]"
        >
          <n-input v-model:value="formValue.ip" :disabled="formValue.dhcp" :placeholder="$t('placeholder.enterIP')" />
        </n-form-item>
        <n-form-item
          :label="$t('setting.subnetMask')"
          path="netmask"
          :rule="[{ validator: isValidMask, trigger: ['input', 'blur'] }]"
        >
          <n-input
            v-model:value="formValue.netmask"
            :disabled="formValue.dhcp"
            :placeholder="$t('placeholder.enterSubnetMask')"
          />
        </n-form-item>
        <n-form-item
          :label="$t('setting.defaultGateway')"
          path="gateway"
          :rule="[{ validator: validatorGateway, trigger: ['input', 'blur'] }]"
        >
          <n-input
            v-model:value="formValue.gateway"
            :disabled="formValue.dhcp"
            :placeholder="$t('placeholder.enterGateway')"
          />
        </n-form-item>
        <n-form-item
          :label="$t('setting.preferredDNSServer')"
          path="preferred_dns"
          :rule="[{ validator: validatorDns, trigger: ['input', 'blur'] }]"
        >
          <n-input
            v-model:value="formValue.preferred_dns"
            :disabled="formValue.dhcp"
            :placeholder="$t('placeholder.enterDNS')"
          />
        </n-form-item>
        <n-form-item
          :label="$t('setting.AlternateDNSServer')"
          path="alternate_dns"
          :rule="[{ validator: validatorDns, trigger: ['input', 'blur'] }]"
        >
          <n-input
            v-model:value="formValue.alternate_dns"
            :disabled="formValue.dhcp"
            :placeholder="$t('placeholder.enterDNS')"
          />
        </n-form-item>
    </n-form>

    <div class="action-bar separator-line-top mt-[28px]">
      <n-button type="primary" :loading="loading" :disabled="readonlyAp" @click="handleSubmit">{{ $t('common.save') }}</n-button>
      <n-button :disabled="readonlyAp" @click="handleCancel">{{ $t('common.cancel') }}</n-button>
    </div>
  </div>
</template>

<script lang="ts" setup>
  import { onMounted, ref } from 'vue';
  import { type FormItemRule } from 'naive-ui';
  import { useI18n } from 'vue-i18n';
  import { getNetwork, getSystemStatus, setNetwork, NETWORK_MODE_AP } from '@/api';

  const { t } = useI18n();

  const loading = ref(false);
  const readonlyAp = ref(false);
  const formRef = ref();

  const formValue = ref({
    id: 1,
    name: 'eth0',
    dhcp: false,
    ip: '',
    netmask: '',
    gateway: '',
    preferred_dns: '',
    alternate_dns: '',
  });

  let originalData: typeof formValue.value | null = null;

  const ipv4Pattern =
    /^(?:(?:25[0-5]|2[0-4][0-9]|1[0-9]{2}|[1-9][0-9]|[0-9])\.){3}(?:25[0-5]|2[0-4][0-9]|1[0-9]{2}|[1-9][0-9]|[0-9])$/;

  const validatorIPv4 = (_rule: FormItemRule, value: string) => {
    if (!value) return true;
    if (!ipv4Pattern.test(value)) return new Error(t('placeholder.invalidIP'));
    return true;
  };

  const isValidMask = (_rule: FormItemRule, value: string) => {
    if (!value) return true;
    const maskPattern =
      /^(254|252|248|240|224|192|128|0)\.0\.0\.0|255\.(254|252|248|240|224|192|128|0)\.0\.0|255\.255\.(254|252|248|240|224|192|128|0)\.0|255\.255\.255\.(255|254|252|248|240|224|192|128|0)$/;
    if (!maskPattern.test(value)) return new Error(t('placeholder.invalidSubnetMask'));
    return true;
  };

  const validatorGateway = (_rule: FormItemRule, value: string) => {
    if (!value) return true;
    if (!ipv4Pattern.test(value)) return new Error(t('placeholder.invalidGateway'));
    return true;
  };

  const validatorDns = (_rule: FormItemRule, value: string) => {
    if (!value) return true;
    if (!ipv4Pattern.test(value)) return new Error(t('placeholder.invalidDNS'));
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
      await setNetwork(formValue.value.id, {
        id: formValue.value.id,
        name: formValue.value.name,
        dhcp: formValue.value.dhcp,
        ip: formValue.value.ip,
        netmask: formValue.value.netmask,
        dns: [formValue.value.preferred_dns, formValue.value.alternate_dns].filter(Boolean),
        gateway: formValue.value.gateway ? [formValue.value.gateway] : [],
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
      fetchNetwork();
    }
  };

  async function fetchNetwork() {
    try {
      const st = await getSystemStatus();
      readonlyAp.value = st?.network_mode === NETWORK_MODE_AP;
      const res: any = await getNetwork(1);
      if (res) {
        formValue.value.id = res.id ?? 1;
        formValue.value.name = res.name ?? 'eth0';
        formValue.value.dhcp = !!res.dhcp;
        formValue.value.ip = res.ip ?? '';
        formValue.value.netmask = res.netmask ?? '';
        formValue.value.gateway = Array.isArray(res.gateway) ? res.gateway[0] ?? '' : '';
        formValue.value.preferred_dns = Array.isArray(res.dns) ? res.dns[0] ?? '' : '';
        formValue.value.alternate_dns = Array.isArray(res.dns) ? res.dns[1] ?? '' : '';
        originalData = { ...formValue.value };
      }
    } catch (e) {
      console.error(e);
    }
  }

  onMounted(() => {
    fetchNetwork();
  });
</script>

<style scoped>
  .network-tcp-ip-form :deep(.n-form-item) {
    margin-bottom: 18px;
  }

  .network-tcp-ip-form :deep(.n-form-item-label__text) {
    white-space: nowrap;
  }

  .network-tcp-ip-form :deep(.n-radio) {
    margin-right: 24px;
  }
</style>
