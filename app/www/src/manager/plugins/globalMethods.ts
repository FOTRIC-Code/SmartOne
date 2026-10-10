import i18n from '@/manager/i18n';

/**
 * 注册全局方法
 * @param app
 */
export async function setupGlobalMethods(app) {
  app.use(i18n);
  app.config.globalProperties.$t = (...args: Parameters<typeof i18n.global.t>) =>
    i18n.global.t(...args);
  window['$t'] = (...args: any[]) => i18n.global.t(...args);
}
