# Windows 开发环境（WSL2）

SDK 只能在 Linux（Ubuntu 22.04 / 24.04）下编译。只有 Windows 电脑时，先用 WSL2 装一个 Ubuntu 22.04，
**配好环境后再 clone 仓库**。之后编译在 WSL 里做，找板子和烧录在 Windows 上做。

可以手动按本文操作，也可以把文末的提示词交给 AI 工具（如 Cursor）代为执行。

## 0. 必须由你本人完成的步骤

下面几步 AI 无法代劳，遇到时请手动完成：

- **开启 CPU 虚拟化**：任务管理器 → 性能 → CPU，确认"虚拟化：已启用"；未启用需进 BIOS 打开（Intel VT-x / AMD SVM）。
- **管理员权限**：安装 WSL 需要"以管理员身份运行"PowerShell。
- **重启电脑**：WSL 安装完成后按提示重启。
- **设置 Linux 账号**：第一次打开 Ubuntu 时输入新的用户名和密码（与 Windows 账号无关，`sudo` 时要用）。

## 1. 环境要求

| 项目 | 要求 |
|---|---|
| 系统 | Windows 10 22H2 或 Windows 11 |
| WSL | WSL2 + Ubuntu 22.04 |
| 磁盘 | 系统盘剩余 ≥ 100 GB（首次全量构建产物很大） |
| 内存 | 建议 16 GB，给 WSL 分配 ≥ 8 GB |
| 网络 | 首次构建需访问外网，下载约 620 MB 源码包 |

## 2. 安装 WSL2 和 Ubuntu 22.04

以管理员身份打开 PowerShell：

```powershell
wsl --install -d Ubuntu-22.04
```

重启电脑后，从开始菜单打开"Ubuntu 22.04"，设置用户名和密码。确认版本为 WSL2：

```powershell
wsl -l -v      # VERSION 一列应为 2
```

已装过旧的 WSL1 发行版时，执行 `wsl --set-version Ubuntu-22.04 2`。

## 3. 给 WSL 分配资源

在 Windows 用户目录新建或编辑 `%UserProfile%\.wslconfig`：

```ini
[wsl2]
memory=8GB
processors=8
swap=8GB
```

保存后在 PowerShell 执行 `wsl --shutdown`，再重新打开 Ubuntu 生效。内存更大的电脑可以把 `memory` 调高，编译会更快。

## 4. 在 Ubuntu 里安装编译依赖

以下命令都在 **Ubuntu 终端** 中执行。

```shell
sudo apt update
sudo apt install -y git make gcc g++ rsync expect expect-dev \
  flex bison device-tree-compiler u-boot-tools libssl-dev \
  python3 python-is-python3 bc cpio unzip zip file wget curl lz4 \
  pkg-config libncurses-dev
```

再安装 Node.js 20 和 pnpm（编译 Web 前端用），方法见 [build-host-deps.md](build-host-deps.md) 的"Node.js / pnpm"一节。

## 5. clone 仓库并首次构建

```shell
cd ~
git clone https://github.com/FOTRIC-Code/SmartOne.git
cd SmartOne
./build.sh --rebuild
```

首次构建会先自动下载约 1.8GB 的 SmartOne-SDK 底包和源码缓存包（kernel、u-boot、buildroot、工具链、
烧录工具等），校验后解压到仓库目录；下载中断时重新执行同一命令即可续传。

首次全量构建需要较长时间（通常 1～2 小时，取决于网络和机器性能）。完成后 `dist/` 下会生成：

- `parkia_cube_fw_V<版本>.img`：整包固件
- `parkia_cube_tiny_fw_V<版本>.bin`：OTA 升级包

> **仓库必须放在 WSL 自己的目录（如 `~/SmartOne`），不要放在 `/mnt/c/...` 或其它 Windows 盘符下。**
> 放在 Windows 盘上编译会非常慢，并且会因为文件权限、大小写、符号链接问题而失败。

## 6. Windows 与 WSL 之间互相访问文件

- Windows 资源管理器访问 WSL 里的文件：`\\wsl$\Ubuntu-22.04\home\<Linux用户名>\SmartOne`
- 在 Ubuntu 终端执行 `explorer.exe .` 可直接用资源管理器打开当前目录
- 用 Cursor / VS Code 打开工程：在 Ubuntu 终端进入 `~/SmartOne` 后执行 `cursor .`（或 `code .`），编辑器会以 WSL 模式打开

## 7. 日常开发：哪一步在哪一侧做

| 步骤 | 在哪做 | 怎么做 |
|---|---|---|
| 编译固件 | WSL | `./build.sh`（改了代码后增量构建） |
| 编译自己的应用 | WSL | `tools/developer-kit/build_user_app.sh`，见 [user-app.md](user-app.md) |
| 找板子 IP | Windows | 运行设备搜索工具 `ipconfig.exe` |
| 下载应用到板子 | WSL | `tools/developer-kit/install_user_app.sh <板子IP> <ELF>` |
| 烧录整包固件 | Windows | RKDevTool，选择 `\\wsl$\...\SmartOne\dist\` 下的 `.img` |

说明：

- `ipconfig.exe` 是随设备提供的**设备搜索工具**，不是 Windows 自带的 `ipconfig` 网络命令。
  WSL2 默认是 NAT 网络，收不到局域网组播，所以找板子要在 Windows 上做。
- WSL 可以直接访问局域网里的板子（`ssh root@<板子IP>`），下载应用不需要额外配置。
- 烧录工具和 USB 驱动在仓库 `tools/windows/` 下（首次构建时随 SDK 下载）：先解压安装 `DriverAssitant_v5.13.zip`（Rockchip USB 驱动），
  再使用 `tools/windows/RKDevTool/` 中的 RKDevTool 烧录。可以直接从 `\\wsl$\...` 路径复制这两个目录到 Windows 本地使用。

## 8. 常见问题

| 现象 | 处理 |
|---|---|
| `wsl --install` 报"虚拟机平台未启用"或 0x80370102 | BIOS 中开启 CPU 虚拟化，并在"启用或关闭 Windows 功能"里勾选"虚拟机平台" |
| 构建时被系统杀掉（`Killed`）或卡死 | WSL 内存不足：调大 `.wslconfig` 的 `memory` / `swap` 后 `wsl --shutdown` |
| 构建非常慢、权限报错、符号链接报错 | 仓库放在了 `/mnt/c`：移到 `~/` 下重新 clone |
| 下载源码包失败 | 检查网络或代理；重新执行 `./build.sh --rebuild` 会从中断处继续 |
| 系统盘空间不足 | 清理后重试；或用 `wsl --export` / `wsl --import` 把 Ubuntu 迁移到其它盘 |

## 附：交给 AI 执行的提示词

把下面这段复制给 Cursor 等 AI 工具即可。AI 会逐步检查并执行，遇到第 0 节那些需要你本人操作的步骤时会停下来提示你。

```text
请帮我在这台 Windows 电脑上配置 SmartOne 固件 SDK 的开发环境，参考 https://github.com/FOTRIC-Code/SmartOne 上的 docs/windows-wsl-setup.md：
1. 检查 Windows 版本、CPU 虚拟化是否已开启、系统盘剩余空间（需 ≥ 100GB），不满足时告诉我怎么处理。
2. 检查是否已安装 WSL2 和 Ubuntu 22.04；没有就给出管理员 PowerShell 命令
   wsl --install -d Ubuntu-22.04，需要重启或设置 Linux 用户名密码时提示我手动完成。
3. 在 %UserProfile%\.wslconfig 中给 WSL 分配至少 8GB 内存，然后执行 wsl --shutdown 使其生效。
4. 进入 Ubuntu，安装 apt 编译依赖和 Node.js 20 + pnpm。
5. 把 https://github.com/FOTRIC-Code/SmartOne.git clone 到 ~/SmartOne（不要放在 /mnt/c），
   执行 ./build.sh --rebuild（首次会自动下载约 1.8GB 的 SDK），
   确认 dist/ 下生成了 parkia_cube_fw_V*.img。
每一步执行前先说明要做什么；失败时贴出错误并给出修复方法。
```
