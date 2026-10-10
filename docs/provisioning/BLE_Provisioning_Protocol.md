# Parkia.Cube BLE 配网协议说明

## 1. 适用范围

本文档定义 Parkia.Cube 设备的 BLE 配网协议。BLE 是设备默认的首配网入口，用于完成：

- 发现设备
- 读取设备信息
- 查询当前配网状态
- 扫描可用 Wi-Fi
- 下发 STA 配网参数
- 可选切换设备 AP 模式
- 清除已保存配网

设备联网成功事件会返回 `ip` / `http_port`，供客户端确认入网结果并记录设备可达信息。

## 2. 设计原则

- BLE 名称用于发现，不作为强鉴权凭据。
- 设备仅在 `commissioning_open = true` 时接受配网写入。
- 同一时间只允许一个客户端持有配网写会话；其他客户端可读取信息或收到 `busy` 错误。
- 协议优先保持简单，业务负载使用 JSON。
- `wifi_scan` 保留前向兼容字段 `band`，默认值为 `2.4g`；当前版本仅返回 2.4G 热点列表。

## 3. BLE GATT 定义

### 3.1 Service

- Name: `Provisioning Service`
- UUID: `00001234-0000-1000-8000-00805F9B34FB`

### 3.2 Characteristics

#### Command Characteristic

- UUID: `00001235-0000-1000-8000-00805F9B34FB`
- Property: `Write`, `Write Without Response`
- Direction: App -> Device
- Purpose: 下发配网命令

#### Event Characteristic

- UUID: `00001236-0000-1000-8000-00805F9B34FB`
- Property: `Notify`
- Direction: Device -> App
- Purpose: 返回执行结果、进度事件、扫描结果和失败原因

## 4. 设备发现规则

- 广播名建议格式：`SmartOne-XXXX`
- 其中 `XXXX` 建议取序列号或 MAC 尾号，便于同场区分设备
- App 发现设备时，必须同时校验：
  - 广播名称前缀为 `SmartOne`
  - 广播包含 Service UUID `00001234-0000-1000-8000-00805F9B34FB`

## 5. 通用消息格式

所有命令均通过 `1235` 写入 JSON，请求和响应均使用 UTF-8 编码。

### 5.1 请求格式

```json
{
  "op": "wifi_sta_apply",
  "req_id": 4
}
```

### 5.2 响应格式

```json
{
  "op": "wifi_sta_apply",
  "req_id": 4,
  "code": 0,
  "msg": "OK",
  "data": {}
}
```

### 5.3 通用字段说明

- `op`: 命令名称
- `req_id`: 请求 ID；客户端自增，用于幂等和匹配响应
- `code`: 结果码
- `msg`: 简要文本说明
- `data`: 业务载荷

## 6. 接口定义

### 6.1 `info`

#### 作用

读取设备基础信息和当前是否开放配网窗口。

#### 请求

```json
{
  "op": "info",
  "req_id": 1
}
```

#### 响应

```json
{
  "op": "info",
  "req_id": 1,
  "code": 0,
  "msg": "OK",
  "data": {
    "device_name": "SmartOne-8A7C",
    "device_id": "cube-001",
    "serial_no": "PCK240001",
    "model": "Parkia.Cube",
    "fw_version": "1.0.0.0",
    "provisioned": false,
    "commissioning_open": true,
    "ble_name": "SmartOne-8A7C"
  }
}
```

### 6.2 `status`

#### 作用

查询当前配网状态、网络状态、最近一次失败信息和当前 IP。

补充约定：

- `provision_state`、`network_state`、`network_mode`、`ip` 来自设备运行时
  状态机，不再依赖 `/etc/cfg/provision/state/config.json` 中的过渡态字段。
- `last_error` 仍允许从最近一次持久化失败信息回填，因此在设备重启后仍可能可读，
  直到下一次成功连接或显式清理。

#### 请求

```json
{
  "op": "status",
  "req_id": 2
}
```

#### 响应

```json
{
  "op": "status",
  "req_id": 2,
  "code": 0,
  "msg": "OK",
  "data": {
    "provision_state": "unprovisioned",
    "network_state": "offline",
    "network_mode": "sta",
    "stage": "idle",
    "saved_ssid": "",
    "ip": "",
    "last_error": {
      "code": 0,
      "reason": "none"
    }
  }
}
```

### 6.3 `wifi_scan`

#### 作用

扫描设备当前支持的热点列表。当前版本只返回 2.4G AP。

#### 请求

```json
{
  "op": "wifi_scan",
  "req_id": 3,
  "band": "2.4g"
}
```

#### 请求约束

- `band` 为可选字段
- `band` 默认值为 `2.4g`
- 当前版本允许值：`2.4g`, `5g`, `all`
- 即使传入 `5g` 或 `all`，当前设备仍只返回 2.4G 列表
- 客户端不得据此假设当前版本支持 5G 入网

#### 响应

```json
{
  "op": "wifi_scan",
  "req_id": 3,
  "code": 0,
  "msg": "OK",
  "data": {
    "list": [
      {
        "ssid": "fotric-test",
        "rssi": -55,
        "security": "wpa2_psk",
        "channel": 6,
        "band": "2.4g"
      }
    ],
    "done": true
  }
}
```

### 6.4 `wifi_sta_apply`

#### 作用

下发目标 Wi-Fi STA 参数并触发设备入网。

#### 请求

```json
{
  "op": "wifi_sta_apply",
  "req_id": 4,
  "ssid": "fotric-test",
  "pwd": "test@831598",
  "hidden": false
}
```

#### 高级可选字段

- `bssid`
- `security`

普通 App 可不传这些字段。

#### 受理通知（当前实现保证）

```json
{
  "op": "wifi_sta_apply",
  "req_id": 4,
  "code": 0,
  "msg": "Accepted",
  "data": {
    "stage": "queued"
  }
}
```

- 当前固件首版保证返回 `queued` 和最终成功/失败结果。
- 在 STA 入网实际进行期间，客户端应继续读取 `status` 获取阶段与最近错误。
- 重配网失败时，设备保留最后一次已知可用配置，并优先恢复上一次成功模式：
  之前是 `STA` 就回保存的成功 STA，之前是 `AP` 就回原 AP。

#### 成功响应

```json
{
  "op": "wifi_sta_apply",
  "req_id": 4,
  "code": 0,
  "msg": "Provisioned",
  "data": {
    "stage": "done",
    "ip": "192.168.1.181",
    "http_port": 80
  }
}
```

#### 失败响应

```json
{
  "op": "wifi_sta_apply",
  "req_id": 4,
  "code": 300101,
  "msg": "wifi_connect_failed",
  "data": {
    "stage": "connecting"
  }
}
```

### 6.5 `wifi_ap_apply`

#### 作用

切换设备到 AP 模式，或修改设备 AP 参数。该接口用于高级配置或维护模式。

#### 请求

```json
{
  "op": "wifi_ap_apply",
  "req_id": 5,
  "ssid": "SmartOne-8A7C",
  "pwd": "12345678",
  "channel": 6
}
```

#### 响应

```json
{
  "op": "wifi_ap_apply",
  "req_id": 5,
  "code": 0,
  "msg": "OK",
  "data": {
    "network_mode": "ap",
    "ip": "192.168.4.1"
  }
}
```

### 6.6 `reset_provision`

#### 作用

清空保存的 STA 配置并让设备重新进入待配网状态。

#### 请求

```json
{
  "op": "reset_provision",
  "req_id": 6
}
```

#### 响应

```json
{
  "op": "reset_provision",
  "req_id": 6,
  "code": 0,
  "msg": "Provision reset",
  "data": {
    "provision_state": "unprovisioned",
    "commissioning_open": true
  }
}
```

## 7. 错误码

### 7.1 通用错误码

- `0`: 成功
- `-4`: 参数非法
- `-409`: 设备忙，已被其他客户端占用
- `-413`: 数据过长

### 7.2 业务错误码

- `300101`: Wi-Fi 连接失败
- `300102`: Wi-Fi 扫描失败
- `300103`: 设备 AP 启动失败
- `300104`: 当前状态不允许执行请求（如 `commissioning_open = false`）

## 8. 硬件重置按钮（RECOVERY）

### 8.1 概述

设备配网成功后，BLE 广播会被完全停止（释放 AIC8800DC combo 芯片的射频资源给
Wi-Fi），此时无法通过 BLE 重新配网。如需重新配网，用户须通过 **RECOVERY 物理按钮**
触发工厂重置。

### 8.2 硬件连接

- 按钮连接到 RK3506 的 **SARADC IN1**（通道 1）
- Vref = 1.8V，按下时 ADC 读数接近 0V，松开时接近 1.8V
- 软件通过 IIO sysfs（`/sys/bus/iio/devices/iio:deviceN/in_voltage1_raw`）读取

### 8.3 触发条件

- **长按 ≥ 3 秒**触发重置
- 短按无效，防止误触
- 触发后须松开按钮才能再次触发

### 8.4 重置流程

```
长按 RECOVERY ≥ 3s
  ├── 1. 断开 Wi-Fi（wpa_cli disconnect）
  ├── 2. 清除配网状态（provision_state → unprovisioned，清空 SSID/密码）
  ├── 3. LED 切换到慢闪（WAIT_PROVISION：1s 亮 / 1s 灭）
  └── 4. 重启 BLE 广播（重新启动 bluetoothd + hci0）
```

重置完成后，设备回到初始待配网状态，App 可重新发现并配网。

### 8.5 LED 指示

| 阶段 | LED 状态 | 说明 |
|------|---------|------|
| 按钮按下中 | 无变化 | 等待满足 3 秒阈值 |
| 重置触发 | 慢闪（1s 亮/1s 灭） | 进入待配网状态 |
| 重新配网成功 | 常亮 | 设备在线 |

## 9. 行为约束

- 设备只有在 `commissioning_open = true` 时才接受写入型命令
- 已配网设备默认不开放改 Wi-Fi 操作
- `reset_provision` 成功后，设备应重新进入 BLE 待配网状态；不会再自动拉起 SoftAP
- `wifi_sta_apply` 失败时，不应清空最后一次已知可用配置
- `wifi_sta_apply` 失败时，应优先恢复上一次成功模式，而不是无条件退回 AP
- 同一时刻只允许一个客户端执行写操作
- 已配网设备的 BLE 广播已停止；重新配网须通过 RECOVERY 按钮长按 ≥ 3 秒触发硬件重置

## 10. 与当前代码实现的对应模块

- `info` / `status` / `wifi_scan` / `wifi_sta_apply` / `wifi_ap_apply` /
  `reset_provision` 的 BLE 命令入口都在 `ble/ble_advertiser.c`
- 配网状态机与 Wi-Fi 副作用在 `ble_provision.c`
- RECOVERY 按钮检测在 `ble_button.c`（SARADC 轮询 + 长按检测）
- 在线后的 AP/STA 模式管理仍可通过 `GET/PUT system/network_mode` 与
  `GET/PUT system/provision_hotspot` 观察或切换

## 11. 客户端接入要求

- App 扫描设备时，应按 Service UUID 过滤
- 首次连接后，应先订阅 `1236`
- App 调用顺序建议为：`info` -> `status` -> `wifi_scan` -> `wifi_sta_apply`
- 配网成功后，App 切换到设备局域网 IP，通过 HTTP 完成后续管理
