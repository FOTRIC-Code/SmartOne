import { App } from 'vue';
import { createRouter, createWebHashHistory, RouteRecordRaw } from 'vue-router';
import { renderIcon } from '@/utils/index';
import { HomeOutline, VideocamOutline, SettingsOutline } from '@vicons/ionicons5';
import { useAsyncRoute } from '@/data/stores/asyncRoute';
import { PageEnum } from '@/data/enums';
import { isNavigationFailure } from 'vue-router';
import { translateRouteTitle } from '@/manager/i18n';
import { useGlobSetting } from '@/utils/env';

const Layout = () => import('@/components/Layout/index.vue');
const SettingTabShell = () => import('@/views/setting/components/SettingTabShell.vue');

/** Cube API–backed settings only: Connect / Smart / Device */
export const settingRoutes: RouteRecordRaw[] = [
  {
    path: 'connect',
    name: 'setting_connect',
    meta: { title: 'route.connect', activeMenu: 'setting_index', icon: 'NetworkOutlined' },
    component: SettingTabShell,
    redirect: '/setting/base/connect/tcp-ip',
    children: [
      { path: 'tcp-ip', name: 'setting_connect_tcp_ip', meta: { title: 'route.TCP/IP', activeMenu: 'setting_index', parentMenu: 'setting_connect' }, component: () => import('@/views/setting/network/tcp-ip/index.vue') },
      { path: 'port', name: 'setting_connect_port', meta: { title: 'route.portConfig', activeMenu: 'setting_index', parentMenu: 'setting_connect' }, component: () => import('@/views/setting/network/port/index.vue') },
      { path: 'ftp', name: 'setting_connect_ftp', meta: { title: 'route.ftpConfig', activeMenu: 'setting_index', parentMenu: 'setting_connect' }, component: () => import('@/views/setting/network/ftp/index.vue') },
      { path: 'webhook', name: 'setting_connect_webhook', meta: { title: 'route.webhookConfig', activeMenu: 'setting_index', parentMenu: 'setting_connect' }, component: () => import('@/views/setting/network/webhook/index.vue') },
    ],
  },
  {
    path: 'smart',
    name: 'setting_smart',
    meta: { title: 'route.smart', activeMenu: 'setting_index', icon: 'ThermalOutlined' },
    component: SettingTabShell,
    redirect: '/setting/base/smart/intrusion',
    children: [
      { path: 'intrusion', name: 'setting_smart_intrusion', meta: { title: 'route.smartIntrusion', activeMenu: 'setting_index', parentMenu: 'setting_smart' }, component: () => import('@/views/setting/smart/intrusion/index.vue') },
      { path: 'high-temp', name: 'setting_smart_high_temp', meta: { title: 'route.smartHighTemp', activeMenu: 'setting_index', parentMenu: 'setting_smart' }, component: () => import('@/views/setting/smart/high-temp/index.vue') },
      { path: 'body-temp', name: 'setting_smart_body_temp', meta: { title: 'route.smartBodyTemp', activeMenu: 'setting_index', parentMenu: 'setting_smart' }, component: () => import('@/views/setting/smart/body-temp/index.vue') },
    ],
  },
  {
    path: 'thermal',
    name: 'setting_thermal',
    meta: { title: 'route.thermalManagement', activeMenu: 'setting_index', icon: 'FlameOutlined' },
    component: SettingTabShell,
    redirect: '/setting/base/thermal/basic',
    children: [
      { path: 'basic', name: 'setting_thermal_basic', meta: { title: 'route.thermalBasicConfig', activeMenu: 'setting_index', parentMenu: 'setting_thermal' }, component: () => import('@/views/setting/thermal/basic/index.vue') },
    ],
  },
  {
    path: 'image',
    name: 'setting_image',
    meta: { title: 'route.imageManagement', activeMenu: 'setting_index', icon: 'ImageOutlined' },
    component: SettingTabShell,
    redirect: '/setting/base/image/display',
    children: [
      { path: 'display', name: 'setting_image_display', meta: { title: 'route.displayConfig', activeMenu: 'setting_index', parentMenu: 'setting_image' }, component: () => import('@/views/setting/image/display/index.vue') },
    ],
  },
  {
    path: 'device',
    name: 'setting_device',
    meta: { title: 'route.device', activeMenu: 'setting_index', icon: 'SystemOutlined' },
    component: SettingTabShell,
    redirect: '/setting/base/device/base-info',
    children: [
      { path: 'base-info', name: 'setting_device_base_info', meta: { title: 'route.baseInfo', activeMenu: 'setting_index', parentMenu: 'setting_device' }, component: () => import('@/views/setting/system/base-info/index.vue') },
      { path: 'time', name: 'setting_device_time', meta: { title: 'route.timeConfig', activeMenu: 'setting_index', parentMenu: 'setting_device' }, component: () => import('@/views/setting/system/time/index.vue') },
      { path: 'unit', name: 'setting_device_unit', meta: { title: 'route.unitConfig', activeMenu: 'setting_index', parentMenu: 'setting_device' }, component: () => import('@/views/setting/system/unit/index.vue') },
      { path: 'maintenance', name: 'setting_device_maintenance', meta: { title: 'route.deviceMaintenance', activeMenu: 'setting_index', parentMenu: 'setting_device' }, component: () => import('@/views/setting/system/maintenance/index.vue') },
    ],
  },
];

export const appRoutes: RouteRecordRaw[] = [
  {
    path: '/home',
    name: 'home',
    component: Layout,
    meta: { sort: 0, isRoot: true, activeMenu: 'home_index', icon: renderIcon(HomeOutline) },
    redirect: '/home/index',
    children: [
      {
        path: 'index',
        name: 'home_index',
        meta: { title: 'route.home', activeMenu: 'home_index' },
        component: () => import('@/views/home/index.vue'),
      },
    ],
  },
  {
    path: '/live',
    name: 'live',
    component: Layout,
    meta: { sort: 1, isRoot: true, activeMenu: 'live_index', icon: renderIcon(VideocamOutline) },
    redirect: '/live/index',
    children: [
      {
        path: 'index',
        name: 'live_index',
        meta: { title: 'route.live', activeMenu: 'live_index' },
        component: () => import('@/views/preview/index.vue'),
      },
    ],
  },
  {
    path: '/setting',
    name: 'setting',
    component: Layout,
    meta: { sort: 2, isRoot: true, activeMenu: 'setting_index', icon: renderIcon(SettingsOutline) },
    children: [
      {
        path: 'base',
        name: 'setting_index',
        meta: { title: 'route.settings', activeMenu: 'setting_index' },
        redirect: '/setting/base/smart/intrusion',
        component: () => import('@/views/setting/index.vue'),
        children: settingRoutes,
      },
    ],
  },
];

const routes: RouteRecordRaw[] = [
  { path: '/', name: 'Root', redirect: PageEnum.BASE_HOME, meta: { title: 'Root' } },
  /* login removed — device APIs are open; keep redirect for old bookmarks */
  { path: '/login', redirect: PageEnum.BASE_HOME },
  /* legacy redirects */
  { path: '/preview', redirect: '/live' },
  { path: '/preview/index', redirect: '/live/index' },
  { path: '/setup', redirect: '/home' },
  {
    path: '/redirect',
    name: 'Redirect',
    component: Layout,
    meta: { title: 'Redirect', hideBreadcrumb: true },
    children: [
      { path: '/redirect/:path(.*)', name: 'RedirectSon', component: () => import('@/views/redirect/index.vue'), meta: { title: 'Redirect', hideBreadcrumb: true } },
    ],
  },
  ...appRoutes,
  {
    path: '/:path(.*)*',
    name: 'ErrorPage',
    component: Layout,
    meta: { title: 'ErrorPage', hideBreadcrumb: true },
    children: [
      { path: '/:path(.*)*', name: 'ErrorPageSon', component: () => import('@/views/exception/404.vue'), meta: { title: 'ErrorPage', hideBreadcrumb: true } },
    ],
  },
];

const router = createRouter({
  history: createWebHashHistory(''),
  routes,
  strict: true,
  scrollBehavior: () => ({ left: 0, top: 0 }),
});

export function setupRouter(app: App) {
  app.use(router);

  const asyncRouteStore = useAsyncRoute();

  router.beforeEach(async (to, _from, next) => {
    const Loading = window['$loading'] || null;
    Loading && Loading.start();

    if (!asyncRouteStore.isDynamicRouteAdded) {
      asyncRouteStore.setMenus(appRoutes);
      asyncRouteStore.setDynamicRouteAdded(true);
    }

    next();
    Loading && Loading.finish();
  });

  router.afterEach((to, _, failure) => {
    if (to.path.startsWith('/home')) {
      document.title = translateRouteTitle('route.home');
    } else if (to.path.startsWith('/live') || to.path.startsWith('/preview')) {
      document.title = translateRouteTitle('route.live');
    } else if (to.path.startsWith('/setting')) {
      document.title = translateRouteTitle('route.settings');
    } else {
      document.title = translateRouteTitle(to.meta?.title) || useGlobSetting().title;
    }
    if (isNavigationFailure(failure)) {
      // navigation failure
    }
    const keepAliveComponents = asyncRouteStore.keepAliveComponents;
    const currentComName: any = to.matched.find((item) => item.name == to.name)?.name;
    if (currentComName && !keepAliveComponents.includes(currentComName) && to.meta?.keepAlive) {
      keepAliveComponents.push(currentComName);
    } else if (!to.meta?.keepAlive || to.name == 'Redirect') {
      const index = keepAliveComponents.findIndex((name) => name == currentComName);
      if (index != -1) {
        keepAliveComponents.splice(index, 1);
      }
    }
    asyncRouteStore.setKeepAliveComponents(keepAliveComponents);
    const Loading = window['$loading'] || null;
    Loading && Loading.finish();
  });

  router.onError((error) => {
    console.log(error, '路由错误');
  });
}

export default router;
