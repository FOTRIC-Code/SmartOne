import { useGlobSetting } from '@/utils/env';

const globSetting = useGlobSetting();

export function getWebRTCUrl(): string {
  if (globSetting.webrtcUrl) {
    const port = globSetting.webrtcUrlPort ? `:${globSetting.webrtcUrlPort}` : '';
    return `${globSetting.webrtcUrl}${port}`;
  }
  /* Same-origin :80 proxy; device sets rtc.externIP from Host (USB / SoftAP / STA). */
  return `${location.protocol}//${location.host}`;
}
