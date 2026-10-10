# Parkia.Cube 配网流程

实现位置：

| 环节 | 代码 |
| --- | --- |
| Boot / 主循环 | `app/net_app/main.c` |
| BLE 配网通信 | `ble/ble.c` + `ble/ble_advertiser.c`（内部） |
| Wi-Fi 执行 | `libs/provisioning/provisioning.c` |
| Wi-Fi SoftAP/STA | `wifi/wifi_manager.c` |
| LED | `led/led_manager.c` |
| 持久化 | `http_api/config/config_manager.c` → `/etc/cfg/provision/state/config.json` |
| Web 管理 | `app/www/` → `/var/www`（**不含**网页配网） |

配网仅走 **BLE**；HTTP 不提供配网 API，强制门户已关闭。

---

## 首次添加设备（端到端）

```text
设备 boot
  → config_init 读配网状态
  → ble_init（GATT 广播）
  → 未配网：SoftAP + HTTP(192.168.4.1:80) 供本地管理
App
  → 扫描 BLE，连接 GATT
  → 读设备信息 JSON
  → 用户选 Wi-Fi、输密码
  → Write: { "mode":"sta", "ssid", "password" }
  → 订阅 Notify
设备
  → provisioning_request_wifi → provisioning_poll 异步 join
  → Notify: { "status":"ok", "ip" }
  → 保存 STA 凭据（provision_state=2）+ 切 STA（network_mode=1）
App
  → 切回家用 Wi-Fi，用 STA IP 访问；写入本地设备列表
```

### 分步说明

1. **设备上电**：`config_init` 加载 HSI / provision / network 等；`ble_init` 启动蓝牙广播。
2. **未配网启动**：`handle_boot_state` → SoftAP + LED 等待；`provision_state=0`（AP_READY）。
3. **App 连 BLE**：写入 STA 凭据或 `mode=reset`。
4. **异步 join**：`provisioning_poll` 延迟执行 `net_wifi_connect`。
5. **结果回传**：`net_ble_notify_provision_result` 经 Notify 推送 ok/fail。
6. **收尾**：成功后有 SoftAP 宽限期，随后关热点；设备在家用 Wi-Fi 上可被访问。

---

## 日常开机

| 条件 | 行为 |
| --- | --- |
| 未配网（默认 AP） | SoftAP 开、BLE 广播、LED 等待、HTTP 可用 |
| 已配网且 `network_mode=0`（AP） | 保持 SoftAP，已存 STA 凭据保留 |
| 已配网且 `network_mode=1`、Wi-Fi 可达 | 重连保存的 SSID、LED online |
| 已配网且 `network_mode=1`、Wi-Fi 不可达 | LED offline；重试耗尽后回 SoftAP 待重新配网 |

---

## 复位配网

触发方式：App BLE 写入 `{ "mode":"reset" }`，或物理键（若硬件支持）。

```text
config_reset_provision   # 清 ssid 与错误
  → provisioning_enter_ap_mode
      → net_wifi_stop_station / stop_hotspot
      → net_wifi_start_hotspot
      → ap_ready + LED 等待配网
      → BLE Notify ok
```

复位后 App 重新按「首次添加」下发 Wi-Fi 凭据。

---

## HTTP 只读网络状态

管理页与 App 可用 `GET system/status` 查询 `provision_state`、`network_mode`、`ssid`、`ip`（开放，无需鉴权）。**不可**通过 HTTP 修改 Wi-Fi；切 AP/STA 走 BLE（`mode=ap` / STA 配网成功自动切 STA）。系统默认始终为 AP 模式。
