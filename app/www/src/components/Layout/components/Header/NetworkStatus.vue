<template>
  <div
    v-if="loaded"
    class="network-status flex items-center gap-[8px] px-[12px] h-[28px] rounded-[14px] cursor-pointer select-none"
    :class="toneClass"
    :title="tooltip"
    @click="goNetworkPage"
  >
    <span class="dot w-[6px] h-[6px] rounded-full"></span>
    <span class="text-[12px] whitespace-nowrap">{{ label }}</span>
  </div>
</template>

<script lang="ts" setup>
  import { computed, onMounted, onUnmounted, ref } from 'vue';
  import { useRouter } from 'vue-router';
  import { useI18n } from 'vue-i18n';
  import {
    getSystemStatus,
    NETWORK_MODE_AP,
    NETWORK_STATE_ONLINE,
    PROVISION_STATE_FAILED,
    type SystemStatus,
  } from '@/api';

  const POLL_MS = 20_000;

  const router = useRouter();
  const { t } = useI18n();

  const status = ref<SystemStatus>({});
  const loaded = ref(false);
  let timer: ReturnType<typeof setTimeout> | null = null;
  let disposed = false;

  const mode = computed(() => status.value.network_mode ?? NETWORK_MODE_AP);
  const online = computed(() => status.value.network_state === NETWORK_STATE_ONLINE);

  /* Hotspot after a failed join, as opposed to a hotspot the user asked for. */
  const isFallback = computed(
    () => mode.value === NETWORK_MODE_AP && status.value.provision_state === PROVISION_STATE_FAILED
  );

  const label = computed(() => {
    const ssid = status.value.ssid || '';
    if (mode.value === NETWORK_MODE_AP) {
      return isFallback.value ? t('setting.statusApFallback') : t('setting.statusAp', { ssid });
    }
    if (!online.value) {
      return ssid ? t('setting.statusStaOffline', { ssid }) : t('setting.modeStateOffline');
    }
    return t('setting.statusStaOnline', { ssid });
  });

  const tooltip = computed(() => {
    const ip = status.value.ip;
    return ip ? `${label.value} · ${ip}` : label.value;
  });

  const toneClass = computed(() => {
    if (isFallback.value) return 'tone-warn';
    return online.value ? 'tone-ok' : 'tone-warn';
  });

  function goNetworkPage() {
    router.push('/setting/base/connect/tcp-ip');
  }

  async function poll() {
    try {
      status.value = (await getSystemStatus()) || {};
      loaded.value = true;
    } catch {
      // 重启期间请求失败时保留上一次展示
    }
    if (!disposed) timer = setTimeout(poll, POLL_MS);
  }

  onMounted(poll);

  onUnmounted(() => {
    disposed = true;
    if (timer) clearTimeout(timer);
  });
</script>

<style lang="less" scoped>
  .network-status {
    &.tone-ok {
      background: rgba(24, 160, 88, 0.12);
      color: #18a058;
      .dot {
        background: #18a058;
      }
    }

    &.tone-warn {
      background: rgba(240, 160, 32, 0.14);
      color: #d98a06;
      .dot {
        background: #d98a06;
      }
    }
  }
</style>
