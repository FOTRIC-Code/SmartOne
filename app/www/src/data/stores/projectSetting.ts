import { defineStore } from 'pinia';
import { computed } from 'vue';
import projectSetting from '@/manager/config/projectSetting';
import type { IHeaderSetting, IMenuSetting, IMultiTabsSetting, ICrumbsSetting } from '/#/config';

const {
  navMode,
  navTheme,
  isMobile,
  headerSetting,
  showFooter,
  menuSetting,
  multiTabsSetting,
  crumbsSetting,
  permissionMode,
  isPageAnimate,
  pageAnimateType,
} = projectSetting;

interface ProjectSettingState {
  navMode: string; //导航模式
  navTheme: string; //导航风格
  headerSetting: IHeaderSetting; //顶部设置
  showFooter: boolean; //页脚
  menuSetting: IMenuSetting; //多标签
  multiTabsSetting: IMultiTabsSetting; //多标签
  crumbsSetting: ICrumbsSetting; //面包屑
  permissionMode: string; //权限模式
  isPageAnimate: boolean; //是否开启路由动画
  pageAnimateType: string; //路由动画类型
  isMobile: boolean; // 是否处于移动端模式
}

export const useProjectSettingStore = defineStore({
  id: 'app-project-setting',
  state: (): ProjectSettingState => ({
    navMode: navMode,
    navTheme,
    isMobile,
    headerSetting,
    showFooter,
    menuSetting,
    multiTabsSetting,
    crumbsSetting,
    permissionMode,
    isPageAnimate,
    pageAnimateType,
  }),
  getters: {
    getNavMode(): string {
      return this.navMode;
    },
    getNavTheme(): string {
      return this.navTheme;
    },
    getIsMobile(): boolean {
      return this.isMobile;
    },
    getHeaderSetting(): object {
      return this.headerSetting;
    },
    getShowFooter(): boolean {
      return this.showFooter;
    },
    getMenuSetting(): object {
      return this.menuSetting;
    },
    getMultiTabsSetting(): object {
      return this.multiTabsSetting;
    },
    getCrumbsSetting(): object {
      return this.crumbsSetting;
    },
    getPermissionMode(): string {
      return this.permissionMode;
    },
    getIsPageAnimate(): boolean {
      return this.isPageAnimate;
    },
    getPageAnimateType(): string {
      return this.pageAnimateType;
    },
  },
  actions: {
    setNavTheme(value: string): void {
      this.navTheme = value;
    },
    setIsMobile(value: boolean): void {
      this.isMobile = value;
    },
  },
});

/**
 * 在 setup 内使用，返回项目设置相关的 computed 引用集合
 */
export function useProjectSetting() {
  const projectStore = useProjectSettingStore();

  const navMode = computed(() => projectStore.navMode);
  const navTheme = computed(() => projectStore.navTheme);
  const isMobile = computed(() => projectStore.isMobile);
  const headerSetting = computed(() => projectStore.headerSetting);
  const multiTabsSetting = computed(() => projectStore.multiTabsSetting);
  const menuSetting = computed(() => projectStore.menuSetting);
  const crumbsSetting = computed(() => projectStore.crumbsSetting);
  const permissionMode = computed(() => projectStore.permissionMode);
  const showFooter = computed(() => projectStore.showFooter);
  const isPageAnimate = computed(() => projectStore.isPageAnimate);
  const pageAnimateType = computed(() => projectStore.pageAnimateType);

  return {
    navMode,
    navTheme,
    isMobile,
    headerSetting,
    multiTabsSetting,
    menuSetting,
    crumbsSetting,
    permissionMode,
    showFooter,
    isPageAnimate,
    pageAnimateType,
  };
}
