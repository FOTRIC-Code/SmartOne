import type { GlobConfig, GlobEnvConfig } from '/#/config';

import { warn } from '@/utils/log';
import { getConfigFileName } from '../../build/getConfigFileName';

function getAppEnvConfig() {
  const ENV_NAME = getConfigFileName(import.meta.env);

  const ENV = (import.meta.env.DEV
    ? (import.meta.env as unknown as GlobEnvConfig)
    : window[ENV_NAME as any]) as unknown as GlobEnvConfig;

  const {
    VITE_GLOB_APP_TITLE,
    VITE_GLOB_APP_SHORT_NAME,
    VITE_GLOB_API_WEBRTC_URL,
    VITE_GLOB_API_WEBRTC_PORT,
  } = ENV;

  if (!/^[a-zA-Z\_]*$/.test(VITE_GLOB_APP_SHORT_NAME)) {
    warn(
      `VITE_GLOB_APP_SHORT_NAME Variables can only be characters/underscores, please modify in the environment variables and re-running.`
    );
  }

  return {
    VITE_GLOB_APP_TITLE,
    VITE_GLOB_APP_SHORT_NAME,
    VITE_GLOB_API_WEBRTC_URL,
    VITE_GLOB_API_WEBRTC_PORT,
  };
}

export const useGlobSetting = (): Readonly<GlobConfig> => {
  const {
    VITE_GLOB_APP_TITLE,
    VITE_GLOB_APP_SHORT_NAME,
    VITE_GLOB_API_WEBRTC_PORT,
    VITE_GLOB_API_WEBRTC_URL,
  } = getAppEnvConfig();

  const glob: Readonly<GlobConfig> = {
    title: VITE_GLOB_APP_TITLE,
    shortName: VITE_GLOB_APP_SHORT_NAME,
    webrtcUrl: VITE_GLOB_API_WEBRTC_URL,
    webrtcUrlPort: VITE_GLOB_API_WEBRTC_PORT,
  };
  return glob as Readonly<GlobConfig>;
};

export function isProdMode(): boolean {
  return import.meta.env.PROD;
}
