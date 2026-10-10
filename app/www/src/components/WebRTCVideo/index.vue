<template>
  <div class="webrtc-video-root" ref="videoParentRef">
    <video ref="videoRef" autoplay muted playsinline></video>
    <div
      v-if="statusOverlayVisible"
      class="webrtc-status-overlay"
      :class="`is-${connectionStatus}`"
    >
      <div class="webrtc-status-spinner" v-if="connectionStatus !== 'failed'" />
      <span class="webrtc-status-text">{{ statusText }}</span>
    </div>
  </div>
</template>

<script lang="ts">
  type SharedWebRTCState = {
    peer: RTCPeerConnection | null;
    stream: MediaStream | null;
    streamId: string | null;
    generation: number;
    users: number;
    closeTimer: ReturnType<typeof setTimeout> | null;
    reconnectTimer: ReturnType<typeof setTimeout> | null;
    abortController: AbortController | null;
    reconnectAttempt: number;
    creating: boolean;
  };

  export type WebRTCConnectionStatus =
    | 'idle'
    | 'connecting'
    | 'playing'
    | 'reconnecting'
    | 'failed';

  const sharedWebRTC: SharedWebRTCState = {
    peer: null,
    stream: null,
    streamId: null,
    generation: 0,
    users: 0,
    closeTimer: null,
    reconnectTimer: null,
    abortController: null,
    reconnectAttempt: 0,
    creating: false,
  };

  let activeVideo: HTMLVideoElement | null = null;
  let pageCloseHookInstalled = false;
  let sharedConnectionStatus: WebRTCConnectionStatus = 'idle';
  const statusSubscribers = new Set<(status: WebRTCConnectionStatus) => void>();

  const setSharedConnectionStatus = (status: WebRTCConnectionStatus) => {
    if (sharedConnectionStatus === status) return;
    sharedConnectionStatus = status;
    statusSubscribers.forEach((listener) => listener(status));
  };

  /** Match device WebRTC cooldown / low-mem recover window. */
  const RECONNECT_BASE_MS = 3000;
  const RECONNECT_MAX_MS = 15000;
  const SIGNALING_TIMEOUT_MS = 10000;
  const SHARED_CLOSE_DELAY_MS = 5000;

  const webrtcLog = (level: 'info' | 'warn' | 'error', step: string, detail?: Record<string, unknown>) => {
    const payload = detail ? ` ${JSON.stringify(detail)}` : '';
    const msg = `[WebRTC][${step}]${payload}`;
    if (level === 'error') console.error(msg);
    else if (level === 'warn') console.warn(msg);
    else console.info(msg);
  };

  const clearCloseTimer = () => {
    if (sharedWebRTC.closeTimer) {
      clearTimeout(sharedWebRTC.closeTimer);
      sharedWebRTC.closeTimer = null;
    }
  };

  const clearReconnectTimer = () => {
    if (sharedWebRTC.reconnectTimer) {
      clearTimeout(sharedWebRTC.reconnectTimer);
      sharedWebRTC.reconnectTimer = null;
    }
  };

  const detachVideo = (video?: HTMLVideoElement | null) => {
    if (!video) return;
    if (activeVideo === video) activeVideo = null;
    video.srcObject = null;
    video.removeAttribute('src');
    video.load();
  };

  const attachStreamToActiveVideo = () => {
    if (activeVideo && sharedWebRTC.stream) {
      activeVideo.srcObject = sharedWebRTC.stream;
    }
  };

  const closeSharedWebRTC = (opts?: { keepReconnect?: boolean }) => {
    clearCloseTimer();
    if (!opts?.keepReconnect) {
      clearReconnectTimer();
      sharedWebRTC.reconnectAttempt = 0;
    }
    sharedWebRTC.generation++;
    sharedWebRTC.creating = false;

    if (sharedWebRTC.abortController) {
      sharedWebRTC.abortController.abort();
      sharedWebRTC.abortController = null;
    }

    if (sharedWebRTC.peer) {
      sharedWebRTC.peer.onicecandidate = null;
      sharedWebRTC.peer.ontrack = null;
      sharedWebRTC.peer.oniceconnectionstatechange = null;
      sharedWebRTC.peer.onconnectionstatechange = null;
      sharedWebRTC.peer.onsignalingstatechange = null;
      sharedWebRTC.peer.getSenders().forEach((sender) => sender.track?.stop());
      sharedWebRTC.peer.getReceivers().forEach((receiver) => receiver.track?.stop());
      sharedWebRTC.peer.close();
      sharedWebRTC.peer = null;
    }

    if (sharedWebRTC.stream) {
      sharedWebRTC.stream.getTracks().forEach((track) => track.stop());
      sharedWebRTC.stream = null;
    }

    sharedWebRTC.streamId = null;
  };

  const scheduleSharedClose = () => {
    if (sharedWebRTC.users > 0) return;
    clearCloseTimer();
    clearReconnectTimer();
    sharedWebRTC.closeTimer = setTimeout(() => {
      if (sharedWebRTC.users <= 0) {
        closeSharedWebRTC();
        setSharedConnectionStatus('idle');
      }
    }, SHARED_CLOSE_DELAY_MS);
  };

  const isPeerUsable = (streamId: string) => {
    const peer = sharedWebRTC.peer;
    return (
      !!peer &&
      sharedWebRTC.streamId === streamId &&
      peer.signalingState !== 'closed' &&
      peer.connectionState !== 'closed' &&
      peer.connectionState !== 'failed' &&
      peer.iceConnectionState !== 'failed' &&
      peer.iceConnectionState !== 'closed'
    );
  };

  const reconnectDelayMs = (attempt: number) => {
    const exp = Math.min(RECONNECT_MAX_MS, RECONNECT_BASE_MS * Math.pow(2, Math.max(0, attempt - 1)));
    const jitter = Math.floor(Math.random() * 500);
    return exp + jitter;
  };

  const installPageCloseHook = () => {
    if (pageCloseHookInstalled) return;
    pageCloseHookInstalled = true;
    window.addEventListener('pagehide', () => closeSharedWebRTC());
    window.addEventListener('beforeunload', () => closeSharedWebRTC());
  };
</script>

<script lang="ts" setup>
  import { getWebRTCUrl } from '@/manager/businessLogic';
  import axios from 'axios';
  import { computed, onBeforeUnmount, onMounted, ref, watch } from 'vue';
  import { useI18n } from 'vue-i18n';

  const props = defineProps({
    streamId: {
      type: String,
      default: '101',
    },
  });

  const { t } = useI18n();
  const videoParentRef = ref();
  const videoRef = ref();
  const connectionStatus = ref<WebRTCConnectionStatus>(sharedConnectionStatus);

  let mounted = false;

  const statusOverlayVisible = computed(
    () =>
      connectionStatus.value === 'connecting' ||
      connectionStatus.value === 'reconnecting' ||
      connectionStatus.value === 'failed'
  );

  const statusText = computed(() => {
    switch (connectionStatus.value) {
      case 'connecting':
        return t('common.videoConnecting');
      case 'reconnecting':
        return t('common.videoReconnecting');
      case 'failed':
        return t('common.videoReconnectFailed');
      default:
        return '';
    }
  });

  const onSharedStatus = (status: WebRTCConnectionStatus) => {
    connectionStatus.value = status;
  };

  const bindActiveVideo = () => {
    const video = videoRef.value as HTMLVideoElement | undefined;
    if (!video) return;
    activeVideo = video;
    attachStreamToActiveVideo();
  };

  const scheduleReconnect = (reason: string) => {
    if (!mounted || sharedWebRTC.users <= 0) return;
    if (sharedWebRTC.reconnectTimer) return;

    sharedWebRTC.reconnectAttempt += 1;
    const delay = reconnectDelayMs(sharedWebRTC.reconnectAttempt);
    setSharedConnectionStatus('reconnecting');
    webrtcLog('warn', 'reconnect_scheduled', {
      reason,
      attempt: sharedWebRTC.reconnectAttempt,
      delayMs: delay,
      streamId: props.streamId,
    });

    sharedWebRTC.reconnectTimer = setTimeout(() => {
      sharedWebRTC.reconnectTimer = null;
      if (!mounted || sharedWebRTC.users <= 0) return;
      createWebRTC();
    }, delay);
  };

  const failAndReconnect = (reason: string, peer?: RTCPeerConnection | null) => {
    if (peer && sharedWebRTC.peer && sharedWebRTC.peer !== peer) return;
    setSharedConnectionStatus('failed');
    closeSharedWebRTC({ keepReconnect: true });
    scheduleReconnect(reason);
  };

  const createWebRTC = async () => {
    clearCloseTimer();
    clearReconnectTimer();
    bindActiveVideo();
    if (!mounted || sharedWebRTC.users <= 0) return;

    if (isPeerUsable(props.streamId)) {
      attachStreamToActiveVideo();
      sharedWebRTC.reconnectAttempt = 0;
      if (sharedWebRTC.stream) {
        setSharedConnectionStatus('playing');
      }
      return;
    }

    if (sharedWebRTC.creating) return;

    closeSharedWebRTC({ keepReconnect: true });
    if (!mounted) return;

    setSharedConnectionStatus(sharedWebRTC.reconnectAttempt > 0 ? 'reconnecting' : 'connecting');
    sharedWebRTC.creating = true;
    const generation = ++sharedWebRTC.generation;
    const nextPeer = new RTCPeerConnection();
    const abortController = new AbortController();
    sharedWebRTC.peer = nextPeer;
    sharedWebRTC.streamId = props.streamId;
    sharedWebRTC.abortController = abortController;
    initPeerListen(nextPeer, generation);
    webrtcLog('info', 'create_start', {
      streamId: props.streamId,
      generation,
      attempt: sharedWebRTC.reconnectAttempt,
      signalingBase: getWebRTCUrl(),
    });

    try {
      // Modern browsers ignore deprecated offerToReceive*; without a transceiver,
      // createOffer() yields SDP with no m-line and ZLM rejects with media.empty().
      nextPeer.addTransceiver('video', { direction: 'recvonly' });
      const offer = await nextPeer.createOffer();
      if (!mounted || generation !== sharedWebRTC.generation || sharedWebRTC.peer !== nextPeer) return;
      await nextPeer.setLocalDescription(offer);
      webrtcLog('info', 'local_offer_set', {
        iceGathering: nextPeer.iceGatheringState,
        signaling: nextPeer.signalingState,
        offerBytes: offer.sdp?.length ?? 0,
        hasVideoMLine: /m=video/i.test(offer.sdp || ''),
      });
      const localSdp = await waitForIceGatheringComplete(nextPeer, abortController.signal);
      if (!mounted || generation !== sharedWebRTC.generation || sharedWebRTC.peer !== nextPeer) return;
      webrtcLog('info', 'ice_gather_done', {
        iceGathering: nextPeer.iceGatheringState,
        sdpBytes: localSdp.length,
        candidateLines: (localSdp.match(/^a=candidate:/gm) || []).length,
      });
      await sendOffer(nextPeer, localSdp, generation, abortController);
      if (generation === sharedWebRTC.generation && sharedWebRTC.peer === nextPeer) {
        sharedWebRTC.reconnectAttempt = 0;
      }
    } catch (e: any) {
      if (abortController.signal.aborted) return;
      webrtcLog('error', 'create_failed', {
        message: String(e?.message || e || 'create failed'),
        ice: nextPeer.iceConnectionState,
        conn: nextPeer.connectionState,
        signaling: nextPeer.signalingState,
      });
      const msg = String(e?.message || e || 'create failed');
      failAndReconnect(msg, nextPeer);
    } finally {
      if (generation === sharedWebRTC.generation) {
        sharedWebRTC.creating = false;
      }
    }
  };

  const initPeerListen = (currentPeer: RTCPeerConnection, generation: number) => {
    currentPeer.ontrack = (ev) => {
      if (generation !== sharedWebRTC.generation || sharedWebRTC.peer !== currentPeer) return;
      if (ev.streams && ev.streams[0]) {
        const stream = ev.streams[0];
        sharedWebRTC.stream = stream;
        sharedWebRTC.reconnectAttempt = 0;
        attachStreamToActiveVideo();
        setSharedConnectionStatus('playing');
        webrtcLog('info', 'ontrack', {
          tracks: stream.getTracks().map((t) => `${t.kind}:${t.readyState}`),
        });
      }
    };

    currentPeer.onconnectionstatechange = () => {
      if (generation !== sharedWebRTC.generation || sharedWebRTC.peer !== currentPeer) return;
      const state = currentPeer.connectionState;
      webrtcLog('info', 'connectionstate', { state, ice: currentPeer.iceConnectionState });
      if (state === 'connected') {
        if (sharedWebRTC.stream) setSharedConnectionStatus('playing');
      } else if (state === 'failed' || state === 'closed') {
        failAndReconnect(`connection ${state}`, currentPeer);
      } else if (state === 'disconnected') {
        // Brief ICE blips are common; only reconnect if still disconnected shortly after.
        setTimeout(() => {
          if (generation !== sharedWebRTC.generation || sharedWebRTC.peer !== currentPeer) return;
          if (currentPeer.connectionState === 'disconnected' || currentPeer.connectionState === 'failed') {
            failAndReconnect('connection disconnected', currentPeer);
          }
        }, 2500);
      }
    };

    currentPeer.oniceconnectionstatechange = () => {
      if (generation !== sharedWebRTC.generation || sharedWebRTC.peer !== currentPeer) return;
      const state = currentPeer.iceConnectionState;
      webrtcLog('info', 'iceconnectionstate', { state, conn: currentPeer.connectionState });
      if (state === 'failed') {
        failAndReconnect(`ice ${state}`, currentPeer);
      }
    };
  };

  /** ZLM expects a non-trickle offer (candidates already in SDP). */
  const waitForIceGatheringComplete = (
    peer: RTCPeerConnection,
    signal: AbortSignal,
    timeoutMs = SIGNALING_TIMEOUT_MS
  ): Promise<string> => {
    const sdp = () => peer.localDescription?.sdp ?? '';
    if (peer.iceGatheringState === 'complete') {
      return Promise.resolve(sdp());
    }
    return new Promise((resolve, reject) => {
      let settled = false;
      const finish = (ok: boolean, err?: Error) => {
        if (settled) return;
        settled = true;
        clearTimeout(timer);
        peer.removeEventListener('icegatheringstatechange', onStateChange);
        signal.removeEventListener('abort', onAbort);
        if (ok) resolve(sdp());
        else reject(err ?? new Error('ice gathering failed'));
      };
      const onStateChange = () => {
        if (peer.iceGatheringState === 'complete') finish(true);
      };
      const onAbort = () => finish(false, new Error('signaling aborted'));
      const timer = setTimeout(() => {
        // Prefer sending host/srflx already gathered over hanging forever.
        finish(true);
      }, timeoutMs);
      peer.addEventListener('icegatheringstatechange', onStateChange);
      signal.addEventListener('abort', onAbort);
      if (peer.iceGatheringState === 'complete') finish(true);
    });
  };

  const sendOffer = async (
    currentPeer: RTCPeerConnection,
    offerSdp: string,
    generation: number,
    abortController: AbortController
  ) => {
    if (!offerSdp || !/m=video/i.test(offerSdp)) {
      throw new Error('invalid offer: missing video m-line');
    }

    const url = `${getWebRTCUrl()}/media/webrtc?app=video&stream=${props.streamId}&type=play`;
    webrtcLog('info', 'signaling_post', {
      url,
      offerBytes: offerSdp.length,
      timeoutMs: SIGNALING_TIMEOUT_MS,
    });
    let res;
    const t0 = performance.now();
    try {
      res = await axios(url, {
        method: 'post',
        data: offerSdp,
        headers: { 'Content-Type': 'application/sdp' },
        responseType: 'text',
        signal: abortController.signal,
        timeout: SIGNALING_TIMEOUT_MS,
        validateStatus: () => true,
        transformResponse: [(data) => data],
      });
    } catch (e: any) {
      if (abortController.signal.aborted) return;
      const timedOut = e?.code === 'ECONNABORTED' || /timeout/i.test(String(e?.message || ''));
      webrtcLog('error', 'signaling_transport', {
        timedOut,
        code: e?.code || '',
        message: String(e?.message || e || 'signaling failed'),
        elapsedMs: Math.round(performance.now() - t0),
        url,
      });
      throw new Error(timedOut ? 'signaling timeout' : e?.message || 'signaling failed');
    }

    if (!mounted || generation !== sharedWebRTC.generation || sharedWebRTC.peer !== currentPeer) return;

    let body = typeof res.data === 'string' ? res.data : String(res.data ?? '');
    if (body && !body.startsWith('v=') && body.trimStart().startsWith('{')) {
      try {
        const parsed = JSON.parse(body);
        body = typeof parsed?.sdp === 'string' ? parsed.sdp : body;
        webrtcLog('info', 'signaling_json', {
          code: parsed?.code,
          type: parsed?.type,
          msg: parsed?.msg || parsed?.message,
          hasSdp: typeof parsed?.sdp === 'string',
        });
      } catch {
        /* keep raw body for error path */
      }
    }
    webrtcLog('info', 'signaling_response', {
      httpStatus: res.status,
      elapsedMs: Math.round(performance.now() - t0),
      bodyBytes: body.length,
      startsWithV: body.startsWith('v='),
      preview: body.slice(0, 80).replace(/\r?\n/g, '\\n'),
    });
    if (res.status >= 400 || !body || !body.startsWith('v=')) {
      const errText = body && !body.startsWith('v=') ? body.slice(0, 120) : `HTTP ${res.status}`;
      // Server may reject with low-mem / busy; wait then retry (matches device gate).
      if (/memory|busy|pending|retry|cooldown|session/i.test(errText)) {
        sharedWebRTC.reconnectAttempt = Math.max(sharedWebRTC.reconnectAttempt, 1);
      }
      throw new Error(`signaling rejected: ${errText}`);
    }

    const answer = { type: 'answer' as RTCSdpType, sdp: body };
    await currentPeer.setRemoteDescription(answer);
    webrtcLog('info', 'remote_answer_set', {
      signaling: currentPeer.signalingState,
      ice: currentPeer.iceConnectionState,
      conn: currentPeer.connectionState,
      answerCandidates: (body.match(/^a=candidate:/gm) || []).length,
    });
  };

  const register = async () => {
    createWebRTC();
  };

  const paly = () => {
    register();
  };

  watch(
    () => props.streamId,
    () => {
      closeSharedWebRTC();
      setSharedConnectionStatus('connecting');
      paly();
    }
  );

  onMounted(() => {
    installPageCloseHook();
    mounted = true;
    sharedWebRTC.users++;
    statusSubscribers.add(onSharedStatus);
    connectionStatus.value = sharedConnectionStatus;
    paly();
  });

  onBeforeUnmount(() => {
    mounted = false;
    statusSubscribers.delete(onSharedStatus);
    detachVideo(videoRef.value as HTMLVideoElement | undefined);
    sharedWebRTC.users = Math.max(0, sharedWebRTC.users - 1);
    scheduleSharedClose();
  });

  defineExpose({
    paly,
    getVideoElement: () => videoRef.value as HTMLVideoElement | undefined,
  });
</script>

<style lang="less" scoped>
  .webrtc-video-root {
    position: relative;
    width: 100%;
    height: 100%;
    overflow: hidden;
  }

  video {
    position: absolute;
    inset: 0;
    display: block;
    width: 100%;
    height: 100%;
    object-fit: fill;
    pointer-events: none;
  }

  .webrtc-status-overlay {
    position: absolute;
    inset: 0;
    z-index: 3;
    display: flex;
    flex-direction: column;
    align-items: center;
    justify-content: center;
    gap: 10px;
    color: #fff;
    pointer-events: none;
    background: rgba(0, 0, 0, 0.45);
  }

  .webrtc-status-text {
    font-size: 14px;
    line-height: 20px;
    text-shadow: 0 1px 2px rgba(0, 0, 0, 0.45);
  }

  .webrtc-status-spinner {
    width: 28px;
    height: 28px;
    border: 2px solid rgba(255, 255, 255, 0.35);
    border-top-color: #fff;
    border-radius: 50%;
    animation: webrtc-spin 0.8s linear infinite;
  }

  .webrtc-status-overlay.is-failed .webrtc-status-text {
    color: #ffd666;
  }

  @keyframes webrtc-spin {
    to {
      transform: rotate(360deg);
    }
  }
</style>
