export interface ProjectSettingState {
  //导航模式
  navMode: string;
  //导航风格
  navTheme: string;
  //顶部设置
  headerSetting: object;
  //页脚
  showFooter: boolean;
  //菜单设置
  menuSetting: object;
  //多标签
  multiTabsSetting: object;
  //面包屑
  crumbsSetting: object;
  //权限模式
  permissionMode: string;
}

export interface IBodySetting {
  fixed: boolean;
}

export interface IHeaderSetting {
  fixed: boolean;
  isReload: boolean;
}

export interface IMenuSetting {
  minMenuWidth: number;
  menuWidth: number;
  fixed: boolean;
  mixMenu: boolean;
  collapsed: boolean;
  mobileWidth: number;
}

export interface ICrumbsSetting {
  show: boolean;
  showIcon: boolean;
}

export interface IMultiTabsSetting {
  fixed: boolean;
  show: boolean;
}

export interface GlobConfig {
  title: string;
  shortName: string;
  webrtcUrl?: string;
  webrtcUrlPort?: string;
}

export interface GlobEnvConfig {
  VITE_GLOB_APP_TITLE: string;
  VITE_GLOB_APP_SHORT_NAME: string;
  VITE_GLOB_API_WEBRTC_URL?: string;
  VITE_GLOB_API_WEBRTC_PORT?: string;
}
