# Parkia.Cube Provisioning Docs

本目录收敛为 BLE 配网相关文档，分为两类：

## 对外发布文档

- [`BLE_Provisioning_Protocol.md`](./BLE_Provisioning_Protocol.md)
  - 面向 App、SDK、联调方
  - 说明 BLE 配网协议、GATT UUID、接口请求/响应和错误码

- [`BLE_Provisioning_Field_Definitions.md`](./BLE_Provisioning_Field_Definitions.md)
  - 面向 App、SDK、测试
  - 说明字段定义、枚举值、默认值和约束

- [`BLE_Provisioning_Sequence.md`](./BLE_Provisioning_Sequence.md)
  - 面向 App、SDK、测试、售后
  - 说明首次配网、重配网、失败恢复和 AP 模式切换时序

## 当前协议边界

- BLE 是默认首配网入口
- 本目录文档只描述 BLE 配网命令、字段和时序
- `wifi_scan` 保留可选字段 `band`，默认 `2.4g`
- 当前协议版本中，`wifi_scan` 实际只返回 2.4G 热点列表

## 配套测试工具

- 主机侧 CLI：`smart/Parkia.Cube/app/net_app/tests/ble_provision_test.py`
- 示例脚本场景：`smart/Parkia.Cube/app/net_app/tests/ble_smoke_scenario.json`
- 板端诊断脚本：`smart/Parkia.Cube/app/net_app/tests/debug_sta_net.sh`

---

## 闭环测试路径

> 以下所有测试均使用 BLE-first（默认）模式。
> BLE 端操作使用 `ble_provision_test.py` 或 nRF Connect 等 BLE 调试工具。
> 板端验证命令通过 USB-RNDIS (`192.168.123.100`) 或串口登录。

### 通用前置条件

| 项目 | 说明 |
|------|------|
| 固件 | 已刷入最新编译的 `parkia_cube_fw_V*.img` |
| BLE 工具 | 手机安装 nRF Connect / 自研 App，或 PC 运行 `ble_provision_test.py` |
| 路由器 | 2.4GHz Wi-Fi 开启，SSID/密码已知 |
| 板端访问 | USB 线连接，可通过 `ssh root@192.168.123.100` 或串口访问 |

---

### A. 首次配网成功 → STA 模式可用

**前提**：设备出厂状态（未配网）或已执行 `reset_provision`。

#### 操作步骤

1. **上电启动**，等待 10 秒让 BLE 广播就绪
2. **BLE 连接**：扫描并连接 `ParkiaCube-XXXX` 设备
3. **获取设备信息**：
   ```json
   → {"op":"info","req_id":1}
   ← {"op":"info","code":0,"msg":"OK","data":{"provisioned":false,"commissioning_open":true,...}}
   ```
4. **扫描 Wi-Fi**：
   ```json
   → {"op":"wifi_scan","req_id":2}
   ← (多条 AP 结果，最后一条 done=true)
   ```
5. **下发配网**：
   ```json
   → {"op":"wifi_sta_apply","req_id":3,"ssid":"YOUR_SSID","pwd":"YOUR_PASSWORD","security":"wpa2_psk"}
   ← {"op":"wifi_sta_apply","req_id":3,"code":0,"msg":"Accepted","data":{"stage":"queued"}}
   ```
6. **等待结果**（约 10-40 秒，BLE notify 推送进度）：
   ```json
   ← {"op":"wifi_sta_apply","req_id":3,"code":0,"msg":"OK","data":{"stage":"preparing"}}
   ← {"op":"wifi_sta_apply","req_id":3,"code":0,"msg":"OK","data":{"stage":"connecting"}}
   ← {"op":"wifi_sta_apply","req_id":3,"code":0,"msg":"Provisioned","data":{"stage":"done","ip":"192.168.x.x","http_port":80}}
   ```

#### 预期结果

- BLE 返回 `stage=done`，含有效 IP 地址
- 设备 LED 切换为在线状态
- 设备可访问互联网

#### 板端验证命令

```bash
# 1. 确认 wlan0 有 IP
ip -4 addr show wlan0

# 2. 确认 p2p0 存在但 DOWN（不应被删除）
ip link show p2p0 2>&1        # 应存在，state DOWN
ip link show wlan1 2>&1       # 应不存在

# 3. 确认路由和 DNS
ip route show default          # 应有 via x.x.x.x dev wlan0
cat /etc/resolv.conf           # 应有 nameserver

# 4. 确认网关可达
GW=$(ip route show default | awk '{print $3; exit}')
ping -I wlan0 -c 4 -W 2 "$GW"        # 应全部通
arping -I wlan0 -c 2 "$GW"           # 应有回复

# 5. 确认互联网可达
ping -c 4 -W 3 223.5.5.5             # 应全部通

# 6. 确认 wpa_supplicant 状态正常
wpa_cli -i wlan0 status               # wpa_state=COMPLETED

# 7. 确认没有 SoftAP 残留
pidof hostapd                          # 应无输出
ip -4 addr show | grep 192.168.4      # 应无匹配

# 8. 完整诊断（可选）
sh /tmp/debug_sta_net.sh 2>&1 | tee /tmp/sta-net.log
```

---

### B. 配网失败后重试

**前提**：设备未配网（或已执行 `reset_provision`）。

#### 操作步骤

1. **BLE 连接**设备
2. **下发错误密码配网**：
   ```json
   → {"op":"wifi_sta_apply","req_id":10,"ssid":"YOUR_SSID","pwd":"wrong_password","security":"wpa2_psk"}
   ```
3. **等待失败结果**（约 40 秒超时）：
   ```json
   ← {"op":"wifi_sta_apply","req_id":10,"code":300101,"msg":"wifi_connect_failed","data":{"stage":"failed"}}
   ```
4. **查询状态确认失败**：
   ```json
   → {"op":"status","req_id":11}
   ← {"data":{"provision_state":"unprovisioned","network_state":"offline",...}}
   ```
5. **下发正确密码重试**：
   ```json
   → {"op":"wifi_sta_apply","req_id":12,"ssid":"YOUR_SSID","pwd":"CORRECT_PASSWORD","security":"wpa2_psk"}
   ```
6. **等待成功结果**

#### 预期结果

- 第一次尝试返回 `code=300101`，`stage=failed`
- BLE 连接保持不断，可以直接重试
- 第二次尝试使用正确密码后返回 `stage=done` + 有效 IP
- LED 先闪烁（配网中）→ 失败闪烁 → 再次配网中 → 在线常亮

#### 板端验证命令

```bash
# 失败后：
wpa_cli -i wlan0 status                # wpa_state 可能是 DISCONNECTED 或 SCANNING
ip -4 addr show wlan0                  # 应无 IP
cat /etc/cfg/provision/state/config.json  # provision_state != "provisioned"

# 成功后：同路径 A 的板端验证命令
```

---

### C. 已配网设备重启恢复

**前提**：设备已完成路径 A 配网成功。

#### 操作步骤

1. **重启设备**：
   ```bash
   reboot
   ```
2. **等待 20-30 秒**让设备完成 boot STA restore
3. **验证网络恢复**

#### 预期结果

- 设备启动后自动连接保存的 Wi-Fi
- 无需 BLE 交互即可恢复在线
- LED 显示在线状态
- boot STA restore 最多重试 6 次（每次间隔 5 秒）

#### 板端验证命令

```bash
# 等待启动完成后：
ip -4 addr show wlan0                   # 应有 IP
ping -c 4 -W 2 $(ip route show default | awk '{print $3; exit}')   # 应通

# 检查配网状态持久化
cat /etc/cfg/provision/state/config.json
# 应显示: provision_state=2 (PROVISIONED), network_state=1 (ONLINE), ssid 非空

# 确认 p2p0 down 但存在
ip link show p2p0 2>&1                  # 存在，state DOWN
```

#### BLE 端验证

```json
→ 连接设备后
→ {"op":"status","req_id":20}
← {"data":{"provision_state":"provisioned","network_state":"online","ip":"192.168.x.x",...}}
```

---

### D. 已配网设备重新配网（切换 Wi-Fi）

**前提**：设备已完成路径 A 配网成功，现需切换到另一个 Wi-Fi。

#### 操作步骤

1. **BLE 连接**设备（已配网设备 BLE 仍在广播）
2. **确认 commissioning_open**：
   ```json
   → {"op":"info","req_id":30}
   ← {"data":{"provisioned":true,"commissioning_open":true,...}}
   ```
   > 注：BLE 连接时自动打开 commissioning window
3. **下发新 Wi-Fi 配网**：
   ```json
   → {"op":"wifi_sta_apply","req_id":31,"ssid":"NEW_SSID","pwd":"NEW_PASSWORD","security":"wpa2_psk"}
   ```
4. **等待成功结果**：
   ```json
   ← {"op":"wifi_sta_apply","req_id":31,"code":0,"msg":"Provisioned","data":{"stage":"done","ip":"10.0.x.x","http_port":80}}
   ```

#### 预期结果

- 旧 Wi-Fi 连接断开
- 新 Wi-Fi 连接成功，获取新 IP
- 持久化配置更新为新 SSID/密码
- 重启后自动连接新 Wi-Fi

#### 板端验证命令

```bash
# 确认新 Wi-Fi 连接
wpa_cli -i wlan0 status               # ssid 应为 NEW_SSID
ip -4 addr show wlan0                 # 应有新网段 IP
ping -c 4 -W 2 $(ip route show default | awk '{print $3; exit}')

# 确认持久化
cat /etc/cfg/provision/state/config.json
# ssid 字段应为 NEW_SSID

# 重启后验证自动恢复
reboot
# 等 30 秒后：
ip -4 addr show wlan0                 # 应有 IP
wpa_cli -i wlan0 status              # ssid=NEW_SSID, wpa_state=COMPLETED
```

---

### E. reset_provision 复位

**前提**：设备已配网（任何状态均可）。

#### 操作步骤

1. **BLE 连接**设备
2. **执行复位**：
   ```json
   → {"op":"reset_provision","req_id":40}
   ← {"op":"reset_provision","code":0,"msg":"Provision reset","data":{"provision_state":"unprovisioned","commissioning_open":true}}
   ```
3. **确认状态**：
   ```json
   → {"op":"status","req_id":41}
   ← {"data":{"provision_state":"unprovisioned","network_state":"offline","saved_ssid":"",...}}
   ```

#### 预期结果

- Wi-Fi 连接断开
- 保存的 SSID/密码被清除
- 设备回到未配网状态（BLE commissioning ready）
- LED 切换为等待配网状态
- 可以从路径 A 重新开始配网
- **BLE 连接不断** — 不会因为复位而强制断开 BLE

#### 板端验证命令

```bash
# 确认 Wi-Fi 断开
ip -4 addr show wlan0                 # 应无 IP（或仅 link-local）
pidof wpa_supplicant                  # 应无（或 STA 被 stop）

# 确认配置已清除
cat /etc/cfg/provision/state/config.json
# provision_state 应为 0 或 1（未配网/AP_READY）
# ssid 应为空

# 确认 SoftAP 未启动（BLE-first 模式不启动 SoftAP）
pidof hostapd                          # 应无输出

# 确认 p2p0 仍存在
ip link show p2p0 2>&1                # 应存在

# 确认 BLE 仍在广播
# 用 BLE 工具重新扫描，设备名 ParkiaCube-XXXX 应可发现
```

#### 复位后重新配网验证

```json
→ {"op":"wifi_sta_apply","req_id":42,"ssid":"YOUR_SSID","pwd":"YOUR_PASSWORD","security":"wpa2_psk"}
← (等待) {"op":"wifi_sta_apply","req_id":42,"code":0,"msg":"Provisioned","data":{"stage":"done","ip":"192.168.x.x"}}
```

---

## 故障排查速查

| 现象 | 可能原因 | 排查命令 |
|------|---------|---------|
| BLE 扫不到设备 | hci0 未就绪 / 广播未启动 | `hciconfig hci0`; `bluetoothctl show` |
| wifi_scan 返回空 | wlan0 未就绪 | `ip link show wlan0`; `wpa_cli -i wlan0 ping` |
| wifi_sta_apply 返回 busy | 上次配网还在进行 | 等待完成或 `reset_provision` |
| stage=done 但 ping 不通 | p2p0 被误删 / 数据面异常 | `ip link show p2p0`; `sh debug_sta_net.sh` |
| 重启后无法恢复 | 持久化失败 | `cat /etc/cfg/provision/state/config.json` |
| 重配网失败 | commissioning 窗口关闭 | 断开 BLE 重连，或 `reset_provision` |
