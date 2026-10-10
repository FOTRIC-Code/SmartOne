import { defineStore } from 'pinia';
import { RouteRecordRaw } from 'vue-router';
import { store } from '@/data';

export interface IAsyncRouteState {
  menus: RouteRecordRaw[];
  keepAliveComponents: string[];
  isDynamicRouteAdded: boolean;
}

export const useAsyncRouteStore = defineStore({
  id: 'app-async-route',
  state: (): IAsyncRouteState => ({
    menus: [],
    keepAliveComponents: [],
    isDynamicRouteAdded: false,
  }),
  actions: {
    setDynamicRouteAdded(added: boolean) {
      this.isDynamicRouteAdded = added;
    },
    setMenus(menus: RouteRecordRaw[]) {
      this.menus = menus;
    },
    setKeepAliveComponents(compNames: string[]) {
      this.keepAliveComponents = compNames;
    },
  },
});

export function useAsyncRoute() {
  return useAsyncRouteStore(store);
}
