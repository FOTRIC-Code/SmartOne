import { defineStore } from 'pinia';
import { store } from '@/data';
import { ACCESS_TOKEN, CURRENT_USER } from '@/data/storageKeys';

import { storage } from '@/utils/storage';
import { DeviceData } from '@/data/models/device';
import { http } from '@/utils/http/axios';

function usernameFromToken(token: string): string {
  if (!token) return '';
  try {
    return atob(token).split(':')[0] || '';
  } catch {
    return '';
  }
}

export interface IUserState {
  token: string;
  username: string;
  welcome: string;
  avatar: string;
  permissions: any[];
  info: DeviceData | null;
  dimension: {
    w: number;
    h: number;
  };
  videoSize: {
    width: number;
    height: number;
  };
}

export const useUserStore = defineStore({
  id: 'app-user',
  state: (): IUserState => {
    const token = storage.get(ACCESS_TOKEN, '');
    return {
      token,
      username: usernameFromToken(token),
      welcome: '',
      avatar: '',
      permissions: [],
      info: storage.get(CURRENT_USER, {}),
      dimension: { w: 640, h: 480 },
      videoSize: { width: 640, height: 480 },
    };
  },
  actions: {
    setToken(token: string) {
      this.token = token;
      this.username = usernameFromToken(token);
      if (token) {
        storage.set(ACCESS_TOKEN, token);
      } else {
        storage.remove(ACCESS_TOKEN);
      }
    },
    setUsername(username: string) {
      this.username = username;
    },
    setAvatar(avatar: string) {
      this.avatar = avatar;
    },
    setPermissions(permissions) {
      this.permissions = permissions;
    },
    setUserInfo(info: DeviceData | null) {
      this.info = info;
      if (info) {
        storage.set(CURRENT_USER, info);
      } else {
        storage.remove(CURRENT_USER);
      }
    },

    async getDimension() {},

    // 获取用户信息
    async getInfo() {
      this.getDimension();
    },

    // 登出
    async logout() {
      this.setPermissions([]);
      this.setUserInfo(null);
      this.setToken('');
      storage.remove(ACCESS_TOKEN);
      storage.remove(CURRENT_USER);

      const { useAsyncRoute } = await import('@/data/stores/asyncRoute');
      useAsyncRoute().setDynamicRouteAdded(false);
    },
  },
});

// Need to be used outside the setup
export function useUser() {
  return useUserStore(store);
}
