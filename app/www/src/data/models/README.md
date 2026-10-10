# models

API 请求 / 响应数据结构（领域模型）。
约定：每个领域一个文件（如 `device.ts`、`user.ts`、`network.ts`），
导出 `interface` / `type`，由 `src/api/*.ts` 中的请求函数和 `src/data/stores/*` 共同消费。

后续从 `src/api/*.ts` 中抽出新增的响应模型时，请按领域归类到本目录，
并在 API 文件中以 `import type { ... } from '@/data/models/<domain>'` 引用，
保持「API 调用」与「数据结构」的关注点分离。
