# net_app 架构说明（Parkia.Cube / RK3506）

模块边界对齐 Holly.Lantern 的 `net_app`。平台相关部分（Wi-Fi、BLE 配网、Buildroot、App 镜像升级）为 Rockchip 专用实现。

产品级总览见：[docs/architecture.md](../../docs/architecture.md)。  
HTTP API 契约见下文第 7 节；[API.md](API.md) 仍可作为独立副本使用。

---

## 1. 目录布局

```text
net_app/
├── main.c                 # 初始化 / 反初始化 + poll 循环
├── CMakeLists.txt         # 源文件、编译选项、SDK 路径 (-DNET_APP_SDK_DIR)
├── net_common/            # g_config、g_system_status、枚举
├── main_process/          # NTP、延迟重启、任务桩
├── http_api/
│   ├── http_api.c/h       # mongoose 监听 + 路由分发
│   ├── http_api_common.*  # {code,message,data} 响应包络
│   ├── http_dto.*         # 线上 DTO 辅助
│   ├── http_auth*.*       # SoftAP 开放路由；已配网后 Basic 鉴权
│   ├── http_api_system.c  # 设备/时间/NTP/网络/端口/升级 + recover_dhcp
│   ├── http_api_alarm.c   # 入侵/高温/体温监护/联动触发
│   ├── http_api_thermal.c # 热像挡片/量程/参数/单位/镜头/校正 + 图像翻转/参数
│   ├── http_api_streaming.c
│   ├── http_api_portal.c  # SoftAP 静态门户辅助
│   └── config/            # /etc/cfg JSON 持久化
├── video_process/         # 对外流媒体 API（采集 + ZLM 门面）
│   ├── video_process.*    # UVC→I420 + 启停 ZLM；HTTP/WebRTC 钩子
│   ├── image_convert.*
│   └── zlm/               # 内部：软编 + ZLM RTSP/WebRTC
│       ├── zlm_server.*
│       ├── zlm_encode.*
│       └── h264_encoder.*
├── thermal/               # IRtek UVC 源、测温与 JPEG 抓拍
├── alarm/                 # 报警判定与历史
├── rtp/                   # TCP 报警帧服务
├── ble/                   # BLE GATT Wi-Fi 配网（链入 net_app）
├── led/                   # LED 状态机
└── libs/
    ├── mongoose/, json/, file/, log/, platform/, upgrade/
    ├── minih264/, irtek/
    └── zlm/               # 入库头文件 + libmk_api.so
```

兄弟进程：

| 应用 | 职责 |
| --- | --- |
| `prebuilts/apps/fab_app/` | 产测 HTTP `:8088`（device_info/upgrade/hsi/seal + sensor/*） |
| `prebuilts/apps/discovery_app/` | 局域网 UDP 发现 `239.255.255.250:9712`；调用 `PUT discovery/recover_dhcp` |
| `app/www/` | SPA → `/var/www`，由 net_app 托管 |

---

## 2. 硬性规则

1. **`g_config` 是运行时唯一真相（SSOT）。** 业务模块（`wifi`、`ble`、`alarm`、`video_process`、`main_process`、`rtp`）只读 `g_config` / `g_system_status`，**禁止**调用 `config_get_*`。
2. **配置持久化边界。** 仅 `http_api_*.c` 与 `config/config_manager.c` 可调用 `config_get_*` / `config_set_*`。
3. **三种形态：** `*_cfg`（落盘 JSON）、`*_dto`（HTTP 线上）、`net_common.h` 中的运行时结构（保存后同步）。
4. **Handler 命名：** `handle_<域>_<资源>_<方法>()`。
5. **HTTP 包络：** `{ "code", "message", "data" }`；HTTP 状态码通常为 200。
6. **`net_enums.h` 是线上契约** — 只可追加，禁止改号。

---

## 3. 启动与 poll

```text
log_init → config_init → led init
→ http_api_start → main_process_init
→ thermal_provider_start → video_process_start → zlm_server_start
→ rtp_server_start → alarm_monitor_start
→ loop: http_api_poll + rtp_server_poll + main_process_poll
→ 退出时逆序反初始化
```

默认端口（另见产品级 `docs/architecture.md`）：HTTP `80`，RTSP `554`，RTP `8001`，ZLM HTTP `8080`，ZLM RTC `8555`。

**视频管线：**

1. **`video_process`** — 始终处理原始帧：UVC → BGR → I420（共享最新一帧），不做编码。
2. **`video_process/zlm/`（`zlm_encode` + `zlm_server`）** — 有观看者时软编 I420 → H.264 并推入 ZLM；空闲则取消发布。
3. **ZLM worker** — 向客户端 remux / 提供 RTSP/WebRTC。

```text
UVC 回调 → raw 队列（仅保留最新）
       ↓
video_process（始终运行）→ 共享 I420
       ↓
zlm_encode（按需）→ H.264 → ZLM → RTSP/WebRTC
```

### 外设接入：热插拔与就绪门禁（硬性）

USB / 串口 / 模组等外设在内核枚举成功后，**厂商控制面往往尚未就绪**：`login`、列描述符可能成功，但立刻开流或发厂商命令会在第三方 SDK / 驱动内直接崩溃（无业务错误码可回收）。

后续在 `net_app` 接入**任意**外设（热像 UVC、可见光、传感器、串口模组等）必须遵守下列规范。参考实现：`thermal/thermal_uvc_bridge.cpp`。

| # | 规则 | 说明 |
| --- | --- | --- |
| 1 | **先探测就绪，再开流/重操作** | Hotplug arrive 或 enumerate 成功后，不得立刻 `start_stream` / 连续厂商写。先做至少一项能证明控制面可用的探测（如 `get_lens_info` / `get_*_params` / 设备特有 GET）。 |
| 2 | **有限重试 + 退避，失败则 defer** | 探测失败时短睡重试（当前热像：最多 8 次 × 250ms）。仍失败则关闭半开会话并返回失败，由上层周期重试（如 `video_process` 在拿不到 native size 时每 1s 调用 `thermal_provider_start`）。**禁止**在半就绪句柄上硬开流。 |
| 3 | **描述符可读 ≠ 可开流** | 帧列表 / 接口描述符来自 USB 描述符缓存，控制 GET 全失败时仍可能列到 format；不得以此作为开流条件。 |
| 4 | **拔出先停会话，插入再走完整 open** | remove → `close_session`（停流 + logout）；arrive → 完整 `login → 就绪门禁 → 应用本地 SSOT 配置 → 开流`。禁止复用拔出前的 HANDLE。 |
| 5 | **生命周期串行化** | open/close/hotplug 与 provider start/stop 共用同一把生命周期锁（或等价串行），避免 capture 线程与 hotplug 线程并发 login/logout。 |
| 6 | **路径/ID 按二进制处理** | IRtek 等 SDK 的 `path` 常为 bus/port 二进制 id（非 C 字符串）。日志用 hex（如 `01:01:02`），比较用 `memcmp` / 定长，禁止 `%s`。 |
| 7 | **SDK 停流要 cancel 再 free** | 自研或维护的 USB 采集库：停流须先 `libusb_cancel_transfer`（或等价），等完成后再 `free_transfer`；帧回调拷贝前检查缓冲容量；bulk 在 `MaxPayload==0` 时拒绝启动。预编译 `.so` 改源码后必须重新编译并随固件部署。 |

```text
enumerate / hotplug arrive
  → login
  → ready probe（GET 控制，带重试）
       ├─ 失败 → logout，return；上层稍后再试
       └─ 成功 → 应用 g_config SSOT → start_stream
hotplug remove / stop
  → stop_stream → logout → 清空运行态
```

日志约定（便于区分「未就绪 defer」与「SDK 内崩」）：

- 就绪重试：`device controls not ready (attempt i/n)`
- 放弃开流：`deferring stream start`
- 即将开流：`starting stream …`
- 开流成功：`streaming started …`

若出现「已打印帧/格式列表，却没有 `starting stream` / `streaming started`」且进程消失，优先按本节门禁缺失或 SDK 停流/缓冲问题排查。

---

## 4. HTTP 路由（`http_api_routes.h`）

| 路径 | Handler 文件 |
| --- | --- |
| `system/device_info`、`device_restart`、`device_reset`、`device_upgrade` | `http_api_system.c` |
| `system/time`、`ntp_server`、`networks`、`status`、`port` | `http_api_system.c` |
| `discovery/recover_dhcp` | `http_api_system.c` |
| `thermal/*/alarm`、`alarm/triggers` | `http_api_alarm.c` |
| `thermal/lens_info`、`thermal/thermal_*`、`image/channels/{id}/*` | `http_api_thermal.c` |
| `streaming/channels`、`media/webrtc` | `http_api_streaming.c` / portal |

静态 SPA 根目录为 `NET_WEB_ROOT`（`/var/www`）。

---

## 5. 库（`libs/`）

| 库 | 职责 |
| --- | --- |
| `mongoose` | 嵌入式 HTTP |
| `json`（cJSON + json_bind） | 配置 / DTO 绑定 |
| `binary` | 对齐 Holly 的小端 / 大端二进制写出 |
| `ftp` | FTP / 显式 FTPS 登录与内存上传 |
| `file` / `platform` / `log` | 文件系统、`system`/`popen`、日志 |
| `upgrade` | App 槽位 OTA 合并（`main_app`、`net_app`、`fab_app`、`discovery_app`、`libIRtekUVCSDK.so`、`www`） |
| `minih264` | 软件 H.264 |
| `irtek` | UVC 热像 SDK（`.so` + 头文件） |
| `zlm` | 入库的 Firefly.MediaServer API 头文件 + `libmk_api.so` |

SDK 路径：由 Buildroot（`net_app.mk`）传入 `-DNET_APP_SDK_DIR=<sdk>`；禁止在 CMake 中写死绝对路径或相对检出路径。

---

## 6. 编译选项

| 选项 | 默认（交叉编译） | 作用 |
| --- | --- | --- |
| `NET_APP_ENABLE_THERMAL_UVC` | ON | 真实 IRtek 桥接；关闭则用 `thermal_uvc_stub.c` |

ZLM（`video_process/zlm/*` + `libs/zlm`）始终编译链接，无 OFF 开关。  
`app/_legacy/zlm` 保留上游源码但默认不参与构建；  
`./app/_legacy/build_zlm.sh` 或 `./build.sh --rebuild-third-party` 会刷新离线库。

生产构建仅通过 `Parkia.Cube/./build.sh`。新增 `.c`/`.cpp` 必须列入 `CMakeLists.txt` 的 `NET_APP_SOURCES`。

---

## 7. HTTP API 契约

本文档以 `Volga.IRtekNetSDK/doc/NetSDK/customer/docs/api.md` 为唯一上游基线，记录 Parkia.Cube `net_app` 实际支持的接口能力。未支持的能力已明确标注。

### 通用约定

- **响应包络**：所有 HTTP 接口响应为 JSON 包络 `{ "code": 0, "message": "OK", "data": {} }`。`code == 0` 为成功。
- **鉴权**：已配网后，非开放路由需 HTTP Basic 认证。当前工厂管理员凭据为 `admin/admin`；未配网/SoftAP 模式下管理路由放行。
- **枚举**：所有枚举字段及枚举路径参数均使用 JSON number / 十进制数字，不使用字符串名称。

### 7.1 配网管理

Parkia.Cube **不支持** HTTP 配网接口，配网仅通过 BLE GATT 服务完成。

| HTTP 接口 | 方法 | 接口名称 | 支持状态 |
| --- | --- | --- | --- |
| `system/provision_info` | `GET` | 获取配网信息 | ❌ 未支持 |
| `system/provision_auth` | `POST` | 配网鉴权 | ❌ 未支持 |
| `system/wifi_aps` | `GET` | 获取 Wi-Fi 热点列表 | ❌ 未支持 |
| `system/wifi_provision` | `POST` | 提交 Wi-Fi 配网 | ❌ 未支持 |
| `system/provision_hotspot` | `GET/PUT` | 获取配网热点 / 设置配网热点 | ❌ 未支持 |
| `system/provision_status` | `GET` | 获取配网状态 | ❌ 未支持 |
| `system/provision_cancel` | `PUT` | 取消配网 | ❌ 未支持 |
| `system/provision_reset` | `PUT` | 重置配网 | ❌ 未支持 |

*注：运行时网络状态可通过开放接口 `GET system/status` 获取。*

### 7.2 设备管理

| HTTP 接口 | 方法 | 接口名称 | 支持状态 | 说明 |
| --- | --- | --- | --- | --- |
| `system/device_info` | `GET/PUT` | 获取设备信息 / 设置设备信息 | ✅ 支持 | PUT 仅可修改 `name` |
| `system/device_restart` | `PUT` | 设备重启 | ✅ 支持 | |
| `system/device_reset` | `PUT` | 设备重置 | ✅ 支持 | 不自动重启 |
| `system/device_upgrade` | `POST` | 设备升级 | ✅ 支持 | multipart/form-data 上传 |
| `system/time` | `GET/PUT` | 获取设备时间 / 设置设备时间 | ✅ 支持 | |
| `system/ntp_server` | `GET/PUT` | 获取 NTP 服务 / 设置 NTP 服务 | ✅ 支持 | |
| `system/networks` | `GET/PUT` | 获取网络参数列表 / 批量设置 | ✅ 支持 | PUT 为 Parkia 扩展 |
| `system/networks/{id}` | `GET/PUT` | 获取网络参数 / 设置网络参数 | ✅ 支持 | 仅 STA 模式允许 PUT |
| `system/port` | `GET/PUT` | 获取设备服务端口 / 设置设备服务端口 | ✅ 支持 | |
| `system/ftp` | `GET/PUT` | 获取 FTP 配置 / 设置 FTP 配置 | ✅ 支持 | 报警快照为 JPEG；GET 不返回密码，PUT 空密码表示保留 |
| `system/ftp/test` | `POST` | FTP 测试 | ✅ 支持 | 使用请求体配置测试，不落盘 |
| `system/webhook` | `GET/PUT` | 获取 Webhook 地址 / 设置 Webhook 地址 | ✅ 支持 | 支持 HTTP/HTTPS |

*注：设备发现 UDP 进程提供 `PUT discovery/recover_dhcp`（本机回环调用）。产测进程 `fab_app`（:8088）提供产测专属的 `device_info`、`hsi`、`seal`、`sensor/sn` 等接口。*

### 7.3 报警联动

| HTTP 接口 | 方法 | 接口名称 | 支持状态 | 说明 |
| --- | --- | --- | --- | --- |
| `alarm/triggers/{type}` | `GET/PUT` | 获取报警联动 / 设置报警联动 | ✅ 支持 | 每种报警独立配置 FTP/Webhook/录像触发 |
| `alarm/schedule/{type}` | `GET/PUT` | 获取布防时间段 / 设置布防时间段 | ❌ 未支持 | |

`type` 为报警枚举值：`2` 入侵、`3` 高温、`4` 体温监护。路径参数和响应/请求体中的 `type` 均使用该数字枚举，不接受字符串名称。  
FTP 与 Webhook 的目标参数为设备全局配置；这里只控制对应报警类型是否触发各项联动。

### 7.4 用户管理

| HTTP 接口 | 方法 | 接口名称 | 支持状态 |
| --- | --- | --- | --- |
| `security/users` | `GET/PUT` | 获取用户列表 / 添加或修改用户 | ❌ 未支持 |
| `security/users/{id}` | `GET/DELETE` | 获取用户信息 / 删除用户 | ❌ 未支持 |

### 7.5 视频流

| HTTP 接口 | 方法 | 接口名称 | 支持状态 |
| --- | --- | --- | --- |
| `streaming/channels` | `GET` | 获取视频流列表 | ✅ 支持 |
| `streaming/channels/{stream_id}` | `GET` | 获取视频流参数 | ✅ 支持 |
| `streaming/channels/{stream_id}/status` | `GET` | 获取视频流状态 | ✅ 支持 |

### 7.6 显示设置

| HTTP 接口 | 方法 | 接口名称 | 支持状态 |
| --- | --- | --- | --- |
| `video/channels` | `GET` | 获取通道列表 | ❌ 未支持 |
| `video/channels/{channel_id}` | `GET/PUT` | 获取通道信息 / 设置通道信息 | ❌ 未支持 |
| `video/channels/{channel_id}/overlay` | `GET/PUT` | 获取字符叠加 / 设置字符叠加 | ❌ 未支持 |
| `image/channels/{channel_id}/image_params` | `GET/PUT` | 获取图像参数 / 设置图像参数 | ✅ 支持 |
| `image/channels/{channel_id}/image_flip` | `GET/PUT` | 获取图像翻转模式 / 设置图像翻转模式 | ✅ 支持 |

*注：图像通道固定为 `101`。翻转/镜像由 net_app 在渲染出口软件实现（UVC 硬件无此能力）；`image_flip` 额外携带 Parkia 扩展字段 `rotation`（顺时针角度 0/90/180/270，flip 之后再旋转），流媒体与抓拍同一生效点。*

### 7.7 热像设置

| HTTP 接口 | 方法 | 接口名称 | 支持状态 |
| --- | --- | --- | --- |
| `thermal/lens_info` | `GET` | 获取热像镜头信息 | ✅ 支持 |
| `thermal/thermal_calibrate` | `PUT` | 执行热像校正 | ✅ 支持 |
| `thermal/thermal_shutter_mode` | `GET/PUT` | 获取热像校正模式 / 设置热像校正模式 | ✅ 支持 |
| `thermal/thermal_shutter_time` | `GET/PUT` | 获取热像自动校正时间 / 设置挡片自动校正时间 | ✅ 支持 |
| `thermal/thermal_resolution` | `GET` | 获取热像分辨率 | ❌ 未支持 |
| `thermal/thermal_range` | `GET/PUT` | 获取热像测温量程 / 设置热像测温量程 | ✅ 支持 |
| `thermal/thermal_lut` | `GET` | 获取热像 LUT 表 | ❌ 未支持 |
| `thermal/thermal_params` | `GET/PUT` | 获取热像测温参数 / 设置热像测温参数 | ✅ 支持 |
| `thermal/color_dist` | `GET/PUT` | 获取热像色彩分布类型 / 设置热像色彩分布类型 | ✅ 支持 |
| `thermal/gain` | `GET/PUT` | 获取温宽模式 / 设置温宽模式 | ✅ 支持 |
| `thermal/preset_palettes` | `GET` | 获取热像调色板列表 | ✅ 支持 |
| `thermal/palette` | `GET/PUT` | 获取热像调色板 / 设置热像调色板 | ✅ 支持 |
| `thermal/thermal_overlay` | `GET/PUT` | 获取热像叠加 / 设置热像叠加 | ❌ 未支持 |
| `thermal/thermal_unit` | `GET/PUT` | 获取热像单位 / 设置热像单位 | ✅ 支持 |
| `thermal/capture/{mode}` | `GET` | 获取热像抓拍文件 | ❌ 未支持 |
| `thermal/marker` | `GET/PUT` | 获取热像测温标识列表 / 设置热像测温标识列表 | ❌ 未支持 |
| `thermal/temp/alarm` | `GET/PUT` | 获取热像测温报警列表 / 设置热像测温报警列表 | ❌ 未支持 |
| `thermal/temp/alarm/interval` | `GET/PUT` | 获取热像测温报警时间间隔 / 设置热像测温报警时间间隔 | ❌ 未支持 |
| `thermal/dpc` | `PUT` | 设置热像坏点 | ❌ 未支持 |
| `thermal/data/{type}` | `GET` | 获取热像实时数据 | ❌ 未支持 |

*注：热像设置为 net_app 配置 SSOT 实现——写操作落盘 `/etc/cfg/thermal/config.json` 后立即返回成功。采集线程在下一帧把配置下发到设备和 FileSDK，设备上线时以配置为准同步。`thermal_params` 的 `refl_temp`/`atm_temp` 线上单位为开尔文（K）；UVC 模组不做硬件测温，参数由 net_app 软件换算路径消费。`lens_info` 与 `thermal_calibrate` 需要设备在线，离线返回失败。`color_dist`/`palette`/`gain` 由 IRtek FileSDK 作用到 UVC 预览；预置调色板列表和色带都从 FileSDK 读取。温宽 `min`/`max`/`span` 线上单位跟随 `thermal_unit`，与 Holly.Lantern 一致，设备内按开尔文保存。*

### 7.8 Parkia 智能报警扩展

Parkia.Cube 不使用 Holly 的 `thermal/temp/alarm` 端点，而是提供 3 类独立智能报警接口。  
入侵与高温报警各配置 `record_duration_sec`，**无 `marker` 字段**；体温监护的录像为每分钟照片合成。

| HTTP 接口 | 方法 | 接口名称 | 支持状态 |
| --- | --- | --- | --- |
| `thermal/intrusion/alarm` | `GET/PUT` | 获取/设置入侵报警 | ✅ 支持 |
| `thermal/high_temp/alarm` | `GET/PUT` | 获取/设置高温报警 | ✅ 支持 |
| `thermal/body_temp/alarm` | `GET/PUT` | 获取/设置体温监护 | ✅ 支持 |

### 7.9 RTP 报警推送服务

SDK 通过连接设备 RTP TCP 端口（默认 8001）接收实时报警帧。

| 协议流程 | 说明 |
| --- | --- |
| `GET /session?role=alarm\r\n\r\n` | 建立报警监听会话 |

报警类型枚举（`NET_PARKIA_ALARM_TYPE_FIRST/LAST` = 2–4；值 `1` 保留给 Holly `thermal/temp/alarm`，Cube 不实现、不发出）：

- `2`: `NET_ALARM_INTRUSION` 入侵
- `3`: `NET_ALARM_HIGH_TEMP` 高温
- `4`: `NET_ALARM_BODY_TEMP` 体温监护

#### 7.9.1 帧格式

所有整数和 `float32` 除 TCP 外层长度外均为小端：

```text
[8 字节 TCP 头][8 字节 RTP 头][16 字节帧头][报警 body]
TCP:   0x24, 3 字节保留, BE32 payload_size
Frame: LE16 msg_type=2, LE16 保留, LE32 body_len, LE64 timestamp_us
Body:  LE32 alarm_type, LE32 info_len, LE64 timestamp_us, type_info
```

`type_info`：

- 入侵：`LE32 motion_level, LE32 message_len, message, LE32 video_path_len, video_path`
- 高温：`float32 temperature, float32 threshold, LE32 message_len, message, LE32 video_path_len, video_path`
- 体温：`float32 temperature, float32 baseline, float32 slope, LE32 message_len, message, LE32 video_path_len, video_path`

`message`、`video_path` 为 UTF-8 编码，线上不含结尾 `\0`；无录像联动时 `video_path` 为空。

服务端握手仅返回 `HTTP 200` 头和 `Content-Length: 0`，随后直接发送二进制帧。

#### 7.9.2 联动行为

- 每次报警按对应类型的 trigger 配置决定是否上传 FTP 快照、发送 Webhook 及录制 MP4。
- FTP 快照由 thermal provider 将最新热像预览帧编码为 JPEG（libjpeg），交给 `libs/ftp/ftp_client` 的 `ftp_upload_buffer()` 上传；不依赖视频推流线程，也不写共享临时文件。
- 无有效热像帧时本次 FTP 联动立即失败，但不阻塞其他报警联动。
- 录像路径在事件发布时确定（`/userdata/parkia/record/{类型名}_{yyyyMMdd}_{HHmmss}.mp4`），随事件的 `video_path` 字段下发到 RTP 与 Webhook。
- `/userdata/parkia/record` 必须由 SD 卡（`/dev/mmcblk0p1`）挂载提供：`/etc/init.d/S15_SD` 负责挂载、热插拔轮询与容量检查，并在卡上写入就绪标志 `.record_ready`；net_app 仅通过 `net_record_dir_ready()` 判断该标志。无卡、挂载失败、只读或剩余空间不足 16MB 时不下发 `video_path`，即不录像、不向该路径写文件。
- 入侵/高温报警经 `mk_recorder_start_task` 录制事件 MP4：总时长为 `record_duration_sec` 秒（允许 30/60），其中前 10 秒为预录（ZLM GOP 帧缓存），其余为触发后的实时画面，录满自动结束；并发报警各自生成文件。
- 体温监护每分钟采样整帧最高温并留存照片（`alarm/photo_album`，每轮 10 张）；报警时由 `video_process/stills_mp4.c` 把本轮照片按时间序合成为 MP4（每张 1 秒，全关键帧）。
- Webhook JSON 为 `{ "type": number, "alarm_info": { ... } }`，`alarm_info` 为扁平结构（`timestamp_us`、`motion_level`、`temperature`、`threshold`、`baseline`、`slope`、`message`、`video_path`），未用字段为 0/空；体温报警事件额外带 `"is_alarm": true`。
- 体温监护 Webhook 开启时，每分钟样本另发一条 `{ "type": 4, "alarm_info": { "timestamp_us", "temperature", "is_alarm": false } }`，接收方以 `is_alarm` 区分采样与报警。

检测算法、冷却门控与线程模型见 `ALARM.md`。

### 7.10 配置落盘路径

| 配置 | 路径 |
| --- | --- |
| HSI / 设备信息 | `/etc/cfg/device/hsi/config.json` |
| 时间 | `/etc/cfg/device/time/config.json` |
| NTP | `/etc/cfg/device/ntp/config.json` |
| 网络 | `/etc/cfg/device/network/config.json` |
| 端口配置（net_app） | `/etc/cfg/device/port/config.json` |
| 端口配置（fab_app） | `/etc/cfg/fab/port/config.json` |
| 配网状态 | `/etc/cfg/provision/state/config.json` |
| Seal | `/etc/cfg/fab/status/config.json` |
| 入侵报警 | `/etc/cfg/alarm/intrusion.json` |
| 高温报警 | `/etc/cfg/alarm/high_temp.json` |
| 体温监护 | `/etc/cfg/alarm/body_temp.json` |
| 入侵联动 | `/etc/cfg/alarm/trigger_intrusion.json` |
| 高温联动 | `/etc/cfg/alarm/trigger_high_temp.json` |
| 体温联动 | `/etc/cfg/alarm/trigger_body_temp.json` |
| FTP | `/etc/cfg/device/ftp/config.json` |
| Webhook | `/etc/cfg/device/webhook/config.json` |

---

### 附录：支持的模型结构

#### 设备管理模型

##### `device_info`
```json
{
  "name": "IR-Cam-01"
}
```

##### `time_info`
```json
{
  "time_mode": 1,
  "local_time": "2026-05-20T10:09:00+08:00",
  "time_zone": "UTC+08:00"
}
```

##### `ntp_info`
```json
{
  "host_name": "pool.ntp.org",
  "port_no": 123,
  "syn_interval": 1440
}
```

##### `network_info`
```json
{
  "id": 1,
  "name": "eth0",
  "dhcp": true,
  "ip": "192.168.1.177",
  "netmask": "255.255.255.0",
  "dns": ["192.168.1.11", "192.168.1.12"],
  "gateway": ["192.168.1.1"]
}
```

##### `port_info`
```json
{
  "http_port_no": 80,
  "rtsp_port_no": 554,
  "rtp_port_no": 8001
}
```

#### 报警联动模型

##### `alarm_trigger`
```json
{
  "type": 2,
  "trigger_ftp_enabled": true,
  "trigger_webhook_enabled": false,
  "trigger_record_enabled": true
}
```
三个开关均按报警类型独立保存；录像时长由对应智能报警配置的 `record_duration_sec` 指定。

##### `ftp_info`
```json
{
  "use_ssl": false,
  "ip": "192.168.1.10",
  "port_no": 21,
  "user_name": "alarm",
  "pwd": "",
  "enable_annoy": false,
  "path_template": "Parkia/{yyyyMMddHHmmss}.jpg"
}
```

GET 与 FTP 测试响应中的 `pwd` 固定为空；PUT/测试请求的空 `pwd` 表示沿用已保存密码。`path_template` 支持 `{yyyyMMddHHmmss}` / `{yyyyMMddHHmmssfff}` 占位符，时间取报警事件时间戳。

##### `webhook_info`
```json
{
  "url": "https://example.com/alarm"
}
```

#### Parkia 智能报警模型

##### `intrusion_alarm`
```json
{
  "enabled": true,
  "no_repeat_minutes": 5,
  "sensitivity": 1,
  "record_duration_sec": 30
}
```
*(注：`sensitivity` 0/1/2 对应运动量阈值 70/50/30；`record_duration_sec` 允许 30/60)*

##### `high_temp_alarm`
```json
{
  "enabled": true,
  "no_repeat_minutes": 3,
  "alarm_temp": 38.0,
  "record_duration_sec": 30
}
```
*(注：无 marker 字段；`record_duration_sec` 允许 30/60)*

##### `body_temp_alarm`
```json
{
  "enabled": true,
  "sensitivity": 1
}
```
*(注：无 marker 字段)*

高温报警比较整幅热像最高温度。体温监护以 10 分钟为一个监测周期：每秒读取整幅热像最高温度，每分钟取这 60 次读数的中位数作为一个样本并拍照（有效读数少于 20 个时跳过该分钟），周期满 10 个样本后按真实时间戳做 Theil-Sen 稳健回归（基线取前 3 个样本的中位数），随后开启新一轮（周期间不重叠）；`sensitivity` 取 `0`（低）、`1`（中）、`2`（高），分别对应斜率阈值 `0.10`、`0.05`、`0.02 °C/min`，斜率绝对值达到阈值即触发，升温与降温均可触发。报警时将最近照片（最多 10 张）按时间顺序合成为 MP4。

#### 视频流模型

##### `stream_info`
```json
{
  "id": 101,
  "bit_rate": 65780,
  "codec_type": "h264",
  "disable_skip": false,
  "enable_rc": false,
  "gop_size": 1,
  "h264_profile": 1,
  "initial_qp": 0,
  "max_frame_rate": 30,
  "maximum_qp": 10,
  "quality_control_type": "MDQ",
  "resolution_width": 240,
  "resolution_height": 320
}
```

##### `stream_status`
```json
{
  "id": 101,
  "connection_count": 5
}
```
