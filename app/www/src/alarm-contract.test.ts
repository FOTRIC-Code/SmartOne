import { describe, expect, it } from 'vitest';
import {
  alarmTriggerPath,
  alarmTriggerTypes,
  isAlarmRecordDuration,
  isAlarmSensitivity,
  normalizeAlarmRecordDuration,
  normalizeAlarmSensitivity,
} from './alarm-contract';
import type { AlarmTrigger } from './api';

describe('alarm contract', () => {
  it('maps every public alarm type to its trigger path', () => {
    expect(alarmTriggerTypes.map(alarmTriggerPath)).toEqual([
      '/alarm/triggers/2',
      '/alarm/triggers/3',
      '/alarm/triggers/4',
    ]);
  });

  it('defines all per-alarm linkage switches', () => {
    const trigger: AlarmTrigger = {
      type: 2,
      trigger_ftp_enabled: false,
      trigger_webhook_enabled: false,
      trigger_record_enabled: true,
    };
    expect(Object.keys(trigger)).toEqual([
      'type',
      'trigger_ftp_enabled',
      'trigger_webhook_enabled',
      'trigger_record_enabled',
    ]);
  });

  it('accepts only customer-contract sensitivity values', () => {
    expect([0, 1, 2].every(isAlarmSensitivity)).toBe(true);
    expect([-1, 3, '1'].some(isAlarmSensitivity)).toBe(false);
  });

  it('accepts only supported recording durations', () => {
    expect([30, 60].every(isAlarmRecordDuration)).toBe(true);
    expect([0, 10, 20, 90, '30'].some(isAlarmRecordDuration)).toBe(false);
  });

  it('normalizes malformed device values to safe defaults', () => {
    expect(normalizeAlarmSensitivity(7)).toBe(1);
    expect(normalizeAlarmRecordDuration(45)).toBe(30);
    expect(normalizeAlarmRecordDuration(10)).toBe(30);
  });
});
