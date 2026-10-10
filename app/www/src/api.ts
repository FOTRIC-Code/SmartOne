import { http } from '@/utils/http/axios';
import {
  alarmTriggerPath,
  type AlarmRecordDuration,
  type AlarmSensitivity,
  type AlarmTriggerType,
} from '@/alarm-contract';

export type {
  AlarmRecordDuration,
  AlarmSensitivity,
  AlarmTriggerType,
} from '@/alarm-contract';

/** net_enum_provision_state */
export const PROVISION_STATE_AP_READY = 0;
export const PROVISION_STATE_PROVISIONING = 1;
export const PROVISION_STATE_PROVISIONED = 2;
export const PROVISION_STATE_FAILED = 3;

/** net_enum_network_state */
export const NETWORK_STATE_OFFLINE = 0;
export const NETWORK_STATE_ONLINE = 1;

/** net_enum_network_mode */
export const NETWORK_MODE_AP = 0;
export const NETWORK_MODE_STA = 1;

export type NetworkMode = typeof NETWORK_MODE_AP | typeof NETWORK_MODE_STA;

export interface SystemStatus {
  streaming?: boolean;
  running_path?: string;
  provision_state?: number;
  network_state?: number;
  network_mode?: NetworkMode;
  /** 热像来源：0=UVC 本地模组，1=Lantern 在线设备（缺省按 UVC 处理） */
  thermal_source?: number;
  /** AP 模式为热点名；STA 模式为已保存的 SSID（离线时同样返回） */
  ssid?: string;
  ip?: string;
}

export function getSystemStatus() {
  return http.Get<SystemStatus>('/system/status', {
    meta: { ignoreToken: true, isReturnNativeResponse: true },
  });
}

export function getDeviceInfo() {
  return http.Get('/system/device_info', {
    meta: { isReturnNativeResponse: true },
  });
}

export function setDeviceName(name: string) {
  return http.Put('/system/device_info', { name });
}

export function getSystemTime() {
  return http.Get('/system/time', {
    meta: { isReturnNativeResponse: true },
  });
}

export function setSystemTime(data: { time_mode: number; local_time: string; time_zone: string }) {
  return http.Put('/system/time', data);
}

export function getNtpServer(_id: number) {
  return http.Get(`/system/ntp_server`, {
    meta: { isReturnNativeResponse: true },
  });
}

export function setNtpServer(data: { host_name: string; port_no: number; syn_interval: number }) {
  return http.Put(`/system/ntp_server`, data);
}

export function getNetwork(id: number) {
  return http.Get(`/system/networks/${id}`, {
    meta: { isReturnNativeResponse: true },
  });
}

export function setNetwork(id: number, data: {
  id: number;
  name: string;
  dhcp: boolean;
  ip: string;
  netmask: string;
  dns: string[];
  gateway: string[];
}) {
  return http.Put(`/system/networks/${id}`, data);
}

export function getPort() {
  return http.Get('/system/port', {
    meta: { isReturnNativeResponse: true },
  });
}

export function setPort(data: { http_port_no: number; rtsp_port_no: number; rtp_port_no: number }) {
  return http.Put('/system/port', data);
}

export function restartDevice() {
  return http.Put('/system/device_restart', {});
}

export function resetDevice() {
  return http.Put('/system/device_reset', {});
}

export function upgradeDevice(formData: FormData) {
  return http.Post('/system/device_upgrade', formData, {
    meta: { isReturnNativeResponse: true },
  });
}

export interface StreamChannel {
  id: number;
  bit_rate: number;
  codec_type: string;
  disable_skip: number;
  enable_rc: number;
  gop_size: number;
  h264_profile: number;
  initial_qp: number;
  max_frame_rate: number;
  maximum_qp: number;
  quality_control_type: string;
  resolution_width: number;
  resolution_height: number;
}

export function getStreamingChannels() {
  return http.Get<StreamChannel[]>('/streaming/channels');
}

// ============================================================
// Thermal - 热像
// ============================================================

export interface ShutterMode {
  mode: number;
}

export function getShutterMode() {
  return http.Get<ShutterMode>('/thermal/thermal_shutter_mode');
}

export function setShutterMode(data: ShutterMode) {
  return http.Put('/thermal/thermal_shutter_mode', data);
}

export interface ShutterTime {
  interval: number;
}

export function getShutterTime() {
  return http.Get<ShutterTime>('/thermal/thermal_shutter_time');
}

export function setShutterTime(data: ShutterTime) {
  return http.Put('/thermal/thermal_shutter_time', data);
}

export function calibrateThermal() {
  return http.Put('/thermal/thermal_calibrate', {});
}

export interface ThermalRange {
  range_id: number;
}

export function getThermalRange() {
  return http.Get<ThermalRange>('/thermal/thermal_range');
}

export function setThermalRange(data: ThermalRange) {
  return http.Put('/thermal/thermal_range', data);
}

export interface LensRangeMap {
  id: number;
  range: { min: number; max: number };
}

export interface LensInfo {
  name?: string;
  range_map?: LensRangeMap[];
}

export function getLensInfo() {
  return http.Get<LensInfo>('/thermal/lens_info');
}

export interface ThermalParams {
  emissivity: number;
  distance: number;
  refl_temp: number;
  atm_temp: number;
  rel_humidity: number;
}

export function getThermalParams() {
  return http.Get<ThermalParams>('/thermal/thermal_params');
}

export function setThermalParams(data: ThermalParams) {
  return http.Put('/thermal/thermal_params', data);
}

export interface ThermalUnit {
  temp_unit: number;
  distance_unit: number;
}

export function getThermalUnit() {
  return http.Get<ThermalUnit>('/thermal/thermal_unit');
}

export function setThermalUnit(data: ThermalUnit) {
  return http.Put('/thermal/thermal_unit', data);
}

// ============================================================
// Image - 图像（通道固定 101；rotation 为 Parkia 扩展字段）
// ============================================================

export const IMAGE_CHANNEL_ID = 101;

export interface ImageParams {
  contrast: number;
  brightness: number;
  dde_enable: boolean;
  dde_value: number;
}

export function getImageParams(channelId = IMAGE_CHANNEL_ID) {
  return http.Get<ImageParams>(`/image/channels/${channelId}/image_params`);
}

export function setImageParams(data: ImageParams, channelId = IMAGE_CHANNEL_ID) {
  return http.Put(`/image/channels/${channelId}/image_params`, data);
}

export interface ImageFlip {
  mode: number;
  /** Parkia 扩展：旋转角度 0/90/180/270，net_app 软件实现 */
  rotation?: number;
}

export function getImageFlip(channelId = IMAGE_CHANNEL_ID) {
  return http.Get<ImageFlip>(`/image/channels/${channelId}/image_flip`);
}

export function setImageFlip(data: ImageFlip, channelId = IMAGE_CHANNEL_ID) {
  return http.Put(`/image/channels/${channelId}/image_flip`, data);
}

export interface ColorDist {
  type: number;
}

export function getColorDist() {
  return http.Get<ColorDist>('/thermal/color_dist');
}

export function setColorDist(data: ColorDist) {
  return http.Put('/thermal/color_dist', data);
}

export interface PresetPalette {
  ref_no: number;
  name?: string;
}

export function getPalettes() {
  return http.Get<PresetPalette[]>('/thermal/preset_palettes');
}

export interface Palette {
  ref_no: number;
  colors: number[];
  color_count: number;
  inverted: boolean;
}

export function getPalette() {
  return http.Get<Palette>('/thermal/palette');
}

export function setPalette(data: Palette) {
  return http.Put('/thermal/palette', data);
}

export interface GainMode {
  mode: number;
  max: number;
  min: number;
  fixedSpan: boolean;
  span: number;
}

export function getGainMode() {
  return http.Get<GainMode>('/thermal/gain');
}

export function setGainMode(data: GainMode) {
  return http.Put('/thermal/gain', data);
}

export interface IntrusionAlarm {
  enabled: boolean;
  no_repeat_minutes: number;
  sensitivity: AlarmSensitivity;
  record_duration_sec: AlarmRecordDuration;
}

export interface HighTempAlarm {
  enabled: boolean;
  no_repeat_minutes: number;
  alarm_temp: number;
  record_duration_sec: AlarmRecordDuration;
}

export interface BodyTempAlarm {
  enabled: boolean;
  sensitivity: AlarmSensitivity;
}

export function getIntrusionAlarm() {
  return http.Get<IntrusionAlarm>('/thermal/intrusion/alarm');
}
export function setIntrusionAlarm(data: IntrusionAlarm) {
  return http.Put('/thermal/intrusion/alarm', data);
}

export function getHighTempAlarm() {
  return http.Get<HighTempAlarm>('/thermal/high_temp/alarm');
}
export function setHighTempAlarm(data: HighTempAlarm) {
  return http.Put('/thermal/high_temp/alarm', data);
}

export function getBodyTempAlarm() {
  return http.Get<BodyTempAlarm>('/thermal/body_temp/alarm');
}
export function setBodyTempAlarm(data: BodyTempAlarm) {
  return http.Put('/thermal/body_temp/alarm', data);
}

export interface AlarmTrigger {
  type?: AlarmTriggerType;
  trigger_ftp_enabled: boolean;
  trigger_webhook_enabled: boolean;
  trigger_record_enabled: boolean;
}

export function getAlarmTrigger(type: AlarmTriggerType) {
  return http.Get<AlarmTrigger>(alarmTriggerPath(type));
}

export function setAlarmTrigger(type: AlarmTriggerType, data: AlarmTrigger) {
  return http.Put(alarmTriggerPath(type), data);
}

export interface FtpInfo {
  use_ssl: boolean;
  ip: string;
  port_no: number;
  user_name: string;
  pwd: string;
  enable_annoy: boolean;
  path_template: string;
}

export function getSystemFtp() {
  return http.Get<FtpInfo>('/system/ftp');
}

export function setSystemFtp(data: FtpInfo) {
  return http.Put('/system/ftp', data);
}

export function testSystemFtp(data: FtpInfo) {
  return http.Post('/system/ftp/test', data);
}

export interface WebhookInfo {
  url: string;
}

export function getSystemWebhook() {
  return http.Get<WebhookInfo>('/system/webhook');
}

export function setSystemWebhook(data: WebhookInfo) {
  return http.Put('/system/webhook', data);
}
