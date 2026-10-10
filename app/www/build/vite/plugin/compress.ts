/**
 * 产物 gzip 预压缩。配合板上 busybox httpd 1.36（CONFIG_FEATURE_HTTPD_GZIP=y）使用：
 * 浏览器请求 /bundle.js 且 Accept-Encoding: gzip 时，httpd 会自动 open(/bundle.js.gz)
 * 并加 Content-Encoding: gzip 头返回，这样 NOR flash 上只用存 .gz。
 *
 * 关键约束（不要随便改 filter）：
 *   - **不能压 .html**。busybox httpd GET / 处理（networking/httpd.c:2419-2451）
 *     先 stat("index.html")，失败就直接 404，**不会**进 send_file 里的 gzip-fallback。
 *     所以 index.html 必须保留实体未压缩，filter 故意排除掉。
 *   - 压 .js / .css / .json / .svg 这几类纯文本资源，PNG/JPG 已自带压缩别再压。
 *   - threshold 1024：小于 1KB 的文件 gzip 后不见得更小，且会带额外 .gz header 开销。
 *
 * 详细规范见 docs/design-docs/web-www-size-optimization.md。
 */
import type { Plugin } from 'vite';

import compressPlugin from 'vite-plugin-compression';

const COMPRESS_FILTER = /\.(js|mjs|json|css|svg)$/i;
const COMPRESS_THRESHOLD = 1024;

export function configCompressPlugin(
  compress: 'gzip' | 'brotli' | 'none' = 'none',
  deleteOriginFile = false
): Plugin | Plugin[] {
  const compressList = compress.split(',');

  const plugins: Plugin[] = [];

  if (compressList.includes('gzip')) {
    plugins.push(
      compressPlugin({
        ext: '.gz',
        deleteOriginFile,
        threshold: COMPRESS_THRESHOLD,
        filter: COMPRESS_FILTER,
      })
    );
  }
  if (compressList.includes('brotli')) {
    plugins.push(
      compressPlugin({
        ext: '.br',
        algorithm: 'brotliCompress',
        deleteOriginFile,
        threshold: COMPRESS_THRESHOLD,
        filter: COMPRESS_FILTER,
      })
    );
  }
  return plugins;
}
