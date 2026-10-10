# Parkia.Cube 架构说明

## 工程定位

| 项 | 说明 |
| --- | --- |
| 工程名 | `Parkia.Cube` |
| 完整固件 | `parkia_cube_fw_V{1.0.0.0}.img` |
| App 镜像 | `parkia_cube_tiny_fw_V{1.0.0.0}.img` |
| 日常升级目标 | 仅 App 镜像 |

本仓库是 **Rockchip SDK 补丁仓**：按 SDK 相对路径存放改动，不完整拷贝 SDK。构建前由 `build.sh` 同步到 `SDK_DIR` 后再编译。

应用层（对齐内部参考项目的 `net_app` + `www`）：

| 目录 | 职责 |
| --- | --- |
| `app/main_app/` | 入口守护；读取 `app_bin`，负责 BLE 配网并拉起业务进程 |
| `app/net_app/` | 设备后端（HTTP API、配网、视频、报警） |
| `prebuilts/apps/fab_app/` | 产测进程（`:8088`：device_info/upgrade/hsi/seal + sensor/*） |
| `prebuilts/apps/discovery_app/` | 局域网 UDP 组播设备发现（`239.255.255.250:9712`） |
| `app/www/` | 静态网页；安装到 `/var/www`，由 `net_app` 托管 |

应用层模块边界对齐内部参考项目的 `net_app`；驱动、文件系统、Buildroot 仍按 RK3506。

## 应用分层

入口守护：`app/main_app`（二进制名 `main_app`）。

- Boot 时读取 `/etc/cfg/main_app/config.json` 的 `app_bin`
- 默认启动 `/usr/bin/net_app`
- Developer Kit 可把用户 ELF 部署到 `/usr/bin/<name>`，并把 `app_bin` 更新到该路径

默认业务进程：`app/net_app`（二进制名 `net_app`）。

| 模块 | 职责 |
| --- | --- |
| `main.c` | 只做初始化 / 反初始化编排与 boot 配网分支 |
| `net_common/` | `g_config`、`g_system_status` 运行态快照 |
| `http_api/` | mongoose 启停、鉴权、路由、`handle_*`、DTO |
| `http_api/config/` | `/etc/cfg` JSON 持久化（`config_get/set`） |
| `main_process/` | NTP 同步、延迟重启等后台任务 |
| `wifi/` | SoftAP / STA（`wifibt-init.sh`、wpa、hostapd、udhcpc） |
| `led/` | LED 状态映射（当前可 stub） |
| `video_process/` | UVC 热像 BGR 软编 H.264，经 ZLMediaKit 输出 RTSP/WebRTC |
| `libs/` | mongoose、cJSON、log、file、shell 封装 |

编码规则细节见 [app/net_app/ARCHITECTURE.md](../app/net_app/ARCHITECTURE.md)（含外设热插拔与就绪门禁）。

### 硬性规则

1. **只有** `http_api_*.c` 与 `config_manager.c` 可调用 `config_get_*` / `config_set_*`。
2. 其它模块读运行态必须用 `g_config` / `g_system_status`。
3. Handler 命名：`handle_<域>_<资源>_<方法>`。
4. HTTP 业务结果一律走 `{code,message,data}` 包络。

## 启动与运行逻辑

```text
main()
  → log_init
  → config_init          # 加载 /etc/cfg，填充 g_config
  → net_wifi_manager_init
  → handle_boot_state    # 见下
  → http_api_start(port) # mongoose 监听，默认 80
  → main_process_init
  → rtp_server_start     # 报警/原始数据 RTP，默认 8001
  → loop: http_api_poll + rtp_server_poll + main_process_poll + provisioning_poll
  → 退出时逆序 uninit
```

### 默认端口

| 服务 | 端口 | 说明 |
| --- | --- | --- |
| HTTP API | 80 | mongoose 业务 API（`provision.http_port_no` / `port_info.http_port_no`） |
| RTSP | 554 | ZLMediaKit 视频播放 |
| RTP | 8001 | 报警数据 / 原始数据长连接（**不是** HTTP API） |
| ZLM HTTP | 8080 | WebRTC 信令，与业务 HTTP API 分离 |
| ZLM RTC | 8555 | WebRTC 媒体 UDP |

### Boot 分支（`handle_boot_state`）

读 `/etc/cfg/provision/state/config.json` 中的 `provision_state`：

| 状态 | 行为 |
| --- | --- |
| 非 `provisioned` | SoftAP + LED 等待配网（状态 `ap_ready`） |
| `provisioned` 且有 SSID | `net_wifi_connect` 重连；LED online/offline |
| `provisioned` 无 SSID | 维持 SoftAP，设备可被直连访问 |

配网状态仅存本地，**无云端绑定流程**。仅 SoftAP + HTTP 配网，不使用蓝牙。

## 请求处理逻辑

```text
HTTP 请求
  → mongoose ev_handler
  → http_auth_gate
  → 若 POST system/device_upgrade：multipart 落盘 → handle_system_upgrade
  → 否则 dispatch → 对应 handle_*
  → create_response / create_error
  → HTTP 200 + JSON
```

副作用类接口还会触达：

- Wi-Fi / LED：配网、复位、boot
- `main_process`：NTP、`device_restart`、升级后重启

## 必备能力

- SoftAP + 内嵌 mongoose HTTP
- Wi-Fi STA 扫描 / 连接
- `/etc/cfg` 本地配网与系统配置持久化
- 配网复位（API / 物理键等入口）
- LED 状态映射
- `system/status` 接口；产测 API 由 sibling `fab_app` 提供（`sensor/sn` 替代原 `cpu_serial`）

## 补丁目录布局

```text
Parkia.Cube/
  app/net_app/
    libs/zlm/          # submodule: 内部 zlm 源码
  prebuilts/apps/fab_app/         # factory HTTP (:8088)
  prebuilts/apps/discovery_app/   # LAN UDP discovery (:9712)
  app/www/
  buildroot/package/net_app/
  buildroot/package/fab_app/
  buildroot/package/discovery_app/
  device/rockchip/common/overlays/rootfs/custom/root/
  docs/
  build.sh
```

运行时入口（在 `S14_Upgrade` bind 之后）：
`S55main_app` → `/usr/bin/main_app`，
`main_app` → `app_bin`（默认 `/usr/bin/net_app`），
`S61discovery_app` → `/usr/bin/discovery_app`，
`S62fab_app` → `/usr/bin/fab_app`。
IRtek `LD_PRELOAD` 在默认业务进程脚本中设置。

## 固件裁剪

不要物理删除 SDK 源码树。通过 Buildroot 配置、包安装规则、rootfs overlay、init 或 post-rootfs 过滤，避免无关 demo / 测试 / UI / 媒体样例进入最终固件。

## 对外接口范围

必须保持 [../app/net_app/API.md](../app/net_app/API.md) 中的：配网 API、基础管理 API、产测 API、视频流查询 API。
