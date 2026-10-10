# Developer Kit

用于用户程序开发的宿主机脚本与 Skill。

## 目录

```text
tools/developer-kit/
  build_user_app.sh
  install_user_app.sh
  update_app_bin.sh
  linux/
  mac/
  windows/
  skills/
```

## 核心脚本

- `build_user_app.sh`：调用 Buildroot SDK 交叉编译用户程序
- `install_user_app.sh`：上传 ELF、更新 `app_bin`、重启设备
- `update_app_bin.sh`：只更新 `app_bin`

## 默认行为

`install_user_app.sh` 默认采用“自定义路径模式”：

- 本地 `user_app` 默认上传到 `/usr/bin/user_app`
- 脚本同步更新 `/etc/cfg/main_app/config.json` 的 `app_bin`
- 然后重启设备

如需兼容旧流程，可把远端名指定为 `net_app`。

## 平台入口

- Linux: `linux/*.sh`
- macOS: `mac/*.command`
- Windows: `windows/*.cmd` / `windows/*.ps1`

## 详细文档

见 [`docs/user-app.md`](../../docs/user-app.md)。
