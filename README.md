# SmartOne SDK (RK3506B)

SmartOne 是基于 Rockchip RK3506B 的完整固件 SDK：Linux 6.1 BSP、U-Boot、Buildroot，
以及设备业务应用（`main_app` / `net_app` / `www`），可以在本地从源码编出整包固件和 OTA 包。

`fab_app`（产测）与 `discovery_app`（局域网设备发现）以预编译 ARM 二进制形式随 SDK 提供，
位于 `prebuilts/apps/<name>/`，构建时直接安装进 rootfs。

## 开发环境搭建（Ubuntu 22.04 / 24.04）

> **只有 Windows 电脑？请先看 [docs/windows-wsl-setup.md](docs/windows-wsl-setup.md)，配好 WSL2 Ubuntu 再 clone 本仓库。**
> 文档末尾附有可直接交给 AI 工具执行的配置提示词。

约定编译环境为 Ubuntu 22.04 或 24.04（ext4 文件系统，内核 ≥ 4.15，SDK 构建时会自检）。

### 1. 安装基础依赖

```shell
sudo apt update
sudo apt install -y git make gcc g++ rsync expect expect-dev \
  flex bison device-tree-compiler u-boot-tools libssl-dev \
  python3 python-is-python3 bc cpio unzip zip file wget curl lz4 \
  pkg-config libncurses-dev
```

完整依赖与分阶段说明见 [docs/build-host-deps.md](docs/build-host-deps.md)。

### 2. Python 环境

- U-Boot 编译需要 `python2`，内核编译需要 `python3`（且 `python` 指向 python3）；
- 缺失时 SDK 会在首次构建时自动调用 `device/rockchip/common/scripts/install-python*.sh`
  安装到用户目录，只需保证 `~/.local/bin` 在 PATH 中。

### 3. Node / pnpm（编译 www 前端）

二选一，构建脚本会自动识别：

- 安装到 `/opt/nodejs/current`（脚本自动加入 PATH）；
- 或用 nvm 安装（脚本自动加载 `~/.nvm`）。

### 4. clone 仓库并首次构建

```shell
cd ~
git clone https://github.com/FOTRIC-Code/SmartOne.git
cd SmartOne
./build.sh --rebuild     # 首次为全量构建，之后日常用 ./build.sh 增量
```

首次执行 `build.sh` 时会自动下载 SmartOne-SDK 底包（约 1.2 GB）
和 Buildroot 源码缓存包（约 620 MB），校验后解压到仓库目录。下载中断时重新执行同一命令会续传；
也可以单独执行 `./build.sh fetch-sdk` 重新下载并解压。

源码缓存解压在 `buildroot/dl/`，之后的构建会复用；缓存里缺少的包由 Buildroot 在线下载。
也可以把缓存放到仓库外：

```shell
export BR2_DL_DIR=/path/to/dl
```

## 仓库与发布内容

SmartOne 分三部分发布：

| 内容 | 发布方式 | 更新频率 |
|---|---|---|
| 应用源码（`app/`）、文档、示例、Developer Kit、构建脚本、板级配置 | 本 git 仓库 | 经常 |
| SmartOne-SDK 底包：kernel、u-boot、Buildroot 主体、工具链、Rockchip 官方工具 | [Releases](https://github.com/FOTRIC-Code/SmartOne/releases) 中的 `sdk-v<X.Y.Z>` | 很少 |
| 编译好的稳定固件（整包 img + tiny OTA 包） | [Releases](https://github.com/FOTRIC-Code/SmartOne/releases) 中的 `fw-v<版本>` | 定期 |

- 只想烧录固件、不需要编译时，直接下载最新的 `fw-v<版本>` release 即可。
- `build.sh` 开头的 `PARKIA_SDK_VERSION` 指定当前代码对应的 SDK 版本。`git pull` 后如果 SDK 版本变了，
  下次执行 `build.sh` 会自动删除旧版 SDK 文件并换成新版，不会动仓库里的文件。
- SDK 解压出的目录（如 `kernel-6.1/`、`buildroot/` 的大部分内容）已写进 `.gitignore`，
  `git status` 不会显示。如果要把这些目录下的某个文件纳入自己的版本管理，用 `git add -f <文件>`。
  SDK 升级时会覆盖这些文件，升级后用 `git diff` 检查，需要时 `git checkout -- <文件>` 恢复自己的改动。

## 构建脚本：build.sh

| 命令 | 用途 | 产物 |
|---|---|---|
| `./build.sh` | 增量构建：只重编 `app/*` + www 前端，重打包 rootfs/固件 | `dist/` 整包 img + tiny OTA 包 |
| `./build.sh --rebuild` | 全量构建：整编 SDK（u-boot / kernel / Buildroot rootfs / app），不删已有产物 | 同上 |
| `./build.sh clean` | 清理所有构建产物 | — |
| `./build.sh publish` | 发布打包：全量构建 + 签名 + sha256 清单 | 同上 + `sha256_V<版本>.txt` |
| `./build.sh fetch-sdk` | 重新下载、校验并解压 SmartOne-SDK 底包（缺失或版本不对时其它命令会自动执行） | — |

- 克隆后必须先 `./build.sh --rebuild` 一次，增量模式依赖已有的构建树。
- 从零构建：`./build.sh clean && ./build.sh --rebuild`。

```text
-v, --version VER       固件版本号（默认 1.0.0.0；也可用环境变量 VERSION）
--boot-medium NAME      启动介质：SPI_NAND | SD_CARD（默认 SPI_NAND；也可用 RK_BOOT_MEDIUM）
-k, --private-key F     RSA 私钥，给 net_app 生成旁挂签名文件 "sign"（不传则跳过签名）
--rebuild-third-party   强制重编 net_app 的第三方库（zlm）
-h, --help              显示帮助
```

产物：

- `dist/parkia_cube_fw_V<版本>.img`：整包固件（烧录用 update.img）；
- `dist/parkia_cube_tiny_fw_V<版本>.bin`：tiny OTA 包（main_app + net_app + discovery_app + fab_app + `lib/` + www + data.json + 可选签名）。

项目模式之外的参数原样透传给 Rockchip SDK 构建脚本：`./build.sh help`、`./build.sh kernel`、
`./build.sh menuconfig`、`./build.sh shell`、`./build.sh cleanall`。

## 开发自己的应用（Developer Kit）

不改固件、只开发用户程序时，用 `tools/developer-kit/`：

1. 先 `./build.sh --rebuild` 得到 Buildroot 交叉工具链；
2. 用 `ipconfig.exe`（discovery 协议）找到板子 IP；
3. `tools/developer-kit/build_user_app.sh` 交叉编译 ELF（示例工程 `samples/user_app/`）；
4. `tools/developer-kit/install_user_app.sh` 下载到 `/usr/bin/<app>`，更新 `app_bin` 并重启，由 `main_app` 拉起。

详见 [docs/user-app.md](docs/user-app.md)。程序无法运行时重新烧录整包固件即可恢复。

## 预编译应用

| 应用 | 位置 | 说明 |
|---|---|---|
| fab_app | `prebuilts/apps/fab_app/` | 产测服务（HTTP :8088），由 `S62fab_app` 启动 |
| discovery_app | `prebuilts/apps/discovery_app/` | 局域网 UDP 组播设备发现（239.255.255.250:9712），由 `S61discovery_app` 启动 |

这两个二进制与本 SDK 的 Buildroot 工具链和 glibc 版本绑定。修改 Buildroot 工具链、
C 库或相关 defconfig 后，预编译二进制可能无法运行。

## 安全须知（量产前必读）

- 固件默认账号口令仅用于开发（系统 root 口令为 `root`，见
  `buildroot/configs/rockchip_rk3506_board_defconfig` 的 `BR2_TARGET_GENERIC_ROOT_PASSWD`；
  Web / HTTP 管理账号默认为 `admin/admin`），**量产前必须修改**。
- SDK 中的 AVB、TEE、安全启动相关密钥（如 `tools/linux/Linux_SecurityAVB/avb_keys/`、
  `external/security/rk_tee_user/**/keys/`）均为 Rockchip 公开的测试密钥，**不能用于量产**，
  请自行生成并妥善保管。
- `./build.sh -k` 使用的 OTA 签名私钥请勿提交到仓库。

## 第三方组件

- `app/net_app/libs/zlm/`：ZLMediaKit，MIT 许可；
- `app/net_app/libs/irtek/`：fotric.cn 专有二进制 SDK，仅随本 SDK 分发；
- 其余组件（Linux、U-Boot、Buildroot 及其软件包、Rockchip BSP）遵循各自目录中的许可证。

更多说明见 [AGENTS.md](AGENTS.md) 与 [docs/](docs/)。

---

# Rockchip Linux SDK

Rockchip Linux SDK for the Rockchip SOC boards
  - wiki <http://opensource.rock-chips.com/wiki_Main_Page>.

## Quick Start

1. Check supported targets:
```shell
   ~ $ make help
```
2. Choose SDK's defconfig:
```shell
   ~ $ make rockchip_defconfig
```
3. Change SDK's configs:
```shell
   ~ $ make menuconfig
   ~ $ make savedefconfig
```
4. Run "make" to build the images, logs saved at "output/log/"
5. Flash the generated "output/firmware/update.img" to your device
6. Boot your device and enjoy it
