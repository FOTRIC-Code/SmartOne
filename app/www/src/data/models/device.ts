export type DeviceFirmwareVersion = [number, number, number, number];

export interface DeviceFirmware {
  build: string;
  name: string;
  version: DeviceFirmwareVersion;
}

export interface DeviceData {
  'create-date': string;
  'device-fw': DeviceFirmware;
  'device-id': string;
  'device-model': string;
  'device-sn0': string;
  'device-sn1': string;
  'device-tag0': string;
  'device-tag1': string;
  'device-tag2': string;
  'device-version': string;
  fpga: string;
  'fpga-version': string;
  hardware: string;
  'hardware-version': string;
  'modify-date': string;
  'seal-date': string;
  sensor: string;
  'sensor-sn': string;
  'update-counter': number;
}
