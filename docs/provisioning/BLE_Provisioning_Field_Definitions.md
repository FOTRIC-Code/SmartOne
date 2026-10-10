# Parkia.Cube BLE 配网字段定义表

## 1. 文档说明

本文档定义 Parkia.Cube BLE 配网协议的字段、类型、枚举值和约束，并对字段设计做简要审查说明。

## 2. 公共字段

### `op`

- 类型：`string`
- 作用：命令名称或响应归属名称
- 是否必填：是
- 示例：`info`
- 审查说明：必须保留，用于多种通知混用时做命令分派

### `req_id`

- 类型：`int`
- 作用：请求 ID，用于幂等和匹配响应
- 是否必填：是
- 约束：客户端侧单连接内递增
- 示例：`4`
- 审查说明：简单协议中最值得保留的一个控制字段

### `code`

- 类型：`int`
- 作用：结果码
- 是否必填：是
- 典型值：
  - `0`：成功
  - `-4`：参数非法
  - `-401`：不允许执行
  - `-409`：设备忙
  - `-413`：数据过长
  - `300101`：Wi-Fi 连接失败
  - `300102`：Wi-Fi 扫描失败
  - `300103`：设备 AP 启动失败
  - `300104`：当前状态不允许
- 审查说明：必须统一，避免每个接口返回不同风格

### `msg`

- 类型：`string`
- 作用：简要文本说明
- 是否必填：建议是
- 示例：`OK`
- 审查说明：保留，但不能只靠 `msg` 判断业务结果，必须以 `code` 为准

### `data`

- 类型：`object`
- 作用：业务字段承载体
- 是否必填：有业务数据时是
- 审查说明：保留，避免顶层字段膨胀

## 3. `info` 接口字段

### 请求

#### `op`

- 固定值：`info`
- 含义：读取设备信息

### 响应 `data`

#### `device_name`

- 类型：`string`
- 含义：设备展示名称
- 示例：`SmartOne-8A7C`
- 审查说明：必须保留，App 列表直接展示

#### `device_id`

- 类型：`string`
- 含义：设备内部唯一标识
- 示例：`cube-001`
- 审查说明：可保留，但文档必须说明它偏内部语义

#### `serial_no`

- 类型：`string`
- 含义：设备序列号
- 示例：`PCK240001`
- 审查说明：保留，售后和生产会用到

#### `model`

- 类型：`string`
- 含义：设备型号
- 示例：`Parkia.Cube`
- 审查说明：保留，客户端需区分设备族

#### `fw_version`

- 类型：`string`
- 含义：当前固件版本
- 示例：`1.0.0.0`
- 审查说明：保留，用于兼容性判断

#### `provisioned`

- 类型：`bool`
- 含义：是否已经完成配网
- 枚举：
  - `true`：已配网
  - `false`：未配网
- 审查说明：必须保留

#### `commissioning_open`

- 类型：`bool`
- 含义：当前是否允许配网写入
- 枚举：
  - `true`：允许写入配网命令
  - `false`：仅允许读取信息/状态
- 审查说明：这是极简安全模型的关键字段，必须保留

#### `ble_name`

- 类型：`string`
- 含义：当前 BLE 广播名
- 示例：`SmartOne-8A7C`
- 审查说明：建议保留，避免设备名和广播名分离时客户端误判

## 4. `status` 接口字段

### 请求

#### `op`

- 固定值：`status`
- 含义：查询当前状态

### 响应 `data`

#### `provision_state`

- 类型：`string(enum)`
- 含义：配网业务生命周期状态；由运行时状态机推导，不直接回显磁盘中的旧字段
- 枚举：
  - `unprovisioned`：未配网
  - `provisioning`：配网进行中
  - `provisioned`：已配网
  - `ap_ready`：当前停在设备热点等待下一次配网
- 审查说明：保留，用来看业务阶段

#### `network_state`

- 类型：`string(enum)`
- 含义：当前运行链路状态；由运行时状态机推导
- 枚举：
  - `offline`：当前未建立 STA 在线链路；AP 模式也表现为 `offline`
  - `online`：当前 STA 已联网
- 审查说明：保留，用来看链路状态

#### `network_mode`

- 类型：`string(enum)`
- 含义：当前运行模式；`status` / `info` 读到的是运行态，不是磁盘中的过渡态
- 枚举：
  - `sta`：家庭 Wi-Fi 模式
  - `ap`：设备热点模式
- 审查说明：保留，用来看设备当前对外工作的模式

#### `stage`

- 类型：`string(enum)`
- 含义：更细的处理步骤
- 枚举：
  - `idle`：空闲
  - `queued`：请求已受理，等待主循环执行
  - `scanning`：扫描热点中
  - `connecting`：连接 Wi-Fi 中
  - `done`：完成
  - `failed`：失败
  - `resetting`：重置中
- 审查说明：保留，便于前端展示和问题定位；当前固件首版保证 `queued`
  和最终状态，中间细粒度阶段应通过 `status` 读取

#### `saved_ssid`

- 类型：`string`
- 含义：已保存的热点名称
- 审查说明：可保留，但对外开放时要注意是否暴露过多用户环境信息

#### `ip`

- 类型：`string`
- 含义：当前运行态 IP；`sta` 模式下返回 STA IP，`ap` 模式下返回设备热点侧
  HTTP 地址
- 示例：`192.168.1.181`
- 审查说明：保留，App 成功后要切 HTTP 管理

#### `last_error`

- 类型：`object`
- 含义：最近一次失败信息；可在必要时持久化，便于设备重启后继续诊断
- 审查说明：保留

##### `last_error.code`

- 类型：`int`
- 含义：最近一次错误码

##### `last_error.reason`

- 类型：`string(enum)`
- 含义：最近一次错误原因
- 枚举：
  - `none`
  - `wifi_scan_failed`
  - `wifi_connect_failed`
  - `dhcp_failed`
  - `busy`
  - `not_allowed`

## 5. `wifi_scan` 接口字段

### 请求

#### `op`

- 固定值：`wifi_scan`
- 含义：发起扫描

#### `band`

- 类型：`string(enum)`
- 含义：期望扫描的频段
- 是否必填：否
- 默认值：`2.4g`
- 预留枚举：
  - `2.4g`：2.4G
  - `5g`：5G
  - `all`：全部频段
- 当前版本行为：无论不传、传 `2.4g`、`5g` 还是 `all`，当前设备仍只返回 2.4G 列表
- 审查说明：这个字段保留是为了前向兼容，但文档必须明确当前版本不支持 5G 扫描结果

### 响应 `data`

#### `list`

- 类型：`array<object>`
- 含义：扫描结果数组
- 审查说明：保留

##### `ssid`

- 类型：`string`
- 含义：热点名称
- 说明：隐藏热点可为空字符串

##### `rssi`

- 类型：`int`
- 含义：信号强度
- 单位：dBm
- 示例：`-55`

##### `security`

- 类型：`string(enum)`
- 含义：热点安全类型
- 枚举：
  - `open`
  - `wpa_psk`
  - `wpa2_psk`
  - `wpa3_sae`
  - `wpa2_wpa3_mixed`
- 审查说明：必须保留，否则客户端无法判断是否需要密码

##### `channel`

- 类型：`int`
- 含义：热点信道
- 2.4G 范围：`1` 到 `13`
- 审查说明：建议保留，便于调试；普通前端可以不展示

##### `band`

- 类型：`string(enum)`
- 含义：热点所在频段
- 枚举：
  - `2.4g`
  - `5g`
- 当前版本约束：当前响应只会返回 `2.4g`
- 审查说明：建议保留，用于明确能力边界

#### `done`

- 类型：`bool`
- 含义：当前扫描结果是否已经发送完成
- 枚举：
  - `true`：结果已结束
  - `false`：后续仍有批次
- 审查说明：建议保留，方便未来扩展为分批通知

## 6. `wifi_sta_apply` 接口字段

### 请求

#### `op`

- 固定值：`wifi_sta_apply`
- 含义：提交 STA 配网参数

#### `ssid`

- 类型：`string`
- 含义：目标家庭 Wi-Fi 名称
- 是否必填：是
- 审查说明：必须保留

#### `pwd`

- 类型：`string`
- 含义：目标家庭 Wi-Fi 密码
- 是否必填：通常是
- 说明：开放网络时可为空字符串
- 审查说明：已统一为 `pwd`（不再接受 `password`）

#### `hidden`

- 类型：`bool`
- 含义：是否为隐藏网络
- 是否必填：否
- 默认值：`false`
- 审查说明：建议保留，真实场景有用，但不应成为首版复杂度来源

#### `bssid`

- 类型：`string`
- 含义：目标 AP 的 MAC 地址
- 是否必填：否
- 示例：`AA:BB:CC:DD:EE:FF`
- 审查说明：高级字段，普通 App 可不使用

#### `security`

- 类型：`string(enum)`
- 含义：目标 AP 的安全类型
- 是否必填：否
- 推荐默认值：`auto`
- 枚举：
  - `auto`
  - `open`
  - `wpa_psk`
  - `wpa2_psk`
  - `wpa3_sae`
  - `wpa2_wpa3_mixed`
- 审查说明：首版可不暴露给普通 App，避免填错导致失败

### 响应 `data`

#### `stage`

- 类型：`string(enum)`
- 含义：当前入网阶段
- 枚举：
  - `connecting`
  - `dhcp`
  - `done`
  - `failed`

#### `ip`

- 类型：`string`
- 含义：入网成功后设备 STA IP
- 仅在成功时出现

#### `http_port`

- 类型：`int`
- 含义：入网成功后 HTTP 管理端口
- 仅在成功时出现
- 审查说明：可以保留，因为它是成功后客户端切换管理接口需要的信息

## 7. `wifi_ap_apply` 接口字段

### 请求

#### `op`

- 固定值：`wifi_ap_apply`
- 含义：切换设备到 AP 模式或设置 AP 参数

#### `ssid`

- 类型：`string`
- 含义：设备 AP 名称
- 是否必填：是
- 约束：建议 1 到 32 字节

#### `pwd`

- 类型：`string`
- 含义：设备 AP 密码
- 是否必填：否
- 约束：
  - 空字符串：开放热点
  - 非空：8 到 63 字符

#### `channel`

- 类型：`int`
- 含义：设备 AP 信道
- 是否必填：否
- 枚举范围：`1` 到 `13`
- 审查说明：高级字段；可对普通用户隐藏

### 响应 `data`

#### `network_mode`

- 类型：`string(enum)`
- 含义：当前模式
- 枚举：
  - `ap`
  - `sta`

#### `ip`

- 类型：`string`
- 含义：设备 AP IP 地址
- 示例：`192.168.4.1`

## 8. `reset_provision` 接口字段

### 请求

#### `op`

- 固定值：`reset_provision`
- 含义：清除已保存配网并重新开放待配网状态

### 响应 `data`

#### `provision_state`

- 类型：`string(enum)`
- 含义：重置后的配网状态
- 典型值：
  - `unprovisioned`

#### `commissioning_open`

- 类型：`bool`
- 含义：重置完成后是否立即开放配网窗口
- 审查说明：必须保留，否则客户端无法确定下一步能否继续配网

## 9. 字段设计审查结论

### 建议保留的核心字段

- `op`
- `req_id`
- `code`
- `msg`
- `device_name`
- `serial_no`
- `provisioned`
- `commissioning_open`
- `provision_state`
- `network_state`
- `network_mode`
- `stage`
- `ssid`
- `pwd`
- `rssi`
- `security`
- `ip`

### 建议作为高级字段的内容

- `device_id`
- `saved_ssid`
- `channel`
- `band`
- `hidden`
- `bssid`

### 不建议延续到 BLE 配网主文档的字段

- `session_token`
- `authenticated`
- `http_host`
- `hotspot_ssid` 作为发现字段
- 任何仅属于旧 SoftAP/HTTP 配网时代的中间态字段
