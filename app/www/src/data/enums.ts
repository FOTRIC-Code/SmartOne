// ============================================================
// pageEnum - 页面路径
// ============================================================

export enum PageEnum {
  BASE_LOGIN = '/login',
  BASE_LOGIN_NAME = 'Login',
  REDIRECT = '/redirect',
  REDIRECT_NAME = 'Redirect',
  BASE_HOME = '/home',
  BASE_HOME_REDIRECT = '/home/index',
  BASE_LIVE = '/live',
  ERROR_PAGE_NAME = 'ErrorPage',
}

export enum ResultEnum {
  SUCCESS = 0,
}

export enum TimeCheckType {
  NET_TIME_MODE_LOCAL = 0,
  NET_TIME_MODE_NTP = 1,
}

/** 热像来源（system/status 的 thermal_source）：0=UVC 本地模组，1=Lantern 在线设备 */
export enum ThermalSource {
  Uvc = 0,
  Lantern = 1,
}

/** 温宽/增益模式（/thermal/gain 的 mode）：1=固定/手动温宽，2=自动温宽 */
export enum TempWidthMode {
  Fixed = 1,
  Auto = 2,
}

/** 色彩分布类型（/thermal/color_dist 的 type）：1=高温差均衡成像，2=温度线性，3=信号线性 */
export enum ColorDistType {
  TTwb = 1,
  TempLinear = 2,
  AdLinear = 3,
}

/** 图像旋转角度（image_flip 的 rotation 扩展字段，由 net_app 软件实现） */
export enum ImageRotation {
  None = 0,
  R90 = 90,
  R180 = 180,
  R270 = 270,
}
