# User App Developer Kit

本套 Developer Kit 面向“只做用户程序、不改固件”的开发同事。

目标：

1. 找到板子 IP
2. 用 Buildroot SDK 交叉编译出 ARM ELF
3. 把 ELF 下载到板子 `/usr/bin`
4. 必要时把 `app_bin` 更新到该 ELF 路径
5. 重启后由 `main_app` 拉起用户程序

## 1. 找板子

默认方式：

- `discovery_app` 协议
- `ipconfig.exe`

辅助方式：

- `adb`
- `ssh`

推荐流程是先用 `ipconfig.exe` 找到设备 IP，再用 `ssh root@<ip>` 连板调试。

## 2. 准备 Buildroot SDK

生成工具链包：

```sh
./build.sh shell
make sdk
```

得到 `arm-buildroot-linux-gnueabihf_sdk-buildroot.tar.gz` 后，解压并执行一次：

```sh
./relocate-sdk.sh
```

后续把该目录作为 `--sdk-root` 传给交叉编译脚本，或设置环境变量：

```sh
export BUILDROOT_SDK_ROOT=/path/to/arm-buildroot-linux-gnueabihf_sdk-buildroot
```

## 3. 交叉编译用户程序

Developer Kit 脚本位于 [`tools/developer-kit/`](../tools/developer-kit/)。

构建仓库内的最小示例：

```sh
bash tools/developer-kit/build_user_app.sh
```

显式指定 SDK：

```sh
bash tools/developer-kit/build_user_app.sh \
  --sdk-root /path/to/arm-buildroot-linux-gnueabihf_sdk-buildroot
```

默认示例产物：

```text
samples/user_app/build/user_app
```

## 4. 下载到板子

### 默认模式：自定义路径模式

`install_user_app.sh` 默认会把 ELF 安装到自定义路径，并同步更新 `app_bin`。

```sh
bash tools/developer-kit/install_user_app.sh <ip> ./samples/user_app/build/user_app
```

上面这条命令会：

1. 上传到 `/usr/bin/user_app`
2. `chmod 0755`
3. 把 `/etc/cfg/main_app/config.json` 中的 `app_bin` 改成 `/usr/bin/user_app`
4. 重启设备

### 可选模式：覆盖 `/usr/bin/net_app`

如需兼容旧流程，可显式指定远端名：

```sh
bash tools/developer-kit/install_user_app.sh <ip> ./samples/user_app/build/user_app net_app
```

此时目标路径变成 `/usr/bin/net_app`，脚本会把 `app_bin` 指回该路径。

## 5. 单独更新 `app_bin`

如只想切换启动路径，不重新上传：

```sh
bash tools/developer-kit/update_app_bin.sh <ip> /usr/bin/my_app
ssh root@<ip> reboot
```

## 6. 验证

重启后可通过以下方式确认：

```sh
ssh root@<ip> ps | grep user_app
ssh root@<ip> cat /tmp/user_app.log
```

仓库示例 `samples/user_app` 每 5 秒会向 `/tmp/user_app.log` 写一条心跳日志。

## 7. AI 工作流

Developer Kit 附带 Cursor Skill：

```text
tools/developer-kit/skills/device-app-dev/SKILL.md
```

建议把以下信息提供给 AI：

- 板子 IP（来自 `ipconfig.exe` / discovery）
- Buildroot SDK 路径
- 目标 ELF 名称
- 是否要覆盖 `/usr/bin/net_app`

Skill 默认约束 AI：

- 用 Buildroot SDK 交叉编译
- 通过 Developer Kit 脚本下载
- 不手写其它部署路径
- 不直接手改 `config.json`

## 8. 失败恢复

本方案不做运行时回退。

如果用户程序无法运行，按固件流程重新烧录整包：

```sh
sudo ./tools/linux/Linux_Upgrade_Tool/Linux_Upgrade_Tool/upgrade_tool uf \
  dist/parkia_cube_fw_V<version>.img
```
