/** 温度单位枚举：0=Celsius, 1=Kelvin, 2=Fahrenheit（与 net_app net_enum_temp_unit 保持一致） */
export enum TempUnit {
  Celsius = 0,
  Kelvin = 1,
  Fahrenheit = 2,
}

/** 距离单位枚举：0=Meter, 1=Feet */
export enum DistanceUnit {
  Meter = 0,
  Feet = 1,
}

/** 单位配置（设备 /thermal/thermal_unit 接口模型） */
export interface ThermalUnit {
  temp_unit: TempUnit;
  distance_unit: DistanceUnit;
}

export const TEMP_UNIT_LABELS: Record<TempUnit, string> = {
  [TempUnit.Celsius]: '°C',
  [TempUnit.Kelvin]: 'K',
  [TempUnit.Fahrenheit]: '°F',
};

export const DISTANCE_UNIT_LABELS: Record<DistanceUnit, string> = {
  [DistanceUnit.Meter]: 'm',
  [DistanceUnit.Feet]: 'ft',
};

export const formatUnitNumber = (value: number) => {
  if (!Number.isFinite(value)) return '';
  return String(Number(value.toFixed(2)));
};

export const convertKelvinToTempUnit = (value: number, unit: TempUnit) => {
  if (unit === TempUnit.Celsius) return value - 273.15;
  if (unit === TempUnit.Fahrenheit) return ((value - 273.15) * 9) / 5 + 32;
  return value;
};

export const convertTempUnitToKelvin = (value: number, unit: TempUnit) => {
  if (unit === TempUnit.Celsius) return value + 273.15;
  if (unit === TempUnit.Fahrenheit) return ((value - 32) * 5) / 9 + 273.15;
  return value;
};

export const convertMetersToDistanceUnit = (value: number, unit: DistanceUnit) =>
  unit === DistanceUnit.Feet ? value * 3.28084 : value;

export const convertDistanceUnitToMeters = (value: number, unit: DistanceUnit) =>
  unit === DistanceUnit.Feet ? value / 3.28084 : value;

export const formatTemperatureFromKelvin = (value: number, unit: TempUnit) =>
  formatUnitNumber(convertKelvinToTempUnit(value, unit));
