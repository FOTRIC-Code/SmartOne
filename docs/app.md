# Parkia.Cube 客户端对接说明

手机 App 通过 **BLE 配网**、通过 **HTTP** 访问设备上的 `net_app` 做日常管理。协议细节见 [../app/net_app/API.md](../app/net_app/API.md)；配网时序见 [flow.md](flow.md)。

## 通用约定

1. 所有 HTTP 接口响应为 JSON 包络：`{ "code", "message", "data" }`。
2. **以 `code == 0` 判定业务成功**；不要只看 HTTP 200。
3. SoftAP 默认地址：`http://192.168.4.1`（端口默认 80）。
4. 已配网后，管理类接口需 HTTP Basic；未配网时管理接口也放行（便于本地调试）。枚举均为 int。

---

## 添加设备（客户端步骤）

| 步骤 | 客户端动作 | 设备侧逻辑（摘要） |
| --- | --- | --- |
| 1 | 打开 App，扫描附近 BLE 广播 | `ble_init` 启动 GATT 服务 |
| 2 | 读取 GATT 设备信息（id/model/sn/热点名） | Write/Notify 特征可读 JSON |
| 3 | 用户选 Wi-Fi、输入密码 | — |
| 4 | 向 Write 特征写入 `{ "mode":"sta", "ssid", "password" }` | `provisioning_request_wifi` |
| 5 | 订阅 Notify，等待 `{ "status":"ok", "ip" }` 或 fail | `provisioning_poll` 异步 join |
| 6 | 手机切回家庭 Wi-Fi，用 STA IP 访问管理页 | SoftAP 宽限期后拆除 |

建议：以 BLE Notify 的 `status` 与 `ip` 为准；HTTP `GET system/status` 仅作辅助只读查询。

---

## Web 管理页（已配网，需 HTTP Basic）

浏览器打开设备 IP 即可使用 SPA 管理界面（实时预览、智能告警、TCP/IP 等）。**网页不提供 Wi-Fi 配网**；未配网时连 SoftAP 可直接打开管理页做本地调试。

---

## 设备列表状态

列表只展示两种状态：

| 状态 | 判定 |
| --- | --- |
| 在线 | 能访问设备 HTTP（如 `GET system/status` 或 `device_info` 超时内成功） |
| 离线 | HTTP 不可达或超时 |

不在列表中展示中间配网态；配网过程只在「添加设备」向导里展示。

---

## 重新配网 / 改 Wi-Fi

| 设备可达性 | 客户端做法 |
| --- | --- |
| 在线或离线 | 使用 App **蓝牙**写入 `{ "mode":"reset" }` 或重新下发 STA 凭据 |
| 网页 | 维护页仅提示使用 App 蓝牙配网，无 HTTP 重置接口 |

---

## 常用管理调用（已配网）

| 目的 | 接口 | 说明 |
| --- | --- | --- |
| 读设备信息 | `GET system/device_info` | 展示名、型号、序列号、固件版本 |
| 读网络摘要 | `GET system/status` | `network_mode` / `ssid` / `ip`（开放） |
| 改显示名 | `PUT system/device_info` | body 只改 `name` |
| 校时 | `GET/PUT system/time`、`ntp_server` | NTP 由设备 `main_process` 周期执行 |
| 升级 App 镜像 | `POST system/device_upgrade` | multipart 上传 `.img` |
| 重启 | `PUT system/device_restart` | 延迟 reboot |
| 恢复配置 | `PUT system/device_reset` | 清本地配置 |
