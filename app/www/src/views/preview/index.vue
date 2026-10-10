<template>
  <div class="live-page h-full flex flex-col min-h-0">
    <div ref="videoAreaRef" class="video-area flex-1 flex flex-col items-center justify-center min-h-0 px-[16px]">
      <div class="video-panel flex flex-col w-full max-w-[960px]" :style="{ width: videoWidth ? `${videoWidth}px` : undefined }">
        <div
          class="video-frame overflow-hidden bg-[#1c1917]"
          :style="{ height: videoHeight ? `${videoHeight}px` : '56.25%' }"
        >
          <AutoContext :width="4" :height="3">
            <WebRTCVideo ref="webrtcVideoRef" :streamId="String(activeStreamId)" />
          </AutoContext>
        </div>
        <div v-if="channels.length > 1" class="stream-bar">
          <button
            v-for="ch in channels"
            :key="ch.id"
            type="button"
            class="stream-chip"
            :class="{ active: activeStreamId === ch.id }"
            @click="activeStreamId = ch.id"
          >
            {{ ch.id }}
          </button>
        </div>
      </div>
    </div>
  </div>
</template>

<script lang="ts" setup>
  import { onBeforeUnmount, onMounted, ref } from 'vue';
  import { getStreamingChannels, type StreamChannel } from '@/api';
  import AutoContext from '@/components/AutoContext/src/index.vue';
  import WebRTCVideo from '@/components/WebRTCVideo/index.vue';

  const webrtcVideoRef = ref();
  const videoAreaRef = ref<HTMLElement>();
  const videoWidth = ref(0);
  const videoHeight = ref(0);
  const activeStreamId = ref(101);
  const channels = ref<StreamChannel[]>([]);

  let resizeObserver: ResizeObserver | null = null;

  const updateVideoSize = () => {
    if (!videoAreaRef.value) return;
    const { width: cw, height: ch } = videoAreaRef.value.getBoundingClientRect();
    const pad = 32;
    const w = Math.min(cw - pad, (ch - pad) * (4 / 3));
    videoWidth.value = Math.max(280, w);
    videoHeight.value = videoWidth.value * (3 / 4);
  };

  onMounted(async () => {
    try {
      const list = await getStreamingChannels();
      channels.value = Array.isArray(list) ? list : [];
      if (channels.value.length) {
        activeStreamId.value = channels.value[0].id || 101;
      }
    } catch {
      channels.value = [{ id: 101 } as StreamChannel];
    }
    updateVideoSize();
    if (videoAreaRef.value) {
      resizeObserver = new ResizeObserver(updateVideoSize);
      resizeObserver.observe(videoAreaRef.value);
    }
  });

  onBeforeUnmount(() => {
    resizeObserver?.disconnect();
  });
</script>

<style scoped>
  .live-page {
    background: #f3f1ec;
  }
  .video-frame {
    border-radius: 8px;
  }
  .stream-bar {
    display: flex;
    gap: 8px;
    padding: 12px 0 0;
  }
  .stream-chip {
    border: 1px solid #d6d3d1;
    background: #fff;
    border-radius: 6px;
    padding: 4px 12px;
    font-size: 13px;
    color: #57534e;
    cursor: pointer;
  }
  .stream-chip.active {
    border-color: #0f766e;
    color: #0f766e;
    font-weight: 600;
  }
</style>
