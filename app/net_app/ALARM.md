# Parkia.Cube net_app 报警功能实现说明

本文描述 net_app 三类智能报警（入侵 / 高温 / 体温监护）的检测算法、配置、RTP 推送与联动（FTP / Webhook / 录像）的**运行时实现**。HTTP 接口表与配置落盘路径见 `ARCHITECTURE.md` §7.8–§7.10。

---

## 1. 模块边界

| 模块 | 职责 |
| --- | --- |
| `thermal/` | UVC 取帧、全画面最高温、JPEG 快照、BGR 预览帧、最新 raw 拷贝；**不**做报警判定 |
| `alarm/` | 周期检测、隔秒帧差、组装 `alarm_event_t`、体温分钟采样与照片相册 |
| `main_process/` | 报警队列消费、规划录像路径、异步 FTP / Webhook |
| `libs/ftp/` | FTP / FTPS 缓冲上传（libcurl） |
| `rtp/` | 报警会话握手与二进制帧发送 |
| `video_process/` | 事件 MP4 录像（ZLM，含预录）；照片合成 MP4 |
| `http_api/` | 报警 / 联动 / FTP / Webhook 配置读写 |

约定：

- 报警层**不拥有**推流逻辑；流只在 `video_process`。
- FTP 快照**不经** `video_process`，由 `thermal_provider_capture_jpeg()` 直接从最新预览帧编码。
- 检测线程直接读 `g_config`，不以 `config_get_*` 做每秒判定。

关键文件：

- `alarm/alarm_monitor.c`、`alarm/intrusion_motion.c`、`alarm/body_temp_history.c`、`alarm/photo_album.c`
- `main_process/main_process.c`、`main_process/alarm_transfer.c`、`main_process/main_process_tasks.c`
- `thermal/thermal_uvc_bridge.cpp`、`thermal/thermal_data_provider.*`
- `rtp/rtp_server.c`、`rtp/rtp_wire.c`
- `video_process/video_process.c`、`video_process/stills_mp4.c`

---

## 2. 报警类型

| 值 | 宏 | 说明 | 数据源 | 周期 |
| --- | --- | --- | --- | --- |
| `2` | `NET_ALARM_INTRUSION` | 入侵 | 相隔 1 秒的两帧 raw | 1 s |
| `3` | `NET_ALARM_HIGH_TEMP` | 高温 | 全画面最高温 | 1 s |
| `4` | `NET_ALARM_BODY_TEMP` | 体温监护 | 最高温分钟中位数序列 | 10 min 一轮 |

- 枚举值 `1` 保留给 Holly 的 `thermal/temp/alarm`，Cube **不实现、不发出**；有效范围由 `NET_PARKIA_ALARM_TYPE_FIRST/LAST`（2–4）界定。
- 灵敏度：`0` LOW / `1` MID / `2` HIGH（`net_enum_alarm_sensitivity_t`）。
- HTTP 路径与 body 中的 `type` 使用**整数枚举**，不用字符串。

---

## 3. 事件管线

```text
alarm_monitor 线程 (1 Hz)
  → check_intrusion() / check_high_temp() / sample_body_temp()
  → 每 60 次循环: check_body_temp()
  → publish_event() = main_process_submit_alarm_event()
        ├─ alarm_plan_record_path()   // 若该类型 trigger_record_enabled 且 SD 就绪，写 video_path
        ├─ 入队 (容量 16)
        └─ rtp_push_alarm_event()      // 实时推送给 SDK（带 video_path）

alarm_worker 线程
  → config_copy_alarm_linkage(type) 取该类型 trigger + 全局 FTP/Webhook
  → trigger_record_enabled && video_path 非空:
        入侵/高温 → main_task_alarm_record()        // ZLM 事件录像
        体温      → main_task_body_temp_compose()   // 照片合成 MP4
  → submit_linkage_job(): 独立线程执行 FTP → Webhook（并发上限 16）
```

要点：

- `video_path` 在**发布前**确定，RTP / Webhook 接收方立即拿到路径，录像本身稍后在 worker 执行。
- 入侵 / 高温：仅当 `publish_event()` 返回 0（入队成功）才刷新冷却时间戳；队列满或未初始化则丢弃且不进入冷却。
- 体温：无冷却；每 10 分钟一轮独立评估。
- FTP 与 Webhook 在同一个 job 内顺序执行，FTP 失败不阻止 Webhook；两者都不阻塞录像。
- Webhook 超时 5 s，HTTP 2xx 视为成功；FTP 上传超时 15 s，测试登录超时 5 s。

---

## 4. 检测算法

### 4.0 运动量（入侵使用）

`intrusion_motion_sample()` 在报警线程里每秒调用一次：`thermal_copy_latest_raw()` 拷出最新一帧 raw AD，和**上一秒保存的那一帧**做差分。thermal 不再计算运动量。

| 参数 | 值 | 含义 |
| --- | --- | --- |
| `INTRUSION_SAMPLE_STRIDE` | `8` | 每 8 个像素取 1 个采样点 |
| `INTRUSION_DIFF_THRESHOLD` | `24` | 同位置两帧 raw AD 差 ≥ 24 计为“变化” |

```text
motion_level = round(changed / sampled × 100)      // 0–100
```

约束：

- 第一秒只有基准帧，或 `frame_no` 没变、宽高/像素数变了：只保存本帧，本秒不判定。
- 挡片打开的帧不会写入 thermal 的最新 raw，拷到的是最后一帧有效画面；挡片期间 `frame_no` 不变，因此不判定。
- 比的是相隔约 1 秒的两帧，不是相邻帧。手在这一秒内进入画面时，整只手相对上一秒都算变化。
- 本质是热像变化占比，不是光流或目标检测。

### 4.1 入侵 `NET_ALARM_INTRUSION` (2)

配置 `/etc/cfg/alarm/intrusion.json`，HTTP `GET/PUT thermal/intrusion/alarm`：

```json
{
  "enabled": false,
  "no_repeat_minutes": 5,
  "sensitivity": 1,
  "record_duration_sec": 30
}
```

`check_intrusion()` 每秒执行：

1. `enabled == false` → 丢掉已保存的上一秒帧并返回
2. `intrusion_motion_sample()`：无帧、或本秒只是基准帧 → 返回；否则得到与上一秒的 `motion`
3. 按 `sensitivity` 选阈值：

| 灵敏度 | 值 | 阈值 |
| --- | --- | --- |
| LOW | 0 | 70 |
| MID | 1 | 50（默认） |
| HIGH | 2 | 30 |

4. `motion < threshold` → 不报
5. `no_repeat_minutes`：`≤0` 无冷却；否则距上次**成功发布**不足 N 分钟则不报
6. 组装事件（`type=2`、`motion_level`、`record_duration_sec`、`"intrusion alarm"`）→ 发布
7. 仅发布成功才刷新 `s_last_intrusion`

灵敏度调的是**面积占比阈值**，不是 ΔAD。

### 4.2 高温 `NET_ALARM_HIGH_TEMP` (3)

配置 `/etc/cfg/alarm/high_temp.json`，HTTP `GET/PUT thermal/high_temp/alarm`：

```json
{
  "enabled": false,
  "no_repeat_minutes": 3,
  "alarm_temp": 38.0,
  "record_duration_sec": 30
}
```

`check_high_temp()` 每秒执行：

1. `thermal_get_global_max_temp(&temp)` — 整幅最高温，无 marker
2. `temp >= alarm_temp` 且冷却就绪 → 发布事件（`type=3`、`temperature`、`threshold = alarm_temp`、`"high temperature alarm"`）
3. 仅发布成功才刷新 `s_last_high_temp`

### 4.3 体温监护 `NET_ALARM_BODY_TEMP` (4)

配置 `/etc/cfg/alarm/body_temp.json`，HTTP `GET/PUT thermal/body_temp/alarm`：

```json
{
  "enabled": false,
  "sensitivity": 1
}
```

公开字段仅这两项；**无** marker / no_repeat / record_duration_sec / slope 阈值。

不看运动，看**全画面最高温随时间的变化趋势**。三级节奏：

#### 秒级：`sample_body_temp()`

- 每秒读一次 `thermal_get_global_max_temp()`，写入当前分钟缓冲（容量 60）。
- `enabled` 从真变假时清空分钟缓冲、历史窗口和照片相册。

#### 分钟级：`check_body_temp()`（每 60 次循环）

1. 分钟缓冲有效读数 `< 20` → 本分钟跳过（清空缓冲，记 WARN）
2. 取本分钟读数的**中位数**作为该分钟样本
3. `body_temp_take_photo()` 拍一张 BGR 预览照片进相册（`photo_album`，容量 10）
4. 样本写入历史窗口（容量 10）
5. `main_process_submit_body_temp_sample()` → 若该类型 Webhook 开启，异步 POST 一条**采样消息**（见 §7.2）
6. 历史未满 10 个 → 返回，等下一分钟

#### 轮级：历史满 10 个样本（约 10 分钟）

1. `photo_album_finish_round()` 封存本轮 10 张照片
2. `body_temp_history_trend()` 计算 `baseline` 与 `slope`：
   - **Theil–Sen**：斜率取所有样本对 `(T_j − T_i) / Δt_min` 的**中位数**（45 对），约 29% 离群点不影响结果
   - `baseline` = 前 3 个样本温度的中位数
   - 单位 °C/min；时间戳非递增的对被跳过
3. 阈值按 `sensitivity`：

| 灵敏度 | 值 | 阈值 (°C/min) |
| --- | --- | --- |
| LOW | 0 | 0.10 |
| MID | 1 | 0.05（默认） |
| HIGH | 2 | 0.02 |

4. `|slope| ≥ threshold` → 发布事件（`type=4`、`temperature`、`baseline`、`slope`、`"body temperature alarm"`）；**升温与降温都触发**
5. 无论是否报警，清空历史窗口，下一分钟开始新一轮（**离散轮次，不滑动**）

体温**无** `no_repeat` 冷却，靠 10 分钟轮次天然限频。

#### 三类对照

| | 入侵 | 高温 | 体温 |
| --- | --- | --- | --- |
| 特征 | 相隔 1 秒两帧的 AD 变化占比 | 整幅最高温 | 最高温分钟中位数的 Theil–Sen 斜率 |
| 判定周期 | 1 s | 1 s | 10 min / 轮 |
| 阈值 | 面积占比 % (70/50/30) | `alarm_temp` °C | 0.10/0.05/0.02 °C/min |
| 冷却 | `no_repeat_minutes` | `no_repeat_minutes` | 无 |
| 录像 | ZLM 事件录像 | ZLM 事件录像 | 10 张照片合成 MP4 |

---

## 5. 配置与校验

| 接口 | 说明 |
| --- | --- |
| `thermal/{intrusion\|high_temp\|body_temp}/alarm` | GET/PUT；PUT 合并写 |
| `alarm/triggers/{type}` | `type` 为整数 2–4；body 的 `type` 必须与路径一致 |
| `system/ftp`、`system/ftp/test` | 全局 FTP；PUT/测试时空 `pwd` 表示沿用已存密码；GET/测试响应清空 `pwd` |
| `system/webhook` | 全局 Webhook；允许空 URL 以关闭 |

联动配置示例：

```json
{
  "type": 2,
  "trigger_ftp_enabled": true,
  "trigger_webhook_enabled": false,
  "trigger_record_enabled": true
}
```

默认值：三类报警 `enabled=false`；联动 `ftp=false`、`webhook=false`、`record=true`。

落盘：

| 配置 | 路径 |
| --- | --- |
| 三类报警 | `/etc/cfg/alarm/{intrusion,high_temp,body_temp}.json` |
| 三类联动 | `/etc/cfg/alarm/trigger_{intrusion,high_temp,body_temp}.json` |
| FTP / Webhook | `/etc/cfg/device/ftp/config.json`、`/etc/cfg/device/webhook/config.json` |

校验：

- `record_duration_sec` 允许 **30 / 60**（DTO 校验）；加载时非 30/60 一律回落到 30。
- `no_repeat_minutes ≥ 0`；`alarm_temp` 必须是有限数；`sensitivity` 落在 0–2。
- 缺少 `trigger_record_enabled` 时默认 `true`。
- 旧布局迁移：扁平 `trigger.json` 扇出到三个 `trigger_*.json`；`record_config.json` 的时长写入入侵/高温配置后删除。

---

## 6. RTP 报警推送

握手：客户端连接 RTP TCP 端口（默认 8001），发送 `GET /session?role=alarm\r\n\r\n`；服务端回 `HTTP/1.1 200 OK` + `Content-Length: 0`，之后直接发二进制帧。

帧布局（除 TCP 外层长度为大端外，整数与 `float32` 均为小端）：

```text
[8-byte TCP][8-byte RTP][16-byte frame][alarm body]
TCP:   0x24 | 3 reserved | BE32 payload_size
RTP:   V=2, PT=96, marker=1, BE16 seq, BE32 timestamp_ms
Frame: LE16 msg_type=2 | LE16 flags=0 | LE32 body_len | LE64 timestamp_us
Body:  LE32 alarm_type | LE32 info_len | LE64 timestamp_us | type_info
```

`type_info` = 类型字段 + `LE32 message_len` + message + `LE32 video_path_len` + video_path：

| 类型 | 类型字段 |
| --- | --- |
| intrusion (2) | `LE32 motion_level` |
| high_temp (3) | `float32 temperature` + `float32 threshold` |
| body_temp (4) | `float32 temperature` + `float32 baseline` + `float32 slope` |

- `message`、`video_path` 为 UTF-8，不含结尾 `\0`；无录像联动时 `video_path` 长度为 0。
- body 时间戳为 0 时，NetSDK 侧可回退使用 frame header 时间戳。
- 网页不消费裸 TCP RTP；由 NetSDK 解析。

---

## 7. 联动：FTP / Webhook / 录像

FTP / Webhook 目标为全局配置，**开关按报警类型**独立。

### 7.1 FTP 快照

`thermal_provider_capture_jpeg()`（最新预览帧 → JPEG 内存）→ `ftp_upload_buffer()`。

- 无有效热像帧：本次 FTP 立即失败，不影响 Webhook / 录像。
- 路径模板默认 `Parkia/{yyyyMMddHHmmss}.jpg`，另支持 `{yyyyMMddHHmmssfff}`；模板为空时落 `alarm.jpg`。时间取事件时间戳。
- 远端目录不存在时自动创建；`use_ssl` 为显式 FTPS（连接后 `AUTH TLS`）。

### 7.2 Webhook

**报警事件** POST JSON（扁平 `alarm_info`，各类型字段始终带上，未用字段为 0/空）：

```json
{
  "type": 2,
  "alarm_info": {
    "timestamp_us": 0,
    "motion_level": 0,
    "temperature": 0.0,
    "threshold": 0.0,
    "baseline": 0.0,
    "slope": 0.0,
    "message": "intrusion alarm",
    "video_path": "/userdata/parkia/record/intrusion_20260928_135700.mp4"
  }
}
```

体温报警事件额外带 `"is_alarm": true`。

**体温采样消息**（仅体温 Webhook 开启时，每分钟一条，不经报警队列）：

```json
{
  "type": 4,
  "alarm_info": {
    "timestamp_us": 0,
    "temperature": 36.5,
    "is_alarm": false
  }
}
```

接收方以 `is_alarm` 区分 `type=4` 的采样与报警。

### 7.3 录像

- 仅当该类型 `trigger_record_enabled == true` 且 `/userdata/parkia/record` 就绪（`net_record_dir_ready()`，由 SD 卡挂载脚本写 `.record_ready` 标志）时规划 `video_path`；否则 `video_path` 为空，不录像。
- 路径：`/userdata/parkia/record/{intrusion|high_temp|body_temp}_{yyyyMMdd}_{HHmmss}.mp4`。
- **入侵 / 高温**：`video_process_record_alarm()` → ZLM MP4 任务。总时长 = `record_duration_sec`，其中前 **10 s** 为预录（GOP 缓存），其余为触发后实时画面；录满自动结束。并发报警各自独立成文件。
- **体温**：`video_process_compose_stills_mp4()` 把本轮封存的 10 张照片按时间序合成 MP4（每张 1 s，全关键帧）；相册为空则失败。

---

## 8. 设计决策摘要

1. 上游契约以 Volga.IRtekNetSDK customer API 为准；Cube 文档写实现范围与 RTP 字节细节。
2. 公开配置去掉 marker；高温用全画面最高温；体温仅暴露 `sensitivity`，内部固定 10 分钟轮次 + 分钟中位数 + Theil–Sen。
3. 体温对 `|slope|` 判定，升降温都报；不设冷却，靠轮次限频。
4. 联动目标全局、开关分类型；录像由 `trigger_record_enabled` 控制，路径在发布前确定并随事件下发。
5. FTP 走 thermal provider 直出 JPEG，与推流线程解耦。
6. `alarm` 只做判定与事件发布，不做推流。
