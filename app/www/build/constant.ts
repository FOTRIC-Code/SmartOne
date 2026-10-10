/**
 * The name of the configuration file entered in the production environment
 */
export const GLOB_CONFIG_FILE_NAME = 'app.config.js';

// 与 net_app/Makefile 的 OUTPUT_DIR=./output 对齐：所有 application 子项目
// 都把构建产物放在自己源码目录下的 output/ 里，再由 Makefile install 拷到
// code/application/output/<name>/ 和 rootfs/var/www/。
export const OUTPUT_DIR = 'output';
