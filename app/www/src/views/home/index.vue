<template>
  <div class="home-page h-full overflow-auto">
    <div class="home-stage min-h-full flex items-center justify-center px-[20px] py-[32px]">
      <div class="device-panel w-full max-w-[520px]">
        <div class="brand">SmartOne</div>
        <h1 class="device-name">{{ deviceName }}</h1>
        <p class="device-meta">{{ metaLine }}</p>

        <div class="status-row">
          <span class="pill" :class="online ? 'is-on' : 'is-off'">
            {{ online ? $t('home.online') : $t('home.offline') }}
          </span>
          <span class="pill muted">
            {{ streaming ? $t('home.streaming') : $t('home.streamingOff') }}
          </span>
        </div>

        <dl class="facts">
          <div>
            <dt>{{ $t('home.wifi') }}</dt>
            <dd>{{ wifiSsid || '—' }}</dd>
          </div>
          <div>
            <dt>{{ $t('home.ip') }}</dt>
            <dd>{{ ip || '—' }}</dd>
          </div>
          <div>
            <dt>{{ $t('home.firmware') }}</dt>
            <dd>{{ firmware || '—' }}</dd>
          </div>
        </dl>

        <div class="actions">
          <n-button type="primary" size="large" block @click="goLive">
            {{ $t('home.openLive') }}
          </n-button>
          <n-button size="large" block quaternary :loading="restarting" @click="doRestart">
            {{ restarting ? $t('home.restarting') : $t('home.restart') }}
          </n-button>
        </div>
      </div>
    </div>
  </div>
</template>

<script lang="ts" setup>
  import { computed, onMounted, onUnmounted, ref } from 'vue';
  import { useRouter } from 'vue-router';
  import { useMessage } from 'naive-ui';
  import { useI18n } from 'vue-i18n';
  import {
    getDeviceInfo,
    getSystemStatus,
    NETWORK_STATE_ONLINE,
    restartDevice,
  } from '@/api';

  const STATUS_POLL_MS = 10_000;

  const router = useRouter();
  const messageApi = useMessage();
  const { t } = useI18n();

  const deviceName = ref('SmartOne');
  const model = ref('');
  const serial = ref('');
  const firmware = ref('');
  const wifiSsid = ref('');
  const ip = ref('');
  const networkState = ref<number | ''>('');
  const streaming = ref(false);
  const restarting = ref(false);

  const online = computed(() => networkState.value === NETWORK_STATE_ONLINE);
  const metaLine = computed(() =>
    [model.value && `${t('home.model')} ${model.value}`, serial.value && `${t('home.serial')} ${serial.value}`]
      .filter(Boolean)
      .join(' · ')
  );

  function goLive() {
    router.push('/live');
  }

  async function doRestart() {
    restarting.value = true;
    try {
      await restartDevice();
      messageApi.success(t('home.restarting'));
    } catch (e: any) {
      messageApi.error(e?.message || t('common.operationFailed'));
    } finally {
      restarting.value = false;
    }
  }

  let timer: ReturnType<typeof setTimeout> | null = null;
  let disposed = false;

  async function pollStatus() {
    const st = await getSystemStatus().catch(() => null);

    if (st) {
      wifiSsid.value = st.ssid || '';
      ip.value = st.ip || '';
      networkState.value = st.network_state ?? '';
      streaming.value = !!st.streaming;
    }
    if (!disposed) timer = setTimeout(pollStatus, STATUS_POLL_MS);
  }

  onMounted(async () => {
    try {
      const info = await getDeviceInfo().catch(() => null);
      if (info) {
        deviceName.value = (info as any).name || (info as any).brand || 'SmartOne';
        model.value = (info as any).model || '';
        serial.value = (info as any).serial_no || '';
        firmware.value = (info as any).firmware_version || '';
      }
      await pollStatus();
    } catch {
      messageApi.error(t('home.loadFailed'));
    }
  });

  onUnmounted(() => {
    disposed = true;
    if (timer) clearTimeout(timer);
  });
</script>

<style scoped>
  .home-stage {
    background:
      radial-gradient(ellipse 80% 50% at 50% -10%, rgba(15, 118, 110, 0.18), transparent 55%),
      linear-gradient(165deg, #f3f1ec 0%, #e8ebe9 48%, #dfe6e3 100%);
  }
  .device-panel {
    animation: rise 0.55s ease-out;
  }
  @keyframes rise {
    from {
      opacity: 0;
      transform: translateY(12px);
    }
    to {
      opacity: 1;
      transform: none;
    }
  }
  .brand {
    font-family: 'Segoe UI', 'PingFang SC', 'Noto Sans SC', sans-serif;
    font-size: 13px;
    letter-spacing: 0.28em;
    text-transform: uppercase;
    color: #0f766e;
    font-weight: 600;
    margin-bottom: 12px;
  }
  .device-name {
    font-family: 'Iowan Old Style', 'Palatino Linotype', 'Songti SC', serif;
    font-size: clamp(28px, 5vw, 40px);
    font-weight: 600;
    color: #1c1917;
    line-height: 1.15;
    margin: 0 0 8px;
  }
  .device-meta {
    color: #78716c;
    font-size: 14px;
    margin: 0 0 20px;
    min-height: 1.2em;
  }
  .status-row {
    display: flex;
    flex-wrap: wrap;
    gap: 8px;
    margin-bottom: 28px;
  }
  .pill {
    font-size: 12px;
    padding: 4px 10px;
    border-radius: 4px;
    font-weight: 500;
  }
  .pill.is-on {
    background: #ccfbf1;
    color: #0f766e;
  }
  .pill.is-off {
    background: #f5f5f4;
    color: #78716c;
  }
  .pill.muted {
    background: rgba(28, 25, 23, 0.06);
    color: #57534e;
  }
  .facts {
    display: grid;
    gap: 14px;
    margin: 0 0 28px;
  }
  .facts dt {
    font-size: 12px;
    color: #a8a29e;
    margin-bottom: 2px;
  }
  .facts dd {
    margin: 0;
    font-size: 16px;
    color: #1c1917;
    font-weight: 500;
    word-break: break-all;
  }
  .actions {
    display: flex;
    flex-direction: column;
    gap: 10px;
  }
</style>
