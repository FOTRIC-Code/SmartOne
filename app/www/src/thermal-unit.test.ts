import { describe, it, expect } from 'vitest';
import {
  TempUnit,
  DistanceUnit,
  convertKelvinToTempUnit,
  convertTempUnitToKelvin,
  convertMetersToDistanceUnit,
  convertDistanceUnitToMeters,
  TEMP_UNIT_LABELS,
} from './data/models/thermal';

/* 契约：temp_unit 0=℃ 1=K 2=℉（与 Volga net_enum_temp_unit、net_app net_enums 一致） */
describe('thermal unit conversion', () => {
  it('kelvin to display by configured unit', () => {
    expect(convertKelvinToTempUnit(293.15, TempUnit.Celsius)).toBeCloseTo(20.0);
    expect(convertKelvinToTempUnit(293.15, TempUnit.Kelvin)).toBeCloseTo(293.15);
    expect(convertKelvinToTempUnit(293.15, TempUnit.Fahrenheit)).toBeCloseTo(68.0);
  });

  it('display back to kelvin', () => {
    expect(convertTempUnitToKelvin(20.0, TempUnit.Celsius)).toBeCloseTo(293.15);
    expect(convertTempUnitToKelvin(293.15, TempUnit.Kelvin)).toBeCloseTo(293.15);
    expect(convertTempUnitToKelvin(68.0, TempUnit.Fahrenheit)).toBeCloseTo(293.15);
  });

  it('lens range example from Volga doc', () => {
    // range_map { min: 253.15, max: 423.15 } K → ℃ 显示 -20 ~ 150
    expect(convertKelvinToTempUnit(253.15, TempUnit.Celsius)).toBeCloseTo(-20.0);
    expect(convertKelvinToTempUnit(423.15, TempUnit.Celsius)).toBeCloseTo(150.0);
  });

  it('distance conversion', () => {
    expect(convertMetersToDistanceUnit(1, DistanceUnit.Feet)).toBeCloseTo(3.28084);
    expect(convertDistanceUnitToMeters(3.28084, DistanceUnit.Meter)).toBeCloseTo(3.28084);
    expect(convertDistanceUnitToMeters(3.28084, DistanceUnit.Feet)).toBeCloseTo(1);
  });

  it('labels follow enum values', () => {
    expect(TEMP_UNIT_LABELS[TempUnit.Celsius]).toBe('°C');
    expect(TEMP_UNIT_LABELS[TempUnit.Kelvin]).toBe('K');
    expect(TEMP_UNIT_LABELS[TempUnit.Fahrenheit]).toBe('°F');
  });
});
