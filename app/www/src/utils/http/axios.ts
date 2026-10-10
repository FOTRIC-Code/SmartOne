import { createAlova } from 'alova';
import adapterFetch from 'alova/fetch';
import { useUser } from '@/data/stores/user';
import { isProdMode } from '@/utils/env';
import { ResultEnum } from '@/data/enums';
import { isUrl } from '@/utils';
import { roundFloatsDeep } from '@/utils/number';

// 开发模式下需要代理的设备路径前缀
const PROXY_PATHS = [
  '/system', '/stream', '/streaming', '/image', '/thermal', '/login',
  '/security', '/osd', '/sensor', '/peri', '/alarm', '/video',
];

export const http = createAlova({
  baseURL: '',
  cacheFor: {
    POST: { mode: 'memory', expire: 0 },
    GET: { mode: 'memory', expire: 0 },
    HEAD: 0,
  },
  cacheLogger: process.env.NODE_ENV === 'development',
  requestAdapter: adapterFetch(),
  beforeRequest(method) {
    const userStore = useUser();
    const token = userStore.token;
    if (!method.meta?.ignoreToken) {
      // Cube has no user-management UI yet; use the documented factory
      // administrator credential until a user token is provisioned.
      const basicToken = token || btoa('admin:admin');
      method.config.headers['Authorization'] = `Basic ${basicToken}`;
    }

    // 开发模式：将设备路径重写为 /api 前缀以触发 Vite proxy
    const urlStr = (method.url || '') as string;
    if (!isUrl(urlStr) && !isProdMode()) {
      for (const p of PROXY_PATHS) {
        if (urlStr.startsWith(p)) {
          method.url = `/api${urlStr}`;
          return;
        }
      }
    }
    // 生产模式：同源访问，无需改写路径
  },
  responded: {
    onSuccess: async (response, method) => {
      if (response.status === 401) {
        throw new Error('Unauthorized');
      }

      // 二进制响应（抓拍等）：直接返回 blob，不走 JSON 解析
      if (method.meta?.binary) {
        if (!response.ok) {
          throw new Error(`Request failed with status ${response.status}`);
        }
        return response.blob();
      }

      const res = (response.json && (await response.json())) || response.body;
      const { message, code, data } = res ?? {};

      if (code === 401 || code === -401) {
        throw new Error('Unauthorized');
      }

      if (ResultEnum.SUCCESS === code) {
        return roundFloatsDeep(data);
      }

      // 业务失败由调用方（页面）自行弹框提示，不在此直接展示 API message
      throw new Error(message || 'Request failed');
    },
    onError: (error) => {
      throw error;
    },
  },
});
