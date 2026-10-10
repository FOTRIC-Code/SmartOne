export const alarmTriggerTypes = [
  2,
  3,
  4,
] as const;

export type AlarmTriggerType = (typeof alarmTriggerTypes)[number];
export type AlarmSensitivity = 0 | 1 | 2;
export type AlarmRecordDuration = 30 | 60;

export function isAlarmSensitivity(value: unknown): value is AlarmSensitivity {
  return value === 0 || value === 1 || value === 2;
}

export function isAlarmRecordDuration(value: unknown): value is AlarmRecordDuration {
  return value === 30 || value === 60;
}

export function normalizeAlarmSensitivity(value: unknown): AlarmSensitivity {
  return isAlarmSensitivity(value) ? value : 1;
}

export function normalizeAlarmRecordDuration(value: unknown): AlarmRecordDuration {
  return isAlarmRecordDuration(value) ? value : 30;
}

export function alarmTriggerPath(type: AlarmTriggerType): string {
  return `/alarm/triggers/${type}`;
}
