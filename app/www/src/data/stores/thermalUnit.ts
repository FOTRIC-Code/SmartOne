import { defineStore } from 'pinia';
import { store } from '@/data';
import { getThermalUnit } from '@/api';
import {
  DistanceUnit,
  DISTANCE_UNIT_LABELS,
  formatTemperatureFromKelvin,
  TempUnit,
  TEMP_UNIT_LABELS,
  type ThermalUnit,
} from '@data/models/thermal';

export interface ThermalUnitState {
  tempUnit: TempUnit;
  distanceUnit: DistanceUnit;
  loaded: boolean;
}

export const useThermalUnitStore = defineStore({
  id: 'app-thermal-unit',
  state: (): ThermalUnitState => ({
    tempUnit: TempUnit.Celsius,
    distanceUnit: DistanceUnit.Meter,
    loaded: false,
  }),
  getters: {
    tempUnitLabel: (state) => TEMP_UNIT_LABELS[state.tempUnit] ?? TEMP_UNIT_LABELS[TempUnit.Celsius],
    distanceUnitLabel: (state) =>
      DISTANCE_UNIT_LABELS[state.distanceUnit] ?? DISTANCE_UNIT_LABELS[DistanceUnit.Meter],
    formatTemperatureFromKelvin: (state) => (value: number) =>
      formatTemperatureFromKelvin(value, state.tempUnit),
  },
  actions: {
    setUnit(data: Partial<ThermalUnit>) {
      this.tempUnit = data.temp_unit ?? TempUnit.Celsius;
      this.distanceUnit = data.distance_unit ?? DistanceUnit.Meter;
      this.loaded = true;
    },
    async fetchUnit(force = false) {
      if (this.loaded && !force) return;

      const res = await getThermalUnit().catch(() => null);
      if (!res) return;
      this.setUnit(res);
    },
  },
});

export function useThermalUnit() {
  return useThermalUnitStore(store);
}
