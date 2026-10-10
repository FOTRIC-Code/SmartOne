import { defineStore } from 'pinia';
import { store } from '@/data';
import { getSystemStatus } from '@/api';
import { ThermalSource } from '@/data/enums';

export interface ThermalSourceState {
  source: ThermalSource;
  loaded: boolean;
}

/** 当前热像来源（UVC 本地模组 / Lantern 在线设备），部分能力仅 Lantern 可用 */
export const useThermalSourceStore = defineStore({
  id: 'app-thermal-source',
  state: (): ThermalSourceState => ({
    source: ThermalSource.Uvc,
    loaded: false,
  }),
  getters: {
    isLantern: (state) => state.source === ThermalSource.Lantern,
  },
  actions: {
    async fetchSource(force = false) {
      if (this.loaded && !force) return;

      const res = await getSystemStatus().catch(() => null);
      this.source =
        res?.thermal_source === ThermalSource.Lantern ? ThermalSource.Lantern : ThermalSource.Uvc;
      this.loaded = true;
    },
  },
});

export function useThermalSource() {
  return useThermalSourceStore(store);
}
